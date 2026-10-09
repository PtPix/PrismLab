#include "ExperimentHost.h"

#include "framework/tools/capture/ImageReference.h"

#include <framework/tools/capture/TextureReadback.h>

#include <donut/core/log.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace Prism::Host
{
	namespace
	{
		// Halton 序列：低差异抖动，配合时域算法使用时域累积。
		float Halton(uint32_t Index, uint32_t Base)
		{
			float Result = 0.f;
			float Fraction = 1.f;

			while (Index > 0)
			{
				Fraction /= float(Base);
				Result += Fraction * float(Index % Base);
				Index /= Base;
			}

			return Result;
		}
	} // namespace

	FExperimentRenderPass::FExperimentRenderPass(donut::app::DeviceManager* DeviceManager, FHostStats& Stats,
												 std::unique_ptr<IExperiment> Experiment, const FHostServices& Services,
												 const FCommandLine& CommandLine)
		: donut::app::IRenderPass(DeviceManager), Stats(Stats), Experiment(std::move(Experiment)), Services(Services),
		  CommandLine(CommandLine)
	{
	}

	FExperimentRenderPass::~FExperimentRenderPass()
	{
		if (bInitializeAttempted && Experiment)
		{
			GetDevice()->waitForIdle();
			Experiment->Shutdown(Context);
		}
		Tools.Replay.CaptureParameters = {};
		Tools.Replay.RestoreParameters = {};
		Experiment.reset();
	}

	FStatus FExperimentRenderPass::Initialize()
	{
		if (!Experiment)
			return FStatus::Error(EErrorCode::InvalidArgument, "no experiment was created");

		if (!Services.Device || !Services.Shaders || !Services.CommonPasses || !Services.Targets ||
			!Services.Profiler)
			return FStatus::Error(EErrorCode::NotInitialized, "the host services are incomplete");

		CommandList = Services.Device->createCommandList();
		if (!CommandList)
			return FStatus::Error(EErrorCode::DeviceError, "failed to create the host command list");

		Context.Gpu.Device = Services.Device;
		Context.Gpu.ShaderFactory = Services.ShaderFactory.get();
		Context.Gpu.CommonPasses = Services.CommonPasses.get();
		Context.Gpu.Shaders = Services.Shaders;
		Context.Gpu.Targets = Services.Targets;
		Context.Gpu.Buffers = Services.Buffers;
		Context.Gpu.Profiler = Services.Profiler;
		Context.Config = Services.Config;
		Context.AssetsDirectory = Services.AssetsDirectory;

		Context.Callbacks.RequestHistoryReset = [this](Prism::EHistoryResetReason Reason)
		{
			RequestHistoryReset(Reason);
		};
		Context.Callbacks.SetPrimaryDepthConvention = [this](Prism::EDepthConvention Convention)
		{
			if (Convention != EDepthConvention::ForwardZ0To1 && Convention != EDepthConvention::ReversedZ0To1)
				return;
			if (Camera.GetDepthConvention() != Convention)
			{
				Camera.SetDepthConvention(Convention);
				RequestHistoryReset(EHistoryResetReason::SettingsChange);
			}
		};

		Context.Callbacks.SaveTexture =
			[this](nvrhi::ITexture* Texture, const std::filesystem::path& Path, nvrhi::ResourceStates State)
		{
			if (!Texture)
				return false;

			Services.Device->waitForIdle();
			return Gpu::SaveTextureToImage(Services.Device, Services.CommonPasses.get(), Texture, State, Path, true);
		};

		Context.Callbacks.RequestQuit = [this]()
		{
			RequestQuit();
		};

		Context.Tools.DebugViews = &DebugViews;
		Context.Tools.Metrics = &Metrics;
		Context.Tools.Replay = &Tools.Replay;
		const auto ToolsStatus =
			Tools.Initialize(Services.Device, *Services.Shaders, *Services.CommonPasses, Services.ExecutablePath);
		if (!ToolsStatus)
			return ToolsStatus;
		Context.Tools.Comparison = Tools.Comparison.IsAvailable() ? &Tools.Comparison : nullptr;

		// 公共调试视图的显示 Pass（框架自带 shader；不存在时只是没有该功能，不影响实验）
		if (!DebugViewPass.Initialize(Services.Device, *Services.Shaders))
			donut::log::warning("Prism: the shared debug view pass is unavailable (prism/DebugView.hlsl).");

		DebugTargetRequest.Name = "DebugView.Output";
		DebugTargetRequest.Format = EPixelFormat::RgbA16Float;
		DebugTargetRequest.Usage = Gpu::ETextureUsage::ShaderResource | Gpu::ETextureUsage::RenderTarget;
		DebugTargetRequest.ClearColor = dm::float4(0.02f, 0.02f, 0.03f, 1.f);

		if (Services.Config)
		{

			Camera.Initialize(Services.Config->Camera);
			OutputSize =
				FExtent2D{std::max(Services.Config->Window.Width, 1u), std::max(Services.Config->Window.Height, 1u)};

			const float RenderScale = std::clamp(Services.Config->Render.RenderScale, 0.1f, 2.f);
			RenderSize = OutputSize.Scaled(RenderScale);
		}
		else
		{
			Camera.Initialize(FCameraPreset{});
			OutputSize = FExtent2D{1280, 720};
			RenderSize = OutputSize;
		}

		Services.Targets->SetRenderSize(RenderSize);
		if (Services.Buffers)
		{
			Services.Buffers->SetRenderSize(RenderSize);
		}

		// 命令行指定了调试视图：条目在实验第一次 Publish 之后才存在，索引会保留到这里生效。
		DebugViews.SetSelectedIndex(CommandLine.DebugView);

		bInitializeAttempted = true;
		const FStatus Status = Experiment->Initialize(Context);
		Stats.Scene = Context.Scene.Stats;
		Stats.SceneDescription = Context.Scene.Description;
		Metrics.SetContext(Experiment->GetName(), Stats.SceneDescription, Stats.RendererDescription, RenderSize,
						   OutputSize);
		if (Status.IsError())
			return WithContext(Status, std::string(Experiment->GetName()) + " initialization failed");

		// 相机在实验初始化之前就已经就位：把首帧的相机数据补上。
		Camera.Update(0.f, RenderSize);

		bInitialized = true;
		HistoryResetFlags = HistoryResetBit(EHistoryResetReason::FirstFrame);

		if (Context.Temporal.JitterSampleCount > 0)
			HistoryResetFlags |= HistoryResetBit(EHistoryResetReason::SettingsChange);

		donut::log::info("Prism: %s initialized (render %u x %u, output %u x %u).", Experiment->GetName(),
						 RenderSize.Width, RenderSize.Height, OutputSize.Width, OutputSize.Height);

		return FStatus::Ok();
	}

	void FExperimentRenderPass::RequestHistoryReset(Prism::EHistoryResetReason Reason)
	{
		HistoryResetFlags |= Prism::HistoryResetBit(Reason);
	}

	void FExperimentRenderPass::RequestQuit()
	{
		if (bQuitRequested)
			return;

		bQuitRequested = true;

		if (GLFWwindow* Window = GetDeviceManager()->GetWindow())
			glfwSetWindowShouldClose(Window, GLFW_TRUE);
	}

	void FExperimentRenderPass::UpdateJitter()
	{
		PreviousJitter = Jitter;

		if (Context.Temporal.JitterSampleCount == 0)
		{
			Jitter = dm::float2(0.f);
			return;
		}

		// 以抖动序列的第一个样本开始，保证历史有效时序列可复现。
		const uint32_t Index = uint32_t((Tools.Replay.GetFrame().Tick % Context.Temporal.JitterSampleCount) + 1);
		Jitter = dm::float2(Halton(Index, 2) - 0.5f, Halton(Index, 3) - 0.5f);
	}

	void FExperimentRenderPass::UpdateRenderSize(FExtent2D InOutputSize)
	{
		if (!InOutputSize.IsValid())
			InOutputSize = FExtent2D{1, 1};

		if (InOutputSize == OutputSize)
			return;

		const float RenderScale = Services.Config ? std::clamp(Services.Config->Render.RenderScale, 0.1f, 2.f) : 1.f;

		const FExtent2D NewRenderSize = InOutputSize.Scaled(RenderScale);
		if (InOutputSize != OutputSize || NewRenderSize != RenderSize)
		{
			OutputSize = InOutputSize;
			RenderSize = NewRenderSize;
			bResolutionChanged = true;

			Services.Targets->SetRenderSize(RenderSize);
			if (Services.Buffers)
			{
				Services.Buffers->SetRenderSize(RenderSize);
			}
		}
	}

	void FExperimentRenderPass::Animate(float ElapsedTimeSeconds)
	{
		if (!bInitialized || bHasFailed)
			return;

		PendingElapsed += ElapsedTimeSeconds;

		if (bResolutionChanged)
		{
			RequestHistoryReset(EHistoryResetReason::ResolutionChange);
			bResolutionChanged = false;

			if (Experiment)
				Experiment->OnResize(Context, RenderSize, OutputSize);
		}

		const float FrameTimeMs = ElapsedTimeSeconds * 1000.f;
		Stats.SmoothedFrameTimeMs =
			(Stats.SmoothedFrameTimeMs <= 0.f) ? FrameTimeMs : (Stats.SmoothedFrameTimeMs * 0.9f + FrameTimeMs * 0.1f);

		Stats.FrameTimeMs = Stats.SmoothedFrameTimeMs;
		Stats.FramesPerSecond = (Stats.SmoothedFrameTimeMs > 0.f) ? (1000.f / Stats.SmoothedFrameTimeMs) : 0.f;
		Stats.RenderSize = RenderSize;
		Stats.OutputSize = OutputSize;
		Stats.CameraPosition = Camera.GetPosition();
		Stats.CameraDirection = Camera.GetDirection();
		Stats.bFirstPerson = Camera.IsFirstPerson();
		Stats.CameraDistance = Camera.GetDistance();
		Stats.FrameIndex = FrameCounter;

		Prism::FFrameInfo HistoryResetInfo;
		HistoryResetInfo.HistoryResetFlags = HistoryResetFlags;
		Stats.HistoryResetDescription = HistoryResetInfo.DescribeHistoryReset();
	}

	void FExperimentRenderPass::Render(nvrhi::IFramebuffer* Framebuffer)
	{
		if (!bInitialized || bHasFailed || !Framebuffer)
			return;

		auto Device = GetDevice();
		if (Tools.PrepareFrame(PendingElapsed, Camera, RenderSize))
			RequestHistoryReset(EHistoryResetReason::Manual);
		if (Camera.ConsumeDiscontinuity())
			RequestHistoryReset(EHistoryResetReason::CameraCut);
		PendingElapsed = 0.f;
		UpdateJitter();
		Camera.SetJitter(Jitter);
		const auto& LogicalFrame = Tools.Replay.GetFrame();
		TimeSeconds = LogicalFrame.Time;

		Frame = FExperimentFrame{};
		Frame.Commands = CommandList;
		Frame.Frame.FrameIndex = LogicalFrame.Tick;
		Frame.Frame.SubmissionIndex = FrameCounter;
		Frame.Frame.RandomSeed = LogicalFrame.Seed;
		Frame.Frame.DeltaTimeSeconds = LogicalFrame.Delta;
		Frame.Frame.TimeSeconds = float(TimeSeconds);
		Frame.Frame.ViewId = Prism::KPrimaryViewId;
		Frame.Frame.RenderSize = RenderSize;
		Frame.Frame.OutputSize = OutputSize;
		Frame.Frame.Jitter = Jitter;
		Frame.Frame.PreviousJitter = PreviousJitter;
		Frame.Frame.HistoryResetFlags = HistoryResetFlags;
		Frame.Camera = Camera.GetCameraData();
		Frame.Camera.Jitter = Jitter;
		Frame.Camera.PreviousJitter = PreviousJitter;
		Frame.RenderSize = RenderSize;
		Frame.OutputSize = OutputSize;

		Frame.View = &Camera.GetView();
		PreviousView.SetViewport(nvrhi::Viewport(float(RenderSize.Width), float(RenderSize.Height)));
		PreviousView.SetMatrices(Frame.Camera.PreviousRaster.WorldToView, Frame.Camera.PreviousRaster.ViewToClip);
		PreviousView.SetPixelOffset(dm::float2(0.f));
		PreviousView.UpdateCache();
		Frame.PreviousView = &PreviousView;

		if (Context.Temporal.Services)
			Context.Temporal.Services->BeginFrame(Frame.Frame);

		// 命令列表还没打开：实验可以在这里做读回与数值验证。
		Frame.Commands = nullptr;
		if (Experiment)
			Experiment->BeginFrame(Context, Frame);
		Frame.Commands = CommandList;

		DebugViews.BeginFrame();
		Metrics.BeginFrame(FrameCounter);

		CommandList->open();
		Services.Profiler->BeginFrame(CommandList);

		// 资产场景需要每帧刷新动画与缓冲；程序化场景是空实现。

		Services.Profiler->BeginScope(CommandList, "Frame");
		OutputTexture = nullptr;
		if (Experiment)
		{
			Gpu::FScopedGpuScope Scope(*Services.Profiler, CommandList, Experiment->GetName());
			OutputTexture = Experiment->Render(Context, Frame);
			++Stats.ExperimentFrames;
		}

		EColorSpace OutputSpace = Context.Output.ColorSpace;
		if (OutputTexture && Tools.Comparison.IsAvailable())
		{
			const auto Compared = Tools.Comparison.Record(CommandList, {OutputTexture, OutputSpace});
			OutputTexture = Compared.Texture;
			OutputSpace = Compared.ColorSpace;
		}

		// Debug views bypass the user display transform.
		bDebugViewActive = false;
		if (const FDebugViewEntry* Selected = DebugViews.GetSelected())
			bDebugViewActive = ApplyDebugView(Selected);

		if (OutputTexture)
		{
			auto Status = Presentation.Record(Context, CommandList, Framebuffer, OutputTexture,
											  bDebugViewActive ? EColorSpace::DisplayEncoded : OutputSpace,
											  Frame.Frame.DeltaTimeSeconds, Frame.Frame.FrameIndex);
			if (!Status)
			{
				donut::log::error("Presentation: %s", Status.ToStringWithCode().c_str());
				bHasFailed = true;
				RequestQuit();
			}
		}

		Services.Profiler->EndScope(CommandList);
		Services.Profiler->EndFrame();
		CollectFrameMetrics();
		Tools.EndFrame(Frame.Camera);
		if (Context.Temporal.Services)
			Context.Temporal.Services->EndFrame();
		CommandList->close();
		Device->executeCommandList(CommandList);

		Metrics.EndFrame();

		HistoryResetFlags = 0;
		++FrameCounter;

		HandleEndOfFrame(CommandList);
	}

	bool FExperimentRenderPass::ApplyDebugView(const FDebugViewEntry* Entry)
	{
		if (!Entry || !Entry->Texture || !DebugViewPass.IsValid() || !Services.Targets)
			return false;

		DebugTarget = Services.Targets->GetOrCreate(DebugTargetRequest);
		if (!DebugTarget)
			return false;

		nvrhi::IFramebuffer* Framebuffer = Services.Targets->GetFramebuffer(DebugTarget, nullptr);
		if (!Framebuffer)
			return false;

		Gpu::FScopedGpuScope Scope(*Services.Profiler, CommandList, "Debug view");

		if (!DebugViewPass.Render(CommandList, Entry->Texture, Framebuffer, Entry->Settings))
			return false;

		OutputTexture = DebugTarget;
		return true;
	}

	void FExperimentRenderPass::CollectFrameMetrics()
	{
		// 宿主负责的每帧统计；实验自己的数值由 context.tools.metrics 上报。
		Metrics.Set("cpu.frame_ms", double(Stats.FrameTimeMs));
		Metrics.Set("gpu.total_ms", double(Services.Profiler->GetTotalMilliseconds()));

		for (const Gpu::FGpuProfiler::FScopeTiming& Timing : Services.Profiler->GetTimings())
		{
			if (!Timing.bValid)
				continue;

			const std::string Name = "gpu." + Timing.Name + "_ms";
			Metrics.Set(Name.c_str(), double(Timing.Milliseconds));
		}

		if (bDebugViewActive)
			Metrics.Set("debug_view_active", 1.0);
	}

	void FExperimentRenderPass::WriteMetricsIfRequested()
	{
		if (bMetricsWritten || CommandLine.MetricsPath.empty())
			return;

		bMetricsWritten = Metrics.WriteCsv(CommandLine.MetricsPath);
	}

	bool FExperimentRenderPass::HandleEndOfFrame(nvrhi::ICommandList* Commands)
	{
		(void)Commands;

		// 截图与参考图：命令列表已经提交，读回需要设备空闲。
		const bool bCaptureRequested = !CommandLine.CapturePath.empty();
		const bool bReferenceRequested = !CommandLine.ReferencePath.empty() || !CommandLine.WriteReferencePath.empty();
		const bool bAnalysisFrameReached = FrameCounter > uint64_t(CommandLine.CaptureFrame);

		if ((bCaptureRequested || bReferenceRequested) && !bCaptured && bAnalysisFrameReached)
		{
			bCaptured = true;

			if (OutputTexture)
			{
				GetDevice()->waitForIdle();

				if (bCaptureRequested)
				{
					const bool bSaved =
						Gpu::SaveTextureToImage(GetDevice(), Services.CommonPasses.get(), Presentation.Output(),
												nvrhi::ResourceStates::RenderTarget, CommandLine.CapturePath, true);
					if (!bSaved)
						bAnalysisFailed = true;
				}

				if (bReferenceRequested)
					AnalyzeReferenceImage();

				const auto* CapturedTexture = bCaptureRequested ? Presentation.Output() : OutputTexture;
				Stats.OutputCaptureSize =
					FExtent2D{CapturedTexture->getDesc().width, CapturedTexture->getDesc().height};
			}
			else
			{
				donut::log::warning("Prism: capture requested but the experiment produced no output texture.");
				bAnalysisFailed = true;
			}

			RequestQuit();
			return true;
		}

		// 性能测量：预热结束点重置统计，测量完成后写 CSV 并退出。
		if (CommandLine.BenchFrames > 0)
		{
			const uint64_t MeasuredStart = uint64_t(CommandLine.BenchWarmup) + 1;

			if (FrameCounter == MeasuredStart)
			{
				Metrics.Reset();
				donut::log::info("Prism: benchmark warmup finished, measuring %u frames.", CommandLine.BenchFrames);
			}

			if (FrameCounter >= MeasuredStart + uint64_t(CommandLine.BenchFrames))
			{
				const std::filesystem::path Path = CommandLine.MetricsPath.empty()
													   ? std::filesystem::path("prism_metrics.csv")
													   : CommandLine.MetricsPath;

				bMetricsWritten = Metrics.WriteCsv(Path);

				if (CommandLine.MetricsPath.empty())
					donut::log::info("Prism: benchmark finished (use --metrics to choose the CSV path).");

				RequestQuit();
				return true;
			}
		}



		return false;
	}

	void FExperimentRenderPass::AnalyzeReferenceImage()
	{
		const TResult<FFloatImage> Current = ReadTextureAsFloat(GetDevice(), OutputTexture);
		if (!Current.IsOk())
		{
			donut::log::error("Prism: cannot read back the output for the reference image: %s",
							  Current.GetStatus().ToStringWithCode().c_str());
			bAnalysisFailed = true;
			return;
		}

		// 写出参考图（用于建立基线）
		if (!CommandLine.WriteReferencePath.empty())
		{
			if (!SaveFloatImage(CommandLine.WriteReferencePath, Current.GetValue()))
				bAnalysisFailed = true;
		}

		// 与参考图比较
		if (CommandLine.ReferencePath.empty())
			return;

		const TResult<FFloatImage> Reference = LoadFloatImage(CommandLine.ReferencePath);
		if (!Reference.IsOk())
		{
			donut::log::error("Prism: cannot load the reference image: %s",
							  Reference.GetStatus().ToStringWithCode().c_str());
			bAnalysisFailed = true;
			return;
		}

		const FImageComparison Comparison =
			CompareImages(Reference.GetValue(), Current.GetValue(), CommandLine.Tolerance);

		if (!Comparison.bValid)
		{
			donut::log::error("Prism: the reference image comparison failed: %s", Comparison.Message.c_str());
			bAnalysisFailed = true;
			return;
		}

		donut::log::info("Prism: reference comparison -- differing pixels %u, non-finite pixels %u, "
						 "max |diff| %.6f, mean |diff| %.6f (tolerance %.6f).",
						 Comparison.DifferingPixels, Comparison.NonFinitePixels, Comparison.MaxAbsolute,
						 Comparison.MeanAbsolute, CommandLine.Tolerance);

		if (!Comparison.Passed(CommandLine.Tolerance))
		{
			if (Comparison.NonFinitePixels > 0)
				donut::log::error("Prism: the output contains %u non-finite pixels (inf / NaN).",
								  Comparison.NonFinitePixels);
			else
				donut::log::error("Prism: the output differs from the reference image beyond the tolerance.");

			bAnalysisFailed = true;
		}
	}

	void FExperimentRenderPass::BackBufferResizing()
	{
		// 等待 GPU：命令列表返回不代表 GPU 已经用完这些资源。
		GetDevice()->waitForIdle();

		Services.Targets->Clear();
		if (Services.Buffers)
		{
			Services.Buffers->Clear();
		}

		Presentation.Reset();
		OutputTexture = nullptr;
	}

	void FExperimentRenderPass::BackBufferResized(const uint32_t Width, const uint32_t Height,
												  const uint32_t SampleCount)
	{
		(void)SampleCount;

		UpdateRenderSize(FExtent2D{std::max(Width, 1u), std::max(Height, 1u)});

		// 接缝通知：时域实现需要丢弃按分辨率分配的历史，显示链需要重建输出尺寸相关的资源。
		if (Context.Temporal.Services)
			Context.Temporal.Services->OnRenderSizeChanged(RenderSize);

		if (Context.Output.DisplayChain)
			Context.Output.DisplayChain->OnOutputResized(Context.Gpu, OutputSize);

		donut::log::info("Prism: output resized to %u x %u (render %u x %u).", OutputSize.Width, OutputSize.Height,
						 RenderSize.Width, RenderSize.Height);
	}

	bool FExperimentRenderPass::KeyboardUpdate(int Key, int Scancode, int Action, int Mods)
	{
		if (Experiment && bInitialized && Experiment->OnKey(Context, Key, Action, Mods))
			return true;

		if (Key == GLFW_KEY_F6 && Action == GLFW_PRESS)
		{
			Tools.Reload.Request();
			return true;
		}
		if (Tools.Replay.BlocksInput() && Action != GLFW_RELEASE)
			return true;
		return Camera.KeyboardUpdate(Key, Scancode, Action, Mods);
	}

	bool FExperimentRenderPass::MousePosUpdate(double Xpos, double Ypos)
	{
		if (Tools.Replay.BlocksInput())
			return true;
		return Camera.MousePosUpdate(Xpos, Ypos);
	}

	bool FExperimentRenderPass::MouseButtonUpdate(int Button, int Action, int Mods)
	{
		if (Tools.Replay.BlocksInput() && Action != GLFW_RELEASE)
			return true;
		return Camera.MouseButtonUpdate(Button, Action, Mods);
	}

	bool FExperimentRenderPass::MouseScrollUpdate(double Xoffset, double Yoffset)
	{
		if (Tools.Replay.BlocksInput())
			return true;
		return Camera.MouseScrollUpdate(Xoffset, Yoffset);
	}

	bool FExperimentRenderPass::ShouldAnimateUnfocused()
	{
		return CommandLine.WantsHeadlessRun();
	}

	bool FExperimentRenderPass::ShouldRenderUnfocused()
	{
		return CommandLine.WantsHeadlessRun();
	}
} // namespace Prism::Host
