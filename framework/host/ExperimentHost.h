#pragma once

// Host layer: the frame driver.
//
// ExperimentRenderPass owns the per-frame state (frame info, camera, profiler, render target pool) and calls
// the experiment once per frame. Everything an experiment should not have to write lives here: command list lifetime,
// blit to the swap chain, resize handling, jitter sequence, screenshots, smoke test and GPU timing.

#include "CommandLine.h"
#include "DebugViewRegistry.h"
#include "Experiment.h"
#include "Metrics.h"

#include <framework/donut/CameraController.h>
#include <framework/donut/HostConfig.h>
#include <framework/donut/SceneHost.h>

#include <framework/nvrhi/DebugView.h>
#include <framework/nvrhi/GpuProfiler.h>
#include <framework/nvrhi/RenderTargetPool.h>

#include <donut/app/DeviceManager.h>
#include <donut/engine/BindingCache.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/ShaderFactory.h>

#include <memory>
#include <string>

namespace prism::host
{
    struct HostStats
    {
        Extent2D renderSize;
        Extent2D outputSize;

        float frameTimeMs = 0.f;
        float framesPerSecond = 0.f;
        float smoothedFrameTimeMs = 0.f;

        dm::float3 cameraPosition = dm::float3(0.f);
        dm::float3 cameraDirection = dm::float3(0.f, 0.f, 1.f);
        bool firstPerson = true;
        float cameraDistance = 6.f;

        adapter::SceneStats scene;
        std::string sceneDescription = "(none)";
        std::string configDescription = "(built-in defaults)";
        std::string rendererDescription;

        uint64_t frameIndex = 0;
        uint64_t experimentFrames = 0;
        std::string historyResetDescription = "none";

        Extent2D outputCaptureSize;
    };

    // 应用装配好、由宿主长期持有的服务；实验只借用。
    struct HostServices
    {
        nvrhi::IDevice* device = nullptr;
        std::shared_ptr<donut::engine::ShaderFactory> shaderFactory;
        std::shared_ptr<donut::engine::CommonRenderPasses> commonPasses;
        gpu::ShaderLibrary* shaders = nullptr;
        gpu::RenderTargetPool* targets = nullptr;
        gpu::BufferPool* buffers = nullptr;
        gpu::ResourceTable* resources = nullptr;
        gpu::GpuProfiler* profiler = nullptr;
        pipeline::SceneForwardPipeline* scenePipeline = nullptr;
        adapter::SceneHost* sceneHost = nullptr;
        const adapter::HostConfig* config = nullptr;
        std::filesystem::path assetsDirectory;
    };

    class ExperimentRenderPass final : public donut::app::IRenderPass
    {
    public:
        ExperimentRenderPass(
            donut::app::DeviceManager* deviceManager,
            HostStats& stats,
            std::unique_ptr<Experiment> experiment,
            const HostServices& services,
            const CommandLine& commandLine);

        ~ExperimentRenderPass() override;

        Status Initialize();

        [[nodiscard]] ExperimentContext& GetContext() { return m_Context; }
        [[nodiscard]] adapter::CameraController& GetCamera() { return m_Camera; }
        [[nodiscard]] Experiment* GetExperiment() { return m_Experiment.get(); }
        [[nodiscard]] const CommandLine& GetCommandLine() const { return m_CommandLine; }
        [[nodiscard]] bool HasFailed() const { return m_HasFailed; }
        [[nodiscard]] bool WasCaptured() const { return m_Captured; }

        // 面板与退出码用的状态
        [[nodiscard]] DebugViewRegistry& GetDebugViews() { return m_DebugViews; }
        [[nodiscard]] Metrics& GetMetrics() { return m_Metrics; }
        [[nodiscard]] bool IsDebugViewActive() const { return m_DebugViewActive; }

        // 参考图比较失败、指标写不出等"运行时分析失败"，与实验失败一起决定退出码。
        [[nodiscard]] bool HasAnalysisFailure() const { return m_AnalysisFailed; }

        // 消息循环结束后由应用调用：把指标 CSV 写出来（--metrics / --bench）。
        void WriteMetricsIfRequested();

        void RequestHistoryReset(prism::HistoryResetReason reason);

        // 结束消息循环（冒烟测试、截图完成或实验主动结束）
        void RequestQuit();

        // IRenderPass
        void Animate(float elapsedTimeSeconds) override;
        void Render(nvrhi::IFramebuffer* framebuffer) override;
        void BackBufferResizing() override;
        void BackBufferResized(uint32_t width, uint32_t height, uint32_t sampleCount) override;

        bool KeyboardUpdate(int key, int scancode, int action, int mods) override;
        bool MousePosUpdate(double xpos, double ypos) override;
        bool MouseButtonUpdate(int button, int action, int mods) override;
        bool MouseScrollUpdate(double xoffset, double yoffset) override;

        bool ShouldAnimateUnfocused() override;
        bool ShouldRenderUnfocused() override;

    private:
        void UpdateJitter();
        void UpdateRenderSize(Extent2D outputSize);
        bool HandleEndOfFrame(nvrhi::ICommandList* commands);
        bool ApplyDebugView(const DebugViewEntry* entry);
        void CollectFrameMetrics();
        void AnalyzeReferenceImage();

        HostStats& m_Stats;
        std::unique_ptr<Experiment> m_Experiment;
        HostServices m_Services;
        CommandLine m_CommandLine;

        ExperimentContext m_Context;
        ExperimentFrame m_Frame;

        adapter::CameraController m_Camera;
        donut::engine::PlanarView m_PreviousView;

        nvrhi::CommandListHandle m_CommandList;
        std::unique_ptr<donut::engine::BindingCache> m_BindingCache;
        nvrhi::ITexture* m_OutputTexture = nullptr;

        Extent2D m_RenderSize;
        Extent2D m_OutputSize;

        dm::float2 m_Jitter = dm::float2(0.f);
        dm::float2 m_PreviousJitter = dm::float2(0.f);

        uint32_t m_HistoryResetFlags = 0;
        uint64_t m_FrameCounter = 0;
        double m_TimeSeconds = 0.0;

        bool m_Initialized = false;
        bool m_HasFailed = false;
        bool m_Captured = false;
        bool m_QuitRequested = false;
        bool m_ResolutionChanged = false;

        // 公共调试视图：实验登记中间结果，宿主面板选择，选中时替换实验输出显示
        gpu::DebugViewPass m_DebugViewPass;
        DebugViewRegistry m_DebugViews;
        gpu::TextureRequest m_DebugTargetRequest;
        nvrhi::ITexture* m_DebugTarget = nullptr;
        bool m_DebugViewActive = false;

        // 指标与运行分析
        Metrics m_Metrics;
        bool m_MetricsWritten = false;
        bool m_AnalysisFailed = false;
    };
}
