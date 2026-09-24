#include "forward.h"

#include <framework/tools/replay/ReplayController.h>
#include <framework/tools/comparison/ComparisonController.h>

#include <donut/core/json.h>
#include <donut/core/log.h>
#include <nvrhi/utils.h>

#include <imgui.h>

#include <array>

namespace dm = donut::math;
using namespace donut::math;

// 共享常量布局：需要 donut 的数学类型在全局可见（与 Donut 自己的 shared header 用法一致）
#include "debug_view_cb.h"

static_assert(sizeof(DebugViewConstants) == 96, "DebugViewConstants layout changed; update debug_view.hlsl");

namespace prism::experiments
{
    namespace
    {
        constexpr uint32_t kDepthConventionForwardZ = 0;

        const char* const kDebugModeNames[] = {
            "Off",
            "Device depth",
            "Linear depth",
            "World position",
            "Normal from depth",
        };
    }

    const char* ForwardExperiment::GetDescription() const
    {
        return "Shared forward scene path plus a debug view pass written by the experiment (depth "
               "decode, world position reconstruction and depth-derived normals).";
    }

    Status ForwardExperiment::Initialize(host::ExperimentContext& context)
    {
        if (!context.gpu.device || !context.gpu.targets || !context.gpu.shaders)
            return Status::Error(ErrorCode::NotInitialized, "the host context is incomplete");

        const host::HostConfig defaults;
        const auto& config = context.config ? *context.config : defaults;
        auto sceneStatus = m_Scene.Initialize(context.gpu, config.scene, config.lighting);
        if (!sceneStatus) return sceneStatus;
        context.scene.stats = m_Scene.Data().stats;
        context.scene.description = m_Scene.Data().description;

        // 读取本实验自己的配置段（samples/forward/config.json 的 experiments.ForwardExperiment）
        if (context.config)
        {
            Json::Value settings;
            if (host::LoadExperimentSettings(*context.config, GetName(), settings))
            {
                settings["debugMode"] >> m_Settings.debugMode;
                settings["depthScale"] >> m_Settings.depthScale;
            }
        }

        m_ColorRequest.name = "SceneColor";
        m_ColorRequest.format = PixelFormat::RGBA16_FLOAT;
        m_ColorRequest.usage = gpu::TextureUsage::ShaderResource | gpu::TextureUsage::RenderTarget;
        m_ColorRequest.clearColor = dm::float4(0.04f, 0.05f, 0.07f, 1.f);

        m_DepthRequest.name = "SceneDepth";
        m_DepthRequest.format = PixelFormat::D32_FLOAT;
        m_DepthRequest.usage = gpu::TextureUsage::ShaderResource | gpu::TextureUsage::DepthStencil;
        m_DepthRequest.clearDepth = kDepthClearValue;

        m_DebugRequest.name = "DebugColor";
        m_DebugRequest.format = PixelFormat::RGBA16_FLOAT;
        m_DebugRequest.usage = gpu::TextureUsage::ShaderResource | gpu::TextureUsage::RenderTarget;
        m_DebugRequest.clearColor = dm::float4(0.02f, 0.02f, 0.03f, 1.f);

        if (!context.gpu.targets->GetOrCreate(m_ColorRequest) || !context.gpu.targets->GetOrCreate(m_DepthRequest))
            return Status::Error(ErrorCode::DeviceError, "failed to create the scene render targets");

        m_DebugConstantBuffer = context.gpu.device->createBuffer(
            nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(DebugViewConstants), "ForwardExperimentDebugView", 4));

        if (!m_DebugConstantBuffer)
            return Status::Error(ErrorCode::DeviceError, "failed to create the debug view constant buffer");

        context.tools.replay->captureParameters = [this]()
        { Json::Value p; p["debugMode"] = m_Settings.debugMode; p["depthScale"] = m_Settings.depthScale; return p; };
        context.tools.replay->restoreParameters = [this](const Json::Value& p)
        { if (p["debugMode"].isInt()) m_Settings.debugMode = p["debugMode"].asInt();
          if (p["depthScale"].isNumeric()) m_Settings.depthScale = p["depthScale"].asFloat(); };

        donut::log::info("ForwardExperiment: ready (debug mode %d).", m_Settings.debugMode);
        return Status::Ok();
    }

    bool ForwardExperiment::EnsureDebugPass(host::ExperimentContext& context, nvrhi::ITexture* depth)
    {
        if (!m_DebugReady)
        {
            nvrhi::BindingLayoutDesc layout; layout.visibility = nvrhi::ShaderType::Pixel;
            layout.bindings = {nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
                nvrhi::BindingLayoutItem::Texture_SRV(0), nvrhi::BindingLayoutItem::Sampler(0)};
            m_DebugBindingLayout = context.gpu.device->createBindingLayout(layout);
            if (!m_DebugBindingLayout) return false;
            auto status = m_DebugPass.Initialize(context.gpu.device, *context.gpu.shaders, *context.gpu.commonPasses,
                {"prism/PrismForward/debug_view.hlsl", "main_ps", nvrhi::ShaderType::Pixel, {}}, {m_DebugBindingLayout});
            if (!status) return false;
            m_DebugReady = true;
        }
        nvrhi::BindingSetDesc bindings;
        bindings.bindings = {nvrhi::BindingSetItem::ConstantBuffer(0, m_DebugConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, depth), nvrhi::BindingSetItem::Sampler(0, context.gpu.commonPasses->m_PointClampSampler)};
        m_DebugBindingSet = m_DebugPass.Bindings(bindings, m_DebugBindingLayout);
        return true;
    }

    nvrhi::ITexture* ForwardExperiment::Render(host::ExperimentContext& context, const host::ExperimentFrame& frame)
    {
        gpu::TextureCache& targets = *context.gpu.targets;

        nvrhi::ITexture* color = targets.GetOrCreate(m_ColorRequest);
        nvrhi::ITexture* depth = targets.GetOrCreate(m_DepthRequest);

        if (!color || !depth)
            return nullptr;

        nvrhi::ICommandList* commands = frame.commands;
        const nvrhi::TextureSubresourceSet subresources(0, 1, 0, 1);

        commands->clearTextureFloat(color, subresources, nvrhi::Color(
            m_ColorRequest.clearColor.x, m_ColorRequest.clearColor.y, m_ColorRequest.clearColor.z, m_ColorRequest.clearColor.w));
        commands->clearDepthStencilTexture(depth, subresources, true, kDepthClearValue, false, 0);

        if (!m_Scene.Data().graph)
            return color;

        {
            gpu::ScopedGpuScope scope(*context.gpu.profiler, commands, "Forward scene");

            m_Scene.Record(commands, frame.frame.submissionIndex, *frame.view, *frame.previousView, targets.GetFramebuffer(color, depth));
        }

        // 中间结果发布给宿主面板（顺序每帧固定）
        if (context.tools.debugViews)
        {
            context.tools.debugViews->Publish(GetName(), "Scene color", color);
            // 设备深度是 forward-Z：远平面接近 1，用 1 - R 才有对比度
            context.tools.debugViews->Publish(GetName(), "Scene depth (1 - device Z)", depth,
                { gpu::DebugViewMode::OneMinusR, 20.f, 0.f });
        }

        context.tools.comparison->Publish("Scene color", {color, ColorSpace::SceneLinear});
        if (m_Settings.debugMode <= 0)
            return color;

        nvrhi::ITexture* debugTarget = targets.GetOrCreate(m_DebugRequest);
        if (!debugTarget)
            return color;

        m_DebugFramebuffer = targets.GetFramebuffer(debugTarget, nullptr);
        if (!m_DebugFramebuffer)
            return color;

        if (!EnsureDebugPass(context, depth))
            return color;

        DebugViewConstants constants = {};
        constants.clipToWorld = frame.camera.current.clipToWorld;
        constants.inverseSize = dm::float2(
            1.f / float(frame.renderSize.width),
            1.f / float(frame.renderSize.height));
        constants.zNear = frame.camera.zNearMeters;
        constants.zFar = frame.camera.zFarMeters;
        constants.mode = m_Settings.debugMode - 1;
        constants.depthConvention = kDepthConventionForwardZ;
        constants.depthScale = m_Settings.depthScale;

        commands->writeBuffer(m_DebugConstantBuffer, &constants, sizeof(constants));

        Status debugStatus;
        {
            gpu::ScopedGpuScope scope(*context.gpu.profiler, commands, "Debug view");

            commands->clearTextureFloat(debugTarget, subresources, nvrhi::Color(
                m_DebugRequest.clearColor.x, m_DebugRequest.clearColor.y, m_DebugRequest.clearColor.z, m_DebugRequest.clearColor.w));

            debugStatus = m_DebugPass.Record(commands, m_DebugFramebuffer, {m_DebugBindingSet});
        }

        // 调试 Pass 失败时回退到场景颜色，而不是显示一张只被清空过的目标。
        if (!debugStatus)
        {
            donut::log::error("ForwardExperiment: the debug view pass failed: %s",
                debugStatus.ToStringWithCode().c_str());
            return color;
        }

        return debugTarget;
    }

    void ForwardExperiment::BuildUI(host::ExperimentContext& context)
    {
        (void)context;

        ImGui::Text("Passes: forward scene (shared) + debug view (this experiment)");

        int debugMode = m_Settings.debugMode;
        if (ImGui::Combo("Debug view", &debugMode, kDebugModeNames, int(std::size(kDebugModeNames))))
            m_Settings.debugMode = debugMode;

        if (m_Settings.debugMode > 0)
            ImGui::SliderFloat("Depth scale", &m_Settings.depthScale, 0.01f, 4.f);
    }

    void ForwardExperiment::OnResize(host::ExperimentContext& context, const Extent2D& renderSize, const Extent2D& outputSize)
    {
        (void)context;
        (void)renderSize;
        (void)outputSize;

        // 池里的纹理会被重建：这些缓存引用必须失效。
        m_DebugPass.ClearBindings();
        m_DebugFramebuffer = nullptr;
        m_DebugBindingSet = nullptr;
    }
}

std::unique_ptr<prism::host::Experiment> prism::host::CreateExperiment()
{
    return std::make_unique<prism::experiments::ForwardExperiment>();
}
