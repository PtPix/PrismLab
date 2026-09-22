#include "ExperimentHost.h"

#include "ImageReference.h"

#include <framework/nvrhi/TextureReadback.h>

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

    ExperimentRenderPass::~ExperimentRenderPass() = default;

    Status ExperimentRenderPass::Initialize()
    {
        if (!m_Experiment)
            return Status::Error(ErrorCode::InvalidArgument, "no experiment was created");

        if (!m_Services.device || !m_Services.shaders || !m_Services.targets || !m_Services.profiler)
            return Status::Error(ErrorCode::NotInitialized, "the host services are incomplete");

        m_CommandList = m_Services.device->createCommandList();
        m_BindingCache = std::make_unique<donut::engine::BindingCache>(m_Services.device);

        m_Context.gpu.device = m_Services.device;
        m_Context.gpu.shaderFactory = m_Services.shaderFactory.get();
        m_Context.gpu.commonPasses = m_Services.commonPasses.get();
        m_Context.gpu.shaders = m_Services.shaders;
        m_Context.gpu.targets = m_Services.targets;
        m_Context.gpu.buffers = m_Services.buffers;
        m_Context.gpu.resources = m_Services.resources;
        m_Context.gpu.profiler = m_Services.profiler;
        m_Context.scene.pipeline = m_Services.scenePipeline;
        m_Context.scene.data = m_Services.sceneHost ? &m_Services.sceneHost->GetData() : nullptr;
        m_Context.config = m_Services.config;
        m_Context.assetsDirectory = m_Services.assetsDirectory;

        m_Context.output.callbacks.requestHistoryReset = [this](prism::HistoryResetReason reason)
        {
            RequestHistoryReset(reason);
        };

        m_Context.output.callbacks.saveTexture = [this](nvrhi::ITexture* texture, const std::filesystem::path& path, nvrhi::ResourceStates state)
        {
            if (!texture)
                return false;

            m_Services.device->waitForIdle();
            return gpu::SaveTextureToImage(m_Services.device, m_Services.commonPasses.get(), texture, state, path, true);
        };

        m_Context.output.callbacks.requestQuit = [this]()
        {
            RequestQuit();
        };

        m_Context.output.debugViews = &m_DebugViews;
        m_Context.output.metrics = &m_Metrics;

        // 公共调试视图的显示 Pass（框架自带 shader；不存在时只是没有该功能，不影响实验）
        if (!m_DebugViewPass.Initialize(m_Services.device, *m_Services.shaders))
            donut::log::warning("Prism: the shared debug view pass is unavailable (prism/DebugView.hlsl).");

        m_DebugTargetRequest.name = "DebugView.Output";
        m_DebugTargetRequest.format = PixelFormat::RGBA16_FLOAT;
        m_DebugTargetRequest.usage = gpu::TextureUsage::ShaderResource | gpu::TextureUsage::RenderTarget;
        m_DebugTargetRequest.clearColor = dm::float4(0.02f, 0.02f, 0.03f, 1.f);

        if (m_Services.config)
        {
            m_Context.scene.ambientTop = dm::float3(m_Services.config->lighting.ambientIntensity);
            m_Context.scene.ambientBottom = dm::float3(m_Services.config->lighting.ambientIntensity * 0.6f);

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

        if (m_Services.resources)
            m_Services.resources->SetRenderSize(m_RenderSize);
        else if (m_Services.targets)
            m_Services.targets->SetRenderSize(m_RenderSize);

        m_Metrics.SetContext(
            m_Experiment->GetName(),
            m_Stats.sceneDescription,
            m_Stats.rendererDescription,
            m_RenderSize,
            m_OutputSize);

        // 命令行指定了调试视图：条目在实验第一次 Publish 之后才存在，索引会保留到这里生效。
        m_DebugViews.SetSelectedIndex(m_CommandLine.debugView);

        const Status status = m_Experiment->Initialize(m_Context);
        if (status.IsError())
            return WithContext(status, std::string(m_Experiment->GetName()) + " initialization failed");

        // 相机在实验初始化之前就已经就位：把首帧的相机数据补上。
        m_Camera.Update(0.f, m_RenderSize);

        m_Initialized = true;
        m_HistoryResetFlags = HistoryResetBit(HistoryResetReason::FirstFrame);

        if (m_Context.output.jitterSampleCount > 0)
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

        if (m_Context.output.jitterSampleCount == 0)
        {
            m_Jitter = dm::float2(0.f);
            return;
        }

        // 以抖动序列的第一个样本开始，保证历史有效时序列可复现。
        const uint32_t index = uint32_t((m_FrameCounter % m_Context.output.jitterSampleCount) + 1);
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

            m_Services.targets->SetRenderSize(m_RenderSize);
        }
    }

    void ExperimentRenderPass::Animate(float elapsedTimeSeconds)
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

            if (m_Experiment)
                m_Experiment->OnResize(m_Context, m_RenderSize, m_OutputSize);
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

        prism::FrameInfo historyResetInfo;
        historyResetInfo.historyResetFlags = m_HistoryResetFlags;
        m_Stats.historyResetDescription = historyResetInfo.DescribeHistoryReset();
    }

    void ExperimentRenderPass::Render(nvrhi::IFramebuffer* framebuffer)
    {
        if (!m_Initialized || m_HasFailed || !framebuffer)
            return;

        auto device = GetDevice();

        m_Frame = ExperimentFrame{};
        m_Frame.commands = m_CommandList;
        m_Frame.frame.frameIndex = m_FrameCounter;
        m_Frame.frame.deltaTimeSeconds = float(GetDeviceManager()->GetAverageFrameTimeSeconds());
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
        if (m_Services.sceneHost)
            m_Services.sceneHost->Update(m_CommandList, uint32_t(m_FrameCounter));

        m_OutputTexture = nullptr;
        if (m_Experiment)
        {
            gpu::ScopedGpuScope scope(*m_Services.profiler, m_CommandList, m_Experiment->GetName());
            m_OutputTexture = m_Experiment->Render(m_Context, m_Frame);
            ++m_Stats.experimentFrames;
        }

        // 调试视图：选中的中间结果替换实验输出（调试图已经是显示空间的数据）
        m_DebugViewActive = false;
        if (const DebugViewEntry* selected = m_DebugViews.GetSelected())
            m_DebugViewActive = ApplyDebugView(selected);

        m_Services.profiler->EndFrame();
        CollectFrameMetrics();

        if (m_OutputTexture)
        {
            // 显示链接缝：实现了就交给它（曝光 / Bloom / Tone Mapping），否则退回直接 blit。
            if (m_Context.output.displayChain)
            {
                DisplayInput displayInput;
                displayInput.sceneColor = m_OutputTexture;
                displayInput.colorSpace = m_DebugViewActive
                    ? prism::ColorSpace::DisplayEncoded
                    : m_Context.output.colorSpace;
                displayInput.outputTarget = framebuffer;
                displayInput.outputSize = m_OutputSize;
                displayInput.deltaTimeSeconds = m_Frame.frame.deltaTimeSeconds;
                displayInput.frameIndex = m_Frame.frame.frameIndex;

                const Status displayStatus = m_Context.output.displayChain->Record(m_Context, m_CommandList, displayInput);
                if (displayStatus.IsError())
                {
                    // 不静默替换算法：报错并关闭显示链，本帧起退回宿主的直接 blit。
                    donut::log::error("Prism: the display chain failed, falling back to a direct blit: %s",
                        displayStatus.ToStringWithCode().c_str());
                    m_Context.output.displayChain = nullptr;
                }
            }

            if (!m_Context.output.displayChain)
                m_Services.commonPasses->BlitTexture(m_CommandList, framebuffer, m_OutputTexture, m_BindingCache.get());
        }

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
        // 宿主负责的每帧统计；实验自己的数值由 context.output.metrics 上报。
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
                    gpu::SaveTextureToImage(
                        GetDevice(),
                        m_Services.commonPasses.get(),
                        m_OutputTexture,
                        nvrhi::ResourceStates::RenderTarget,
                        m_CommandLine.capturePath,
                        true);
                }

                if (referenceRequested)
                    AnalyzeReferenceImage();

                m_Stats.outputCaptureSize = Extent2D{ m_OutputTexture->getDesc().width, m_OutputTexture->getDesc().height };
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

        m_BindingCache->Clear();
        m_OutputTexture = nullptr;
    }

    void ExperimentRenderPass::BackBufferResized(const uint32_t width, const uint32_t height, const uint32_t sampleCount)
    {
        (void)sampleCount;

        UpdateRenderSize(Extent2D{ std::max(width, 1u), std::max(height, 1u) });

        // 接缝通知：时域实现需要丢弃按分辨率分配的历史，显示链需要重建输出尺寸相关的资源。
        if (m_Context.output.temporal)
            m_Context.output.temporal->OnRenderSizeChanged(m_RenderSize);

        if (m_Context.output.displayChain)
            m_Context.output.displayChain->OnOutputResized(m_Context, m_OutputSize);

        donut::log::info("Prism: output resized to %u x %u (render %u x %u).",
            m_OutputSize.width, m_OutputSize.height, m_RenderSize.width, m_RenderSize.height);
    }

    bool ExperimentRenderPass::KeyboardUpdate(int key, int scancode, int action, int mods)
    {
        if (m_Experiment && m_Initialized && m_Experiment->OnKey(m_Context, key, action, mods))
            return true;

        return m_Camera.KeyboardUpdate(key, scancode, action, mods);
    }

    bool ExperimentRenderPass::MousePosUpdate(double xpos, double ypos)
    {
        return m_Camera.MousePosUpdate(xpos, ypos);
    }

    bool ExperimentRenderPass::MouseButtonUpdate(int button, int action, int mods)
    {
        return m_Camera.MouseButtonUpdate(button, action, mods);
    }

    bool ExperimentRenderPass::MouseScrollUpdate(double xoffset, double yoffset)
    {
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
