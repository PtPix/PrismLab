#include "LabHost.h"

#include <backends/nvrhi/common/TextureReadback.h>

#include <donut/core/log.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace renderlab::host
{
    namespace
    {
        // Halton 序列：低差异抖动，配合时域算法使用时域累积。
        float Halton(uint32_t index, uint32_t base)
        {
            float result = 0.f;
            float fraction = 1.f;

            while (index > 0)
            {
                fraction /= float(base);
                result += fraction * float(index % base);
                index /= base;
            }

            return result;
        }
    }

    LabRenderPass::LabRenderPass(
        donut::app::DeviceManager* deviceManager,
        HostStats& stats,
        std::unique_ptr<Lab> lab,
        const HostServices& services,
        const CommandLine& commandLine)
        : donut::app::IRenderPass(deviceManager)
        , m_Stats(stats)
        , m_Lab(std::move(lab))
        , m_Services(services)
        , m_CommandLine(commandLine)
    {
    }

    LabRenderPass::~LabRenderPass() = default;

    Status LabRenderPass::Initialize()
    {
        if (!m_Lab)
            return Status::Error(ErrorCode::InvalidArgument, "no lab was created");

        if (!m_Services.device || !m_Services.shaders || !m_Services.targets || !m_Services.profiler)
            return Status::Error(ErrorCode::NotInitialized, "the host services are incomplete");

        m_CommandList = m_Services.device->createCommandList();
        m_BindingCache = std::make_unique<donut::engine::BindingCache>(m_Services.device);

        m_Context.device = m_Services.device;
        m_Context.shaderFactory = m_Services.shaderFactory.get();
        m_Context.commonPasses = m_Services.commonPasses.get();
        m_Context.shaders = m_Services.shaders;
        m_Context.targets = m_Services.targets;
        m_Context.profiler = m_Services.profiler;
        m_Context.scenePipeline = m_Services.scenePipeline;
        m_Context.scene = m_Services.sceneHost ? &m_Services.sceneHost->GetData() : nullptr;
        m_Context.config = m_Services.config;
        m_Context.assetsDirectory = m_Services.assetsDirectory;

        m_Context.callbacks.requestHistoryReset = [this](renderlab::HistoryResetReason reason)
        {
            RequestHistoryReset(reason);
        };

        m_Context.callbacks.saveTexture = [this](nvrhi::ITexture* texture, const std::filesystem::path& path, nvrhi::ResourceStates state)
        {
            if (!texture)
                return false;

            m_Services.device->waitForIdle();
            return gpu::SaveTextureToImage(m_Services.device, m_Services.commonPasses.get(), texture, state, path, true);
        };

        m_Context.callbacks.requestQuit = [this]()
        {
            RequestQuit();
        };

        if (m_Services.config)
        {
            m_Context.ambientTop = dm::float3(m_Services.config->lighting.ambientIntensity);
            m_Context.ambientBottom = dm::float3(m_Services.config->lighting.ambientIntensity * 0.6f);

            m_Camera.Initialize(m_Services.config->camera);
            m_OutputSize = Extent2D{ std::max(m_Services.config->window.width, 1u), std::max(m_Services.config->window.height, 1u) };

            const float renderScale = std::clamp(m_Services.config->render.renderScale, 0.1f, 2.f);
            m_RenderSize = m_OutputSize.Scaled(renderScale);
        }
        else
        {
            m_Camera.Initialize(adapter::HostCameraPreset{});
            m_OutputSize = Extent2D{ 1280, 720 };
            m_RenderSize = m_OutputSize;
        }

        m_Services.targets->SetRenderSize(m_RenderSize);

        const Status status = m_Lab->Initialize(m_Context);
        if (status.IsError())
            return WithContext(status, std::string(m_Lab->GetName()) + " initialization failed");

        // 相机在实验初始化之前就已经就位：把首帧的相机数据补上。
        m_Camera.Update(0.f, m_RenderSize);

        m_Initialized = true;
        m_HistoryResetFlags = HistoryResetBit(HistoryResetReason::FirstFrame);

        if (m_Context.jitterSampleCount > 0)
            m_HistoryResetFlags |= HistoryResetBit(HistoryResetReason::SettingsChange);

        donut::log::info("RenderLab: %s initialized (render %u x %u, output %u x %u).",
            m_Lab->GetName(), m_RenderSize.width, m_RenderSize.height, m_OutputSize.width, m_OutputSize.height);

        return Status::Ok();
    }

    void LabRenderPass::RequestHistoryReset(renderlab::HistoryResetReason reason)
    {
        m_HistoryResetFlags |= renderlab::HistoryResetBit(reason);
    }

    void LabRenderPass::RequestQuit()
    {
        if (m_QuitRequested)
            return;

        m_QuitRequested = true;

        if (GLFWwindow* window = GetDeviceManager()->GetWindow())
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    void LabRenderPass::UpdateJitter()
    {
        m_PreviousJitter = m_Jitter;

        if (m_Context.jitterSampleCount == 0)
        {
            m_Jitter = dm::float2(0.f);
            return;
        }

        // 以抖动序列的第一个样本开始，保证历史有效时序列可复现。
        const uint32_t index = uint32_t((m_FrameCounter % m_Context.jitterSampleCount) + 1);
        m_Jitter = dm::float2(
            Halton(index, 2) - 0.5f,
            Halton(index, 3) - 0.5f);
    }

    void LabRenderPass::UpdateRenderSize(Extent2D outputSize)
    {
        if (!outputSize.IsValid())
            outputSize = Extent2D{ 1, 1 };

        if (m_OutputSize == outputSize)
            return;

        const float renderScale = m_Services.config
            ? std::clamp(m_Services.config->render.renderScale, 0.1f, 2.f)
            : 1.f;

        const Extent2D renderSize = outputSize.Scaled(renderScale);
        if (m_OutputSize != outputSize || m_RenderSize != renderSize)
        {
            m_OutputSize = outputSize;
            m_RenderSize = renderSize;
            m_ResolutionChanged = true;

            m_Services.targets->SetRenderSize(m_RenderSize);
        }
    }

    void LabRenderPass::Animate(float elapsedTimeSeconds)
    {
        if (!m_Initialized || m_HasFailed)
            return;

        m_TimeSeconds += elapsedTimeSeconds;

        m_Camera.Update(elapsedTimeSeconds, m_RenderSize);

        if (m_Camera.ConsumeDiscontinuity())
            RequestHistoryReset(HistoryResetReason::CameraCut);

        if (m_ResolutionChanged)
        {
            RequestHistoryReset(HistoryResetReason::ResolutionChange);
            m_ResolutionChanged = false;

            if (m_Lab)
                m_Lab->OnResize(m_Context, m_RenderSize, m_OutputSize);
        }

        UpdateJitter();
        m_Camera.SetJitter(m_Jitter);

        const float frameTimeMs = elapsedTimeSeconds * 1000.f;
        m_Stats.smoothedFrameTimeMs = (m_Stats.smoothedFrameTimeMs <= 0.f)
            ? frameTimeMs
            : (m_Stats.smoothedFrameTimeMs * 0.9f + frameTimeMs * 0.1f);

        m_Stats.frameTimeMs = m_Stats.smoothedFrameTimeMs;
        m_Stats.framesPerSecond = (m_Stats.smoothedFrameTimeMs > 0.f) ? (1000.f / m_Stats.smoothedFrameTimeMs) : 0.f;
        m_Stats.renderSize = m_RenderSize;
        m_Stats.outputSize = m_OutputSize;
        m_Stats.cameraPosition = m_Camera.GetPosition();
        m_Stats.cameraDirection = m_Camera.GetDirection();
        m_Stats.firstPerson = m_Camera.IsFirstPerson();
        m_Stats.cameraDistance = m_Camera.GetDistance();
        m_Stats.frameIndex = m_FrameCounter;

        renderlab::FrameInfo historyResetInfo;
        historyResetInfo.historyResetFlags = m_HistoryResetFlags;
        m_Stats.historyResetDescription = historyResetInfo.DescribeHistoryReset();
    }

    void LabRenderPass::Render(nvrhi::IFramebuffer* framebuffer)
    {
        if (!m_Initialized || m_HasFailed || !framebuffer)
            return;

        auto device = GetDevice();

        m_Frame = LabFrame{};
        m_Frame.commands = m_CommandList;
        m_Frame.frame.frameIndex = m_FrameCounter;
        m_Frame.frame.deltaTimeSeconds = float(GetDeviceManager()->GetAverageFrameTimeSeconds());
        m_Frame.frame.timeSeconds = float(m_TimeSeconds);
        m_Frame.frame.viewId = renderlab::kPrimaryViewId;
        m_Frame.frame.renderSize = m_RenderSize;
        m_Frame.frame.outputSize = m_OutputSize;
        m_Frame.frame.jitter = m_Jitter;
        m_Frame.frame.previousJitter = m_PreviousJitter;
        m_Frame.frame.historyResetFlags = m_HistoryResetFlags;
        m_Frame.camera = m_Camera.GetCameraData();
        m_Frame.camera.jitter = m_Jitter;
        m_Frame.camera.previousJitter = m_PreviousJitter;
        m_Frame.renderSize = m_RenderSize;
        m_Frame.outputSize = m_OutputSize;

        m_Frame.view = &m_Camera.GetView();
        m_PreviousView.SetViewport(nvrhi::Viewport(float(m_RenderSize.width), float(m_RenderSize.height)));
        m_PreviousView.SetMatrices(m_Frame.camera.previous.worldToView, m_Frame.camera.previous.viewToClip);
        m_PreviousView.UpdateCache();
        m_Frame.previousView = &m_PreviousView;

        // 命令列表还没打开：实验可以在这里做读回与数值验证。
        m_Frame.commands = nullptr;
        if (m_Lab)
            m_Lab->BeginFrame(m_Context, m_Frame);
        m_Frame.commands = m_CommandList;

        m_CommandList->open();
        m_Services.profiler->BeginFrame(m_CommandList);

        // 资产场景需要每帧刷新动画与缓冲；程序化场景是空实现。
        if (m_Services.sceneHost)
            m_Services.sceneHost->Update(m_CommandList, uint32_t(m_FrameCounter));

        m_OutputTexture = nullptr;
        if (m_Lab)
        {
            gpu::ScopedGpuScope scope(*m_Services.profiler, m_CommandList, m_Lab->GetName());
            m_OutputTexture = m_Lab->Render(m_Context, m_Frame);
            ++m_Stats.labFrames;
        }

        m_Services.profiler->EndFrame();

        if (m_OutputTexture)
            m_Services.commonPasses->BlitTexture(m_CommandList, framebuffer, m_OutputTexture, m_BindingCache.get());

        m_CommandList->close();
        device->executeCommandList(m_CommandList);

        m_HistoryResetFlags = 0;
        ++m_FrameCounter;

        HandleEndOfFrame(m_CommandList);
    }

    bool LabRenderPass::HandleEndOfFrame(nvrhi::ICommandList* commands)
    {
        (void)commands;

        // 截图：命令列表已经提交，读回需要设备空闲。
        const bool captureRequested = !m_CommandLine.capturePath.empty();
        if (captureRequested && !m_Captured && m_FrameCounter > m_CommandLine.captureFrame)
        {
            m_Captured = true;

            if (m_OutputTexture)
            {
                GetDevice()->waitForIdle();
                gpu::SaveTextureToImage(
                    GetDevice(),
                    m_Services.commonPasses.get(),
                    m_OutputTexture,
                    nvrhi::ResourceStates::RenderTarget,
                    m_CommandLine.capturePath,
                    true);
                m_Stats.outputCaptureSize = Extent2D{ m_OutputTexture->getDesc().width, m_OutputTexture->getDesc().height };
            }
            else
            {
                donut::log::warning("RenderLab: capture requested but the lab produced no output texture.");
            }

            RequestQuit();
            return true;
        }

        if (m_CommandLine.smokeTest && m_FrameCounter >= m_CommandLine.smokeTestFrames)
        {
            donut::log::info("RenderLab: smoke test finished %llu frames, shutting down.", (unsigned long long)m_FrameCounter);
            RequestQuit();
            return true;
        }

        return false;
    }

    void LabRenderPass::BackBufferResizing()
    {
        // 等待 GPU：命令列表返回不代表 GPU 已经用完这些资源。
        GetDevice()->waitForIdle();

        m_Services.targets->Clear();
        m_BindingCache->Clear();
        m_OutputTexture = nullptr;
    }

    void LabRenderPass::BackBufferResized(const uint32_t width, const uint32_t height, const uint32_t sampleCount)
    {
        (void)sampleCount;

        UpdateRenderSize(Extent2D{ std::max(width, 1u), std::max(height, 1u) });

        donut::log::info("RenderLab: output resized to %u x %u (render %u x %u).",
            m_OutputSize.width, m_OutputSize.height, m_RenderSize.width, m_RenderSize.height);
    }

    bool LabRenderPass::KeyboardUpdate(int key, int scancode, int action, int mods)
    {
        if (m_Lab && m_Initialized && m_Lab->OnKey(m_Context, key, action, mods))
            return true;

        return m_Camera.KeyboardUpdate(key, scancode, action, mods);
    }

    bool LabRenderPass::MousePosUpdate(double xpos, double ypos)
    {
        return m_Camera.MousePosUpdate(xpos, ypos);
    }

    bool LabRenderPass::MouseButtonUpdate(int button, int action, int mods)
    {
        return m_Camera.MouseButtonUpdate(button, action, mods);
    }

    bool LabRenderPass::MouseScrollUpdate(double xoffset, double yoffset)
    {
        return m_Camera.MouseScrollUpdate(xoffset, yoffset);
    }

    bool LabRenderPass::ShouldAnimateUnfocused()
    {
        return m_CommandLine.smokeTest || !m_CommandLine.capturePath.empty();
    }

    bool LabRenderPass::ShouldRenderUnfocused()
    {
        return m_CommandLine.smokeTest || !m_CommandLine.capturePath.empty();
    }
}
