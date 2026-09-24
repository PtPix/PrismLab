#include "ExperimentHost.h"

#include "framework/tools/capture/ImageReference.h"

#include <framework/tools/capture/TextureReadback.h>

#include <donut/core/log.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace prism::host
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

    ExperimentRenderPass::ExperimentRenderPass(
        donut::app::DeviceManager* deviceManager,
        HostStats& stats,
        std::unique_ptr<Experiment> experiment,
        const HostServices& services,
        const CommandLine& commandLine)
        : donut::app::IRenderPass(deviceManager)
        , m_Stats(stats)
        , m_Experiment(std::move(experiment))
        , m_Services(services)
        , m_CommandLine(commandLine)
    {
    }

    ExperimentRenderPass::~ExperimentRenderPass()
    {
        if (m_InitializeAttempted && m_Experiment)
        {
            GetDevice()->waitForIdle();
            m_Experiment->Shutdown(m_Context);
        }
        m_Tools.replay.captureParameters = {};
        m_Tools.replay.restoreParameters = {};
        m_Experiment.reset();
    }

    Status ExperimentRenderPass::Initialize()
    {
        if (!m_Experiment)
            return Status::Error(ErrorCode::InvalidArgument, "no experiment was created");

        if (!m_Services.device || !m_Services.shaders || !m_Services.targets || !m_Services.profiler)
            return Status::Error(ErrorCode::NotInitialized, "the host services are incomplete");

        m_CommandList = m_Services.device->createCommandList();

        m_Context.gpu.device = m_Services.device;
        m_Context.gpu.shaderFactory = m_Services.shaderFactory.get();
        m_Context.gpu.commonPasses = m_Services.commonPasses.get();
        m_Context.gpu.shaders = m_Services.shaders;
        m_Context.gpu.targets = m_Services.targets;
        m_Context.gpu.buffers = m_Services.buffers;
        m_Context.gpu.resources = m_Services.resources;
        m_Context.gpu.profiler = m_Services.profiler;
        m_Context.config = m_Services.config;
        m_Context.assetsDirectory = m_Services.assetsDirectory;

        m_Context.callbacks.requestHistoryReset = [this](prism::HistoryResetReason reason)
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

        m_Context.tools.debugViews = &m_DebugViews;
        m_Context.tools.metrics = &m_Metrics;
        m_Context.tools.comparison = &m_Tools.comparison;
        m_Context.tools.replay = &m_Tools.replay;
        const auto toolsStatus = m_Tools.Initialize(m_Services.device, *m_Services.shaders,
            *m_Services.commonPasses, m_Services.executablePath);
        if (!toolsStatus) return toolsStatus;

        // 公共调试视图的显示 Pass（框架自带 shader；不存在时只是没有该功能，不影响实验）
        if (!m_DebugViewPass.Initialize(m_Services.device, *m_Services.shaders))
            donut::log::warning("Prism: the shared debug view pass is unavailable (prism/DebugView.hlsl).");

        m_DebugTargetRequest.name = "DebugView.Output";
        m_DebugTargetRequest.format = PixelFormat::RGBA16_FLOAT;
        m_DebugTargetRequest.usage = gpu::TextureUsage::ShaderResource | gpu::TextureUsage::RenderTarget;
        m_DebugTargetRequest.clearColor = dm::float4(0.02f, 0.02f, 0.03f, 1.f);

        if (m_Services.config)
        {

            m_Camera.Initialize(m_Services.config->camera);
            m_OutputSize = Extent2D{ std::max(m_Services.config->window.width, 1u), std::max(m_Services.config->window.height, 1u) };

            const float renderScale = std::clamp(m_Services.config->render.renderScale, 0.1f, 2.f);
            m_RenderSize = m_OutputSize.Scaled(renderScale);
        }
        else
        {
            m_Camera.Initialize(CameraPreset{});
            m_OutputSize = Extent2D{ 1280, 720 };
            m_RenderSize = m_OutputSize;
        }

        if (m_Services.resources)
            m_Services.resources->SetRenderSize(m_RenderSize);
        else if (m_Services.targets)
            m_Services.targets->SetRenderSize(m_RenderSize);



        // 命令行指定了调试视图：条目在实验第一次 Publish 之后才存在，索引会保留到这里生效。
        m_DebugViews.SetSelectedIndex(m_CommandLine.debugView);

        m_InitializeAttempted = true;
        const Status status = m_Experiment->Initialize(m_Context);
        m_Stats.scene = m_Context.scene.stats;
        m_Stats.sceneDescription = m_Context.scene.description;
        m_Metrics.SetContext(
            m_Experiment->GetName(),
            m_Stats.sceneDescription,
            m_Stats.rendererDescription,
            m_RenderSize,
            m_OutputSize);
        if (status.IsError())
            return WithContext(status, std::string(m_Experiment->GetName()) + " initialization failed");

        // 相机在实验初始化之前就已经就位：把首帧的相机数据补上。
        m_Camera.Update(0.f, m_RenderSize);

        m_Initialized = true;
        m_HistoryResetFlags = HistoryResetBit(HistoryResetReason::FirstFrame);

        if (m_Context.temporal.jitterSampleCount > 0)
            m_HistoryResetFlags |= HistoryResetBit(HistoryResetReason::SettingsChange);

        donut::log::info("Prism: %s initialized (render %u x %u, output %u x %u).",
            m_Experiment->GetName(), m_RenderSize.width, m_RenderSize.height, m_OutputSize.width, m_OutputSize.height);

        return Status::Ok();
    }

    void ExperimentRenderPass::RequestHistoryReset(prism::HistoryResetReason reason)
    {
        m_HistoryResetFlags |= prism::HistoryResetBit(reason);
    }

    void ExperimentRenderPass::RequestQuit()
    {
        if (m_QuitRequested)
            return;

        m_QuitRequested = true;

        if (GLFWwindow* window = GetDeviceManager()->GetWindow())
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    void ExperimentRenderPass::UpdateJitter()
    {
        m_PreviousJitter = m_Jitter;

        if (m_Context.temporal.jitterSampleCount == 0)
        {
            m_Jitter = dm::float2(0.f);
            return;
        }

        // 以抖动序列的第一个样本开始，保证历史有效时序列可复现。
        const uint32_t index = uint32_t((m_Tools.replay.GetFrame().tick % m_Context.temporal.jitterSampleCount) + 1);
        m_Jitter = dm::float2(
            Halton(index, 2) - 0.5f,
            Halton(index, 3) - 0.5f);
    }

    void ExperimentRenderPass::UpdateRenderSize(Extent2D outputSize)
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

            if (m_Services.resources) m_Services.resources->SetRenderSize(m_RenderSize);
            else m_Services.targets->SetRenderSize(m_RenderSize);
        }
    }

    void ExperimentRenderPass::Animate(float elapsedTimeSeconds)
    {
        if (!m_Initialized || m_HasFailed)
            return;

        m_PendingElapsed += elapsedTimeSeconds;

        if (m_ResolutionChanged)
        {
            RequestHistoryReset(HistoryResetReason::ResolutionChange);
            m_ResolutionChanged = false;

            if (m_Experiment)
                m_Experiment->OnResize(m_Context, m_RenderSize, m_OutputSize);
        }



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

        prism::FrameInfo historyResetInfo;
        historyResetInfo.historyResetFlags = m_HistoryResetFlags;
        m_Stats.historyResetDescription = historyResetInfo.DescribeHistoryReset();
    }

    void ExperimentRenderPass::Render(nvrhi::IFramebuffer* framebuffer)
    {
        if (!m_Initialized || m_HasFailed || !framebuffer)
            return;

        auto device = GetDevice();
        if (m_Tools.PrepareFrame(m_PendingElapsed, m_Camera, m_RenderSize))
            RequestHistoryReset(HistoryResetReason::Manual);
        if (m_Camera.ConsumeDiscontinuity())
            RequestHistoryReset(HistoryResetReason::CameraCut);
        m_PendingElapsed = 0.f;
        UpdateJitter();
        m_Camera.SetJitter(m_Jitter);
        const auto& logicalFrame = m_Tools.replay.GetFrame();
        m_TimeSeconds = logicalFrame.time;

        m_Frame = ExperimentFrame{};
        m_Frame.commands = m_CommandList;
        m_Frame.frame.frameIndex = logicalFrame.tick;
        m_Frame.frame.submissionIndex = m_FrameCounter;
        m_Frame.frame.randomSeed = logicalFrame.seed;
        m_Frame.frame.deltaTimeSeconds = logicalFrame.delta;
        m_Frame.frame.timeSeconds = float(m_TimeSeconds);
        m_Frame.frame.viewId = prism::kPrimaryViewId;
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

        if (m_Context.temporal.services) m_Context.temporal.services->BeginFrame(m_Frame.frame);

        // 命令列表还没打开：实验可以在这里做读回与数值验证。
        m_Frame.commands = nullptr;
        if (m_Experiment)
            m_Experiment->BeginFrame(m_Context, m_Frame);
        m_Frame.commands = m_CommandList;

        m_DebugViews.BeginFrame();
        m_Metrics.BeginFrame(m_FrameCounter);

        m_CommandList->open();
        m_Services.profiler->BeginFrame(m_CommandList);

        // 资产场景需要每帧刷新动画与缓冲；程序化场景是空实现。

        m_Services.profiler->BeginScope(m_CommandList, "Frame");
        m_OutputTexture = nullptr;
        if (m_Experiment)
        {
            gpu::ScopedGpuScope scope(*m_Services.profiler, m_CommandList, m_Experiment->GetName());
            m_OutputTexture = m_Experiment->Render(m_Context, m_Frame);
            ++m_Stats.experimentFrames;
        }

        ColorSpace outputSpace = m_Context.output.colorSpace;
        if (m_OutputTexture)
        {
            const auto compared = m_Tools.comparison.Record(m_CommandList, {m_OutputTexture, outputSpace});
            m_OutputTexture = compared.texture;
            outputSpace = compared.colorSpace;
        }

        // Debug views bypass the user display transform.
        m_DebugViewActive = false;
        if (const DebugViewEntry* selected = m_DebugViews.GetSelected())
            m_DebugViewActive = ApplyDebugView(selected);


        if (m_OutputTexture)
        {
            auto status = m_Presentation.Record(m_Context, m_CommandList, framebuffer, m_OutputTexture,
                m_DebugViewActive ? ColorSpace::DisplayEncoded : outputSpace,
                m_Frame.frame.deltaTimeSeconds, m_Frame.frame.frameIndex);
            if (!status) { donut::log::error("Presentation: %s", status.ToStringWithCode().c_str()); m_HasFailed = true; RequestQuit(); }
        }

        m_Services.profiler->EndScope(m_CommandList);
        m_Services.profiler->EndFrame();
        CollectFrameMetrics();
        m_Tools.EndFrame(m_Frame.camera);
        if (m_Context.temporal.services) m_Context.temporal.services->EndFrame();
        m_CommandList->close();
        device->executeCommandList(m_CommandList);

        m_Metrics.EndFrame();

        m_HistoryResetFlags = 0;
        ++m_FrameCounter;

        HandleEndOfFrame(m_CommandList);
    }

    bool ExperimentRenderPass::ApplyDebugView(const DebugViewEntry* entry)
    {
        if (!entry || !entry->texture || !m_DebugViewPass.IsValid() || !m_Services.targets)
            return false;

        m_DebugTarget = m_Services.targets->GetOrCreate(m_DebugTargetRequest);
        if (!m_DebugTarget)
            return false;

        nvrhi::IFramebuffer* framebuffer = m_Services.targets->GetFramebuffer(m_DebugTarget, nullptr);
        if (!framebuffer)
            return false;

        gpu::ScopedGpuScope scope(*m_Services.profiler, m_CommandList, "Debug view");

        if (!m_DebugViewPass.Render(m_CommandList, entry->texture, framebuffer, entry->settings))
            return false;

        m_OutputTexture = m_DebugTarget;
        return true;
    }

    void ExperimentRenderPass::CollectFrameMetrics()
    {
        // 宿主负责的每帧统计；实验自己的数值由 context.tools.metrics 上报。
        m_Metrics.Set("cpu.frame_ms", double(m_Stats.frameTimeMs));
        m_Metrics.Set("gpu.total_ms", double(m_Services.profiler->GetTotalMilliseconds()));

        for (const gpu::GpuProfiler::ScopeTiming& timing : m_Services.profiler->GetTimings())
        {
            if (!timing.valid)
                continue;

            const std::string name = "gpu." + timing.name + "_ms";
            m_Metrics.Set(name.c_str(), double(timing.milliseconds));
        }

        if (m_DebugViewActive)
            m_Metrics.Set("debug_view_active", 1.0);
    }

    void ExperimentRenderPass::WriteMetricsIfRequested()
    {
        if (m_MetricsWritten || m_CommandLine.metricsPath.empty())
            return;

        m_MetricsWritten = m_Metrics.WriteCsv(m_CommandLine.metricsPath);
    }

    bool ExperimentRenderPass::HandleEndOfFrame(nvrhi::ICommandList* commands)
    {
        (void)commands;

        // 截图与参考图：命令列表已经提交，读回需要设备空闲。
        const bool captureRequested = !m_CommandLine.capturePath.empty();
        const bool referenceRequested =
            !m_CommandLine.referencePath.empty() || !m_CommandLine.writeReferencePath.empty();
        const bool analysisFrameReached = m_FrameCounter > uint64_t(m_CommandLine.captureFrame);

        if ((captureRequested || referenceRequested) && !m_Captured && analysisFrameReached)
        {
            m_Captured = true;

            if (m_OutputTexture)
            {
                GetDevice()->waitForIdle();

                if (captureRequested)
                {
                    const bool saved = gpu::SaveTextureToImage(
                        GetDevice(),
                        m_Services.commonPasses.get(),
                        m_Presentation.Output(),
                        nvrhi::ResourceStates::RenderTarget,
                        m_CommandLine.capturePath,
                        true);
                    if (!saved) m_AnalysisFailed = true;
                }

                if (referenceRequested)
                    AnalyzeReferenceImage();

                const auto* captured = captureRequested ? m_Presentation.Output() : m_OutputTexture;
                m_Stats.outputCaptureSize = Extent2D{ captured->getDesc().width, captured->getDesc().height };
            }
            else
            {
                donut::log::warning("Prism: capture requested but the experiment produced no output texture.");
                m_AnalysisFailed = true;
            }

            RequestQuit();
            return true;
        }

        // 性能测量：预热结束点重置统计，测量完成后写 CSV 并退出。
        if (m_CommandLine.benchFrames > 0)
        {
            const uint64_t measuredStart = uint64_t(m_CommandLine.benchWarmup) + 1;

            if (m_FrameCounter == measuredStart)
            {
                m_Metrics.Reset();
                donut::log::info("Prism: benchmark warmup finished, measuring %u frames.", m_CommandLine.benchFrames);
            }

            if (m_FrameCounter >= measuredStart + uint64_t(m_CommandLine.benchFrames))
            {
                const std::filesystem::path path = m_CommandLine.metricsPath.empty()
                    ? std::filesystem::path("prism_metrics.csv")
                    : m_CommandLine.metricsPath;

                m_MetricsWritten = m_Metrics.WriteCsv(path);

                if (m_CommandLine.metricsPath.empty())
                    donut::log::info("Prism: benchmark finished (use --metrics to choose the CSV path).");

                RequestQuit();
                return true;
            }
        }

        if (m_CommandLine.smokeTest && m_FrameCounter >= m_CommandLine.smokeTestFrames)
        {
            donut::log::info("Prism: smoke test finished %llu frames, shutting down.", (unsigned long long)m_FrameCounter);
            RequestQuit();
            return true;
        }

        return false;
    }

    void ExperimentRenderPass::AnalyzeReferenceImage()
    {
        const Result<FloatImage> current = ReadTextureAsFloat(GetDevice(), m_OutputTexture);
        if (!current.IsOk())
        {
            donut::log::error("Prism: cannot read back the output for the reference image: %s",
                current.GetStatus().ToStringWithCode().c_str());
            m_AnalysisFailed = true;
            return;
        }

        // 写出参考图（用于建立基线）
        if (!m_CommandLine.writeReferencePath.empty())
        {
            if (!SaveFloatImage(m_CommandLine.writeReferencePath, current.Value()))
                m_AnalysisFailed = true;
        }

        // 与参考图比较
        if (m_CommandLine.referencePath.empty())
            return;

        const Result<FloatImage> reference = LoadFloatImage(m_CommandLine.referencePath);
        if (!reference.IsOk())
        {
            donut::log::error("Prism: cannot load the reference image: %s",
                reference.GetStatus().ToStringWithCode().c_str());
            m_AnalysisFailed = true;
            return;
        }

        const ImageComparison comparison = CompareImages(reference.Value(), current.Value(), m_CommandLine.tolerance);

        if (!comparison.valid)
        {
            donut::log::error("Prism: the reference image comparison failed: %s", comparison.message.c_str());
            m_AnalysisFailed = true;
            return;
        }

        donut::log::info("Prism: reference comparison -- differing pixels %u, non-finite pixels %u, "
            "max |diff| %.6f, mean |diff| %.6f (tolerance %.6f).",
            comparison.differingPixels, comparison.nonFinitePixels,
            comparison.maxAbsolute, comparison.meanAbsolute, m_CommandLine.tolerance);

        if (!comparison.Passed(m_CommandLine.tolerance))
        {
            if (comparison.nonFinitePixels > 0)
                donut::log::error("Prism: the output contains %u non-finite pixels (inf / NaN).", comparison.nonFinitePixels);
            else
                donut::log::error("Prism: the output differs from the reference image beyond the tolerance.");

            m_AnalysisFailed = true;
        }
    }

    void ExperimentRenderPass::BackBufferResizing()
    {
        // 等待 GPU：命令列表返回不代表 GPU 已经用完这些资源。
        GetDevice()->waitForIdle();

        if (m_Services.resources)
            m_Services.resources->Clear();
        else if (m_Services.targets)
            m_Services.targets->Clear();

        m_Presentation.Reset();
        m_OutputTexture = nullptr;
    }

    void ExperimentRenderPass::BackBufferResized(const uint32_t width, const uint32_t height, const uint32_t sampleCount)
    {
        (void)sampleCount;

        UpdateRenderSize(Extent2D{ std::max(width, 1u), std::max(height, 1u) });

        // 接缝通知：时域实现需要丢弃按分辨率分配的历史，显示链需要重建输出尺寸相关的资源。
        if (m_Context.temporal.services)
            m_Context.temporal.services->OnRenderSizeChanged(m_RenderSize);

        if (m_Context.output.displayChain)
            m_Context.output.displayChain->OnOutputResized(m_Context.gpu, m_OutputSize);

        donut::log::info("Prism: output resized to %u x %u (render %u x %u).",
            m_OutputSize.width, m_OutputSize.height, m_RenderSize.width, m_RenderSize.height);
    }

    bool ExperimentRenderPass::KeyboardUpdate(int key, int scancode, int action, int mods)
    {
        if (m_Experiment && m_Initialized && m_Experiment->OnKey(m_Context, key, action, mods))
            return true;

        if (key == GLFW_KEY_F6 && action == GLFW_PRESS) { m_Tools.reload.Request(); return true; }
        if (m_Tools.replay.BlocksInput() && action != GLFW_RELEASE) return true;
        return m_Camera.KeyboardUpdate(key, scancode, action, mods);
    }

    bool ExperimentRenderPass::MousePosUpdate(double xpos, double ypos)
    {
        if (m_Tools.replay.BlocksInput()) return true;
        return m_Camera.MousePosUpdate(xpos, ypos);
    }

    bool ExperimentRenderPass::MouseButtonUpdate(int button, int action, int mods)
    {
        if (m_Tools.replay.BlocksInput() && action != GLFW_RELEASE) return true;
        return m_Camera.MouseButtonUpdate(button, action, mods);
    }

    bool ExperimentRenderPass::MouseScrollUpdate(double xoffset, double yoffset)
    {
        if (m_Tools.replay.BlocksInput()) return true;
        return m_Camera.MouseScrollUpdate(xoffset, yoffset);
    }

    bool ExperimentRenderPass::ShouldAnimateUnfocused()
    {
        return m_CommandLine.WantsHeadlessRun();
    }

    bool ExperimentRenderPass::ShouldRenderUnfocused()
    {
        return m_CommandLine.WantsHeadlessRun();
    }
}
