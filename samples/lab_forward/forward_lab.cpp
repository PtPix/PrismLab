#include "forward_lab.h"

#include <backends/nvrhi/common/PipelineUtils.h>

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

namespace renderlab::labs
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

    const char* ForwardLab::GetDescription() const
    {
        return "Shared forward scene path plus a debug view pass written by the experiment (depth "
               "decode, world position reconstruction and depth-derived normals).";
    }

    Status ForwardLab::Initialize(host::LabContext& context)
    {
        if (!context.device || !context.targets || !context.shaders)
            return Status::Error(ErrorCode::NotInitialized, "the host context is incomplete");

        // 读取本实验自己的配置段（configs/host/camera_default.json 的 labs.ForwardLab）
        if (context.config)
        {
            Json::Value settings;
            if (adapter::LoadLabSettings(*context.config, GetName(), settings))
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

        if (!context.targets->GetOrCreate(m_ColorRequest) || !context.targets->GetOrCreate(m_DepthRequest))
            return Status::Error(ErrorCode::DeviceError, "failed to create the scene render targets");

        m_DebugConstantBuffer = context.device->createBuffer(
            nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(DebugViewConstants), "ForwardLabDebugView", 4));

        if (!m_DebugConstantBuffer)
            return Status::Error(ErrorCode::DeviceError, "failed to create the debug view constant buffer");

        donut::log::info("ForwardLab: ready (debug mode %d).", m_Settings.debugMode);
        return Status::Ok();
    }

    bool ForwardLab::EnsureDebugPass(host::LabContext& context, nvrhi::ITexture* depth)
    {
        // 绑定集引用池里的深度纹理：纹理被重建时（窗口缩放）需要重建绑定集。
        if (!m_DebugBindingSet || m_BoundDepthTexture != depth)
        {
            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_DebugConstantBuffer),
                nvrhi::BindingSetItem::Texture_SRV(0, depth),
                nvrhi::BindingSetItem::Sampler(0, context.commonPasses->m_PointClampSampler),
            };

            if (!nvrhi::utils::CreateBindingSetAndLayout(
                    context.device, nvrhi::ShaderType::Pixel, 0, bindingSetDesc, m_DebugBindingLayout, m_DebugBindingSet))
            {
                return false;
            }

            m_BoundDepthTexture = depth;
        }

        nvrhi::ITexture* target = m_DebugFramebuffer ? m_DebugFramebuffer->getDesc().colorAttachments[0].texture : nullptr;

        if (!m_DebugPipeline || m_DebugFramebufferTarget != target)
        {
            m_DebugFramebufferTarget = target;

            nvrhi::ShaderHandle pixelShader = context.shaders->GetShader(
                "renderlab/debug_view.hlsl", "main_ps", nvrhi::ShaderType::Pixel);

            if (!pixelShader)
                return false;

            gpu::FullScreenPipelineDesc pipelineDesc;
            pipelineDesc.vertexShader = context.commonPasses->m_FullscreenVS;
            pipelineDesc.pixelShader = pixelShader;
            pipelineDesc.framebuffer = m_DebugFramebuffer;
            pipelineDesc.bindingLayout = m_DebugBindingLayout;

            m_DebugPipeline = gpu::CreateFullScreenPipeline(context.device, pipelineDesc);
            if (!m_DebugPipeline)
                return false;
        }

        return true;
    }

    nvrhi::ITexture* ForwardLab::Render(host::LabContext& context, const host::LabFrame& frame)
    {
        gpu::RenderTargetPool& targets = *context.targets;

        nvrhi::ITexture* color = targets.GetOrCreate(m_ColorRequest);
        nvrhi::ITexture* depth = targets.GetOrCreate(m_DepthRequest);

        if (!color || !depth)
            return nullptr;

        nvrhi::ICommandList* commands = frame.commands;
        const nvrhi::TextureSubresourceSet subresources(0, 1, 0, 1);

        commands->clearTextureFloat(color, subresources, nvrhi::Color(
            m_ColorRequest.clearColor.x, m_ColorRequest.clearColor.y, m_ColorRequest.clearColor.z, m_ColorRequest.clearColor.w));
        commands->clearDepthStencilTexture(depth, subresources, true, kDepthClearValue, false, 0);

        if (!context.scenePipeline || !context.scene || !context.scene->graph)
            return color;

        {
            gpu::ScopedGpuScope scope(*context.profiler, commands, "Forward scene");

            context.scenePipeline->RenderScene(
                commands,
                *context.scene->graph,
                *frame.view,
                *frame.previousView,
                targets.GetFramebuffer(color, depth),
                context.ambientTop,
                context.ambientBottom);
        }

        // 中间结果发布给宿主面板（顺序每帧固定）
        if (context.debugViews)
        {
            context.debugViews->Publish(GetName(), "Scene color", color);
            // 设备深度是 forward-Z：远平面接近 1，用 1 - R 才有对比度
            context.debugViews->Publish(GetName(), "Scene depth (1 - device Z)", depth,
                { gpu::DebugViewMode::OneMinusR, 20.f, 0.f });
        }

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

        {
            gpu::ScopedGpuScope scope(*context.profiler, commands, "Debug view");

            commands->clearTextureFloat(debugTarget, subresources, nvrhi::Color(
                m_DebugRequest.clearColor.x, m_DebugRequest.clearColor.y, m_DebugRequest.clearColor.z, m_DebugRequest.clearColor.w));

            gpu::DrawFullScreenQuad(commands, m_DebugPipeline, m_DebugFramebuffer, m_DebugBindingSet);
        }

        return debugTarget;
    }

    void ForwardLab::BuildUI(host::LabContext& context)
    {
        (void)context;

        ImGui::Text("Passes: forward scene (shared) + debug view (this lab)");

        int debugMode = m_Settings.debugMode;
        if (ImGui::Combo("Debug view", &debugMode, kDebugModeNames, int(std::size(kDebugModeNames))))
            m_Settings.debugMode = debugMode;

        if (m_Settings.debugMode > 0)
            ImGui::SliderFloat("Depth scale", &m_Settings.depthScale, 0.01f, 4.f);
    }

    void ForwardLab::OnResize(host::LabContext& context, const Extent2D& renderSize, const Extent2D& outputSize)
    {
        (void)context;
        (void)renderSize;
        (void)outputSize;

        // 池里的纹理会被重建：这些缓存引用必须失效。
        m_DebugPipeline = nullptr;
        m_DebugFramebuffer = nullptr;
        m_DebugFramebufferTarget = nullptr;
        m_DebugBindingSet = nullptr;
        m_BoundDepthTexture = nullptr;
    }
}

std::unique_ptr<renderlab::host::Lab> renderlab::host::CreateLab()
{
    return std::make_unique<renderlab::labs::ForwardLab>();
}
