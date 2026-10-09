#pragma once

// Host layer: the frame driver.
//
// ExperimentRenderPass owns the per-frame state (frame info, camera, profiler, render target pool) and calls
// the experiment once per frame. Everything an experiment should not have to write lives here: command list lifetime,
// blit to the swap chain, resize handling, jitter sequence, screenshots, smoke test and GPU timing.

#include "CommandLine.h"
#include "framework/tools/inspection/DebugViewRegistry.h"
#include "Experiment.h"
#include "framework/tools/metrics/Metrics.h"
#include "ExperimentTools.h"
#include "FramePresentation.h"

#include <framework/adapters/donut/CameraController.h>
#include <framework/app/HostConfig.h>

#include <framework/tools/inspection/DebugView.h>
#include <framework/render/profiling/GpuProfiler.h>
#include <framework/render/resources/TextureCache.h>

#include <donut/app/DeviceManager.h>
#include <donut/engine/BindingCache.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/ShaderFactory.h>

#include <memory>
#include <string>

namespace Prism::Host
{
	struct FHostStats
	{
		FExtent2D RenderSize;
		FExtent2D OutputSize;

		float FrameTimeMs = 0.f;
		float FramesPerSecond = 0.f;
		float SmoothedFrameTimeMs = 0.f;

		dm::float3 CameraPosition = dm::float3(0.f);
		dm::float3 CameraDirection = dm::float3(0.f, 0.f, 1.f);
		bool bFirstPerson = true;
		float CameraDistance = 6.f;

		FSceneStats Scene;
		std::string SceneDescription = "(none)";
		std::string ConfigDescription = "(built-in defaults)";
		std::string RendererDescription;

		uint64_t FrameIndex = 0;
		uint64_t ExperimentFrames = 0;
		std::string HistoryResetDescription = "none";

		FExtent2D OutputCaptureSize;
	};

	// 应用装配好、由宿主长期持有的服务；实验只借用。
	struct FHostServices
	{
		std::filesystem::path ExecutablePath;
		nvrhi::IDevice* Device = nullptr;
		std::shared_ptr<donut::engine::ShaderFactory> ShaderFactory;
		std::shared_ptr<donut::engine::CommonRenderPasses> CommonPasses;
		Gpu::FShaderLibrary* Shaders = nullptr;
		Gpu::FTextureCache* Targets = nullptr;
		Gpu::FBufferCache* Buffers = nullptr;
		Gpu::FGpuProfiler* Profiler = nullptr;
		const Host::FHostConfig* Config = nullptr;
		std::filesystem::path AssetsDirectory;
	};

	class FExperimentRenderPass final : public donut::app::IRenderPass
	{
	  public:
		FExperimentRenderPass(donut::app::DeviceManager* DeviceManager, FHostStats& Stats,
							  std::unique_ptr<IExperiment> Experiment, const FHostServices& Services,
							  const FCommandLine& CommandLine);

		~FExperimentRenderPass() override;

		FStatus Initialize();

		[[nodiscard]] FExperimentContext& GetContext()
		{
			return Context;
		}
		[[nodiscard]] Adapter::FCameraController& GetCamera()
		{
			return Camera;
		}
		[[nodiscard]] IExperiment* GetExperiment()
		{
			return Experiment.get();
		}
		[[nodiscard]] const FCommandLine& GetCommandLine() const
		{
			return CommandLine;
		}
		[[nodiscard]] bool HasFailed() const
		{
			return bHasFailed;
		}
		[[nodiscard]] bool WasCaptured() const
		{
			return bCaptured;
		}
		FExperimentTools& GetTools()
		{
			return Tools;
		}

		// 面板与退出码用的状态
		[[nodiscard]] FDebugViewRegistry& GetDebugViews()
		{
			return DebugViews;
		}
		[[nodiscard]] FMetrics& GetMetrics()
		{
			return Metrics;
		}
		[[nodiscard]] bool IsDebugViewActive() const
		{
			return bDebugViewActive;
		}

		// 参考图比较失败、指标写不出等"运行时分析失败"，与实验失败一起决定退出码。
		[[nodiscard]] bool HasAnalysisFailure() const
		{
			return bAnalysisFailed;
		}

		// 消息循环结束后由应用调用：把指标 CSV 写出来（--metrics / --bench）。
		void WriteMetricsIfRequested();

		void RequestHistoryReset(Prism::EHistoryResetReason Reason);

		// 结束消息循环（冒烟测试、截图完成或实验主动结束）
		void RequestQuit();

		// IRenderPass
		void Animate(float ElapsedTimeSeconds) override;
		void Render(nvrhi::IFramebuffer* Framebuffer) override;
		void BackBufferResizing() override;
		void BackBufferResized(uint32_t Width, uint32_t Height, uint32_t SampleCount) override;

		bool KeyboardUpdate(int Key, int Scancode, int Action, int Mods) override;
		bool MousePosUpdate(double Xpos, double Ypos) override;
		bool MouseButtonUpdate(int Button, int Action, int Mods) override;
		bool MouseScrollUpdate(double Xoffset, double Yoffset) override;

		bool ShouldAnimateUnfocused() override;
		bool ShouldRenderUnfocused() override;

	  private:
		void UpdateJitter();
		void UpdateRenderSize(FExtent2D OutputSize);
		bool HandleEndOfFrame(nvrhi::ICommandList* Commands);
		bool ApplyDebugView(const FDebugViewEntry* Entry);
		void CollectFrameMetrics();
		void AnalyzeReferenceImage();

		FHostStats& Stats;
		std::unique_ptr<IExperiment> Experiment;
		FHostServices Services;
		FCommandLine CommandLine;

		FExperimentContext Context;
		FExperimentFrame Frame;
		FExperimentTools Tools;
		FFramePresentation Presentation;

		Adapter::FCameraController Camera;
		donut::engine::PlanarView PreviousView;

		nvrhi::CommandListHandle CommandList;
		nvrhi::ITexture* OutputTexture = nullptr;

		FExtent2D RenderSize;
		FExtent2D OutputSize;

		dm::float2 Jitter = dm::float2(0.f);
		dm::float2 PreviousJitter = dm::float2(0.f);

		uint32_t HistoryResetFlags = 0;
		uint64_t FrameCounter = 0;
		double TimeSeconds = 0.0;
		float PendingElapsed = 0.f;

		bool bInitialized = false;
		bool bInitializeAttempted = false;
		bool bHasFailed = false;
		bool bCaptured = false;
		bool bQuitRequested = false;
		bool bResolutionChanged = false;

		// 公共调试视图：实验登记中间结果，宿主面板选择，选中时替换实验输出显示
		Gpu::FDebugViewPass DebugViewPass;
		FDebugViewRegistry DebugViews;
		Gpu::FTextureRequest DebugTargetRequest;
		nvrhi::ITexture* DebugTarget = nullptr;
		bool bDebugViewActive = false;

		// 指标与运行分析
		FMetrics Metrics;
		bool bMetricsWritten = false;
		bool bAnalysisFailed = false;
	};
} // namespace Prism::Host
