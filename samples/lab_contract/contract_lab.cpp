#include "contract_lab.h"

#include <backends/nvrhi/common/PipelineUtils.h>
#include <backends/nvrhi/common/TextureReadback.h>

#include <donut/core/json.h>
#include <donut/core/log.h>
#include <nvrhi/utils.h>

#include <imgui.h>

#include <cmath>

namespace dm = donut::math;
using namespace donut::math;

// 共享常量布局：需要 donut 的数学类型在全局可见（与 Donut 自己的 shared header 用法一致）
#include "contract_check_cb.h"

static_assert(sizeof(ContractCheckConstants) == 96, "ContractCheckConstants layout changed; update contract_check.hlsl");

namespace renderlab::labs
{
    namespace
    {
        constexpr uint32_t kDepthConventionForwardZ = 0;
        constexpr uint32_t kThreadGroupSize = 8;
    }

    const char* ContractLab::GetDescription() const
    {
        return "Verifies the data contracts end to end: matrix convention, depth convention and CPU/GPU "
               "reconstruction agreement, with JPEG-free numeric readback of the reconstructed data.";
    }

    Status ContractLab::Initialize(host::LabContext& context)
    {
        if (!context.device || !context.targets || !context.shaders)
            return Status::Error(ErrorCode::NotInitialized, "the host context is incomplete");

        if (context.config)
        {
            Json::Value settings;
            if (adapter::LoadLabSettings(*context.config, GetName(), settings))
            {
                settings["verifyFrame"] >> m_Settings.verifyFrame;
                settings["sampleStride"] >> m_Settings.sampleStride;
                settings["toleranceMeters"] >> m_Settings.toleranceMeters;
                settings["tolerancePixels"] >> m_Settings.tolerancePixels;
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

        m_PositionRequest.name = "ContractPosition";
        m_PositionRequest.format = PixelFormat::RGBA32_FLOAT;
        m_PositionRequest.usage = gpu::TextureUsage::ShaderResource | gpu::TextureUsage::UnorderedAccess;
        m_PositionRequest.hasClearValue = false;

        m_DepthCopyRequest.name = "ContractDeviceDepth";
        m_DepthCopyRequest.format = PixelFormat::R32_FLOAT;
        m_DepthCopyRequest.usage = gpu::TextureUsage::ShaderResource | gpu::TextureUsage::UnorderedAccess;
        m_DepthCopyRequest.hasClearValue = false;

        if (!context.targets->GetOrCreate(m_ColorRequest) || !context.targets->GetOrCreate(m_DepthRequest))
            return Status::Error(ErrorCode::DeviceError, "failed to create the scene render targets");

        m_CheckConstantBuffer = context.device->createBuffer(
            nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ContractCheckConstants), "ContractLabCheck", 4));

        if (!m_CheckConstantBuffer)
            return Status::Error(ErrorCode::DeviceError, "failed to create the contract check constant buffer");

        donut::log::info("ContractLab: ready (verification at frame %u, sample stride %u).",
            m_Settings.verifyFrame, m_Settings.sampleStride);

        return Status::Ok();
    }

    bool ContractLab::EnsureCheckPass(host::LabContext& context, nvrhi::ITexture* depth)
    {
        nvrhi::ITexture* positionTarget = context.targets->GetOrCreate(m_PositionRequest);
        nvrhi::ITexture* depthTarget = context.targets->GetOrCreate(m_DepthCopyRequest);

        if (!positionTarget || !depthTarget)
            return false;

        if (!m_CheckBindingSet || m_BoundDepth != depth || m_BoundPositionTarget != positionTarget || m_BoundDepthTarget != depthTarget)
        {
            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_CheckConstantBuffer),
                nvrhi::BindingSetItem::Texture_SRV(0, depth),
                nvrhi::BindingSetItem::Texture_UAV(0, positionTarget),
                nvrhi::BindingSetItem::Texture_UAV(1, depthTarget),
            };

            if (!nvrhi::utils::CreateBindingSetAndLayout(
                    context.device, nvrhi::ShaderType::Compute, 0, bindingSetDesc, m_CheckBindingLayout, m_CheckBindingSet))
            {
                return false;
            }

            m_BoundDepth = depth;
            m_BoundPositionTarget = positionTarget;
            m_BoundDepthTarget = depthTarget;
        }

        if (!m_CheckPipeline)
        {
            nvrhi::ShaderHandle computeShader = context.shaders->GetShader(
                "renderlab/contract_check.hlsl", "main_cs", nvrhi::ShaderType::Compute);

            if (!computeShader)
                return false;

            m_CheckPipeline = gpu::CreateComputePipeline(context.device, computeShader, m_CheckBindingLayout);
            if (!m_CheckPipeline)
                return false;
        }

        return true;
    }

    void ContractLab::BeginFrame(host::LabContext& context, const host::LabFrame& frame)
    {
        if (!m_HasVerifiedCamera)
            return;

        const bool scheduled = !m_Report.ran && frame.frame.frameIndex >= m_Settings.verifyFrame + 1;

        if (!scheduled && !m_VerificationRequested)
            return;

        m_VerificationRequested = false;
        RunVerification(context);
    }

    nvrhi::ITexture* ContractLab::Render(host::LabContext& context, const host::LabFrame& frame)
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

        if (context.scenePipeline && context.scene && context.scene->graph)
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

        if (!EnsureCheckPass(context, depth))
            return color;

        ContractCheckConstants constants = {};
        constants.clipToWorld = frame.camera.current.clipToWorld;
        constants.inverseSize = dm::float2(
            1.f / float(frame.renderSize.width),
            1.f / float(frame.renderSize.height));
        constants.size = dm::uint2(frame.renderSize.width, frame.renderSize.height);
        constants.zNear = frame.camera.zNearMeters;
        constants.zFar = frame.camera.zFarMeters;
        constants.depthConvention = kDepthConventionForwardZ;

        commands->writeBuffer(m_CheckConstantBuffer, &constants, sizeof(constants));

        {
            gpu::ScopedGpuScope scope(*context.profiler, commands, "Contract check");

            const dm::uint3 groups(
                (frame.renderSize.width + kThreadGroupSize - 1) / kThreadGroupSize,
                (frame.renderSize.height + kThreadGroupSize - 1) / kThreadGroupSize,
                1);

            gpu::Dispatch(commands, m_CheckPipeline, m_CheckBindingSet, groups);
        }

        // 下一帧的 BeginFrame 会用这些数据做校验。
        m_VerifiedCamera = frame.camera;
        m_HasVerifiedCamera = true;

        return color;
    }

    void ContractLab::VerifyCpuMath(const renderlab::CameraData& camera)
    {
        // 矩阵往返：world -> clip -> world
        const dm::float3 probes[] = {
            dm::float3(0.f, 0.f, 0.f),
            dm::float3(1.f, 2.f, -3.f),
            camera.position + camera.forward * 5.f,
            camera.position + camera.right * 2.f + camera.up * 1.f + camera.forward * 8.f,
        };

        for (const dm::float3& probe : probes)
        {
            const dm::float4 clip = dm::float4(probe, 1.f) * camera.current.worldToClip;
            const dm::float4 back = clip * camera.current.clipToWorld;
            if (std::fabs(back.w) < 1e-8f)
                continue;

            const dm::float3 reconstructed = dm::float3(back.x, back.y, back.z) / back.w;
            m_Report.maxMatrixRoundTripError = std::max(m_Report.maxMatrixRoundTripError, dm::length(reconstructed - probe));
        }

        // 深度往返：device depth -> linear depth -> device depth
        for (int step = 0; step <= 9; ++step)
        {
            const float deviceDepth = float(step) / 10.f;
            const float linearDepth = renderlab::LinearizeDepth(
                deviceDepth, camera.zNearMeters, camera.zFarMeters, camera.depthConvention);
            const float roundTrip = renderlab::DeviceDepthFromLinear(
                linearDepth, camera.zNearMeters, camera.zFarMeters, camera.depthConvention);

            m_Report.maxDepthRoundTripError = std::max(m_Report.maxDepthRoundTripError, std::fabs(roundTrip - deviceDepth));
        }
    }

    void ContractLab::RunVerification(host::LabContext& context)
    {
        const renderlab::CameraData& camera = m_VerifiedCamera;

        m_Report = ContractVerificationReport{};
        VerifyCpuMath(camera);

        nvrhi::ITexture* positionTarget = context.targets->Find(m_PositionRequest.name.c_str());
        nvrhi::ITexture* depthTarget = context.targets->Find(m_DepthCopyRequest.name.c_str());

        if (!positionTarget || !depthTarget)
        {
            m_Report.summary = "the contract targets are missing";
            donut::log::error("ContractLab: %s", m_Report.summary.c_str());
            return;
        }

        const Result<gpu::TextureData> positions = gpu::ReadTexture(context.device, positionTarget, PixelFormat::RGBA32_FLOAT);
        const Result<gpu::TextureData> depths = gpu::ReadTexture(context.device, depthTarget, PixelFormat::R32_FLOAT);

        if (!positions.IsOk() || !depths.IsOk())
        {
            m_Report.summary = "texture readback failed";
            donut::log::error("ContractLab: %s", m_Report.summary.c_str());
            return;
        }

        const gpu::TextureData& positionData = positions.Value();
        const gpu::TextureData& depthData = depths.Value();

        const Extent2D size = positionData.size;
        const uint32_t stride = std::max(m_Settings.sampleStride, 1u);

        for (uint32_t y = 0; y < size.height; y += stride)
        {
            for (uint32_t x = 0; x < size.width; x += stride)
            {
                const float deviceDepth = depthData.FloatAt(x, y);

                // 背景像素（清空值）不参与比较
                if (deviceDepth >= kDepthClearValue)
                    continue;

                const dm::float4 gpuSample = positionData.ColorAt(x, y);
                const dm::float3 gpuWorld = dm::float3(gpuSample.x, gpuSample.y, gpuSample.z);
                const float gpuLinearDepth = gpuSample.w;

                const dm::float2 uv = dm::float2(
                    (float(x) + 0.5f) / float(size.width),
                    (float(y) + 0.5f) / float(size.height));

                // 1) 矩阵约定：把 GPU 重建的世界位置投影回屏幕，应当落回同一个像素
                const dm::float2 projectedUv = camera.WorldToUnjitteredUv(gpuWorld);
                const dm::float2 uvError = dm::float2(
                    (projectedUv.x - uv.x) * float(size.width),
                    (projectedUv.y - uv.y) * float(size.height));
                m_Report.maxUvErrorPixels = std::max(m_Report.maxUvErrorPixels, dm::length(uvError));

                // 2) 深度约定：GPU 的线性深度应当等于 CPU 用同一个设备深度算出的线性深度
                const float cpuLinearDepth = camera.LinearizeDepth(deviceDepth);
                m_Report.maxLinearDepthErrorMeters = std::max(
                    m_Report.maxLinearDepthErrorMeters, std::fabs(cpuLinearDepth - gpuLinearDepth));

                // 3) 重建一致性：CPU 与 GPU 从同一个设备深度重建的世界位置应当一致
                const dm::float3 cpuWorld = camera.ReconstructWorldPosition(uv, deviceDepth);
                m_Report.maxPositionErrorMeters = std::max(
                    m_Report.maxPositionErrorMeters, dm::length(cpuWorld - gpuWorld));

                ++m_Report.sampledPixels;
            }
        }

        m_Report.ran = true;
        m_Report.passed =
            m_Report.sampledPixels > 0 &&
            m_Report.maxUvErrorPixels <= m_Settings.tolerancePixels &&
            m_Report.maxPositionErrorMeters <= m_Settings.toleranceMeters &&
            m_Report.maxLinearDepthErrorMeters <= m_Settings.toleranceMeters &&
            m_Report.maxMatrixRoundTripError <= 1e-3f &&
            m_Report.maxDepthRoundTripError <= 1e-5f;

        m_Report.summary = m_Report.passed ? "passed" : "failed";

        donut::log::info("ContractLab: verification %s -- samples %u, uv error %.4f px, position error %.5f m, "
            "linear depth error %.6f m, matrix round trip %.6f, depth round trip %.3e",
            m_Report.summary.c_str(),
            m_Report.sampledPixels,
            m_Report.maxUvErrorPixels,
            m_Report.maxPositionErrorMeters,
            m_Report.maxLinearDepthErrorMeters,
            m_Report.maxMatrixRoundTripError,
            m_Report.maxDepthRoundTripError);

        if (!m_Report.passed && m_Report.sampledPixels > 0)
        {
            // 不静默通过：约定不一致必须让实验失败。
            donut::log::error("ContractLab: the data contracts are not consistent, see docs/architecture.md section 3.");
        }
    }

    void ContractLab::BuildUI(host::LabContext& context)
    {
        ImGui::TextWrapped("A GPU pass reconstructs world position and linear depth from the depth buffer; "
            "the CPU verifies matrix and depth conventions against its own math.");

        if (ImGui::Button("Verify now"))
            m_VerificationRequested = true;

        ImGui::SameLine();
        ImGui::Text("Samples: %u", m_Report.sampledPixels);

        if (m_Report.ran)
        {
            const ImVec4 color = m_Report.passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.f) : ImVec4(0.95f, 0.4f, 0.35f, 1.f);
            ImGui::TextColored(color, "Result: %s", m_Report.passed ? "PASSED" : "FAILED");
            ImGui::Text("Max uv error: %.4f px (limit %.3f)", m_Report.maxUvErrorPixels, m_Settings.tolerancePixels);
            ImGui::Text("Max position error: %.5f m (limit %.4f)", m_Report.maxPositionErrorMeters, m_Settings.toleranceMeters);
            ImGui::Text("Max linear depth error: %.6f m", m_Report.maxLinearDepthErrorMeters);
            ImGui::Text("Matrix round trip: %.6f", m_Report.maxMatrixRoundTripError);
            ImGui::Text("Depth round trip: %.3e", m_Report.maxDepthRoundTripError);
        }
        else
        {
            ImGui::TextUnformatted("Result: not run");
            ImGui::SliderInt("Verify frame", reinterpret_cast<int*>(&m_Settings.verifyFrame), 1, 30);
        }

        if (ImGui::Button("Save reconstructed data (PNG)"))
        {
            if (nvrhi::ITexture* target = context.targets->Find(m_PositionRequest.name.c_str()))
            {
                const std::filesystem::path path = std::filesystem::path(context.assetsDirectory).empty()
                    ? std::filesystem::path("contract_position.png")
                    : context.assetsDirectory.parent_path() / "contract_position.png";

                context.callbacks.saveTexture(target, path, nvrhi::ResourceStates::UnorderedAccess);
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("(needs GPU idle, off the frame path)");
    }

    void ContractLab::OnResize(host::LabContext& context, const Extent2D& renderSize, const Extent2D& outputSize)
    {
        (void)context;
        (void)renderSize;
        (void)outputSize;

        m_CheckBindingSet = nullptr;
        m_CheckPipeline = nullptr;
        m_BoundDepth = nullptr;
        m_BoundPositionTarget = nullptr;
        m_BoundDepthTarget = nullptr;
    }
}

std::unique_ptr<renderlab::host::Lab> renderlab::host::CreateLab()
{
    return std::make_unique<renderlab::labs::ContractLab>();
}
