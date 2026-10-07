#include "Application.h"

#include "CommandLine.h"
#include "ExperimentHost.h"
#include "UiOverlay.h"

#include <framework/app/HostConfig.h>

#include <framework/render/profiling/GpuProfiler.h>
#include <framework/render/resources/TextureCache.h>
#include <framework/render/shaders/ShaderLibrary.h>

#include <donut/app/ApplicationBase.h>
#include <donut/app/DeviceManager.h>
#include <donut/core/log.h>
#include <donut/core/vfs/VFS.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/ShaderFactory.h>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

namespace Prism::Host
{
	namespace
	{
		// GUI 应用没有控制台，把日志同时写进可执行文件旁边的文件。
		void LogToFile(donut::log::Severity, const char* Message)
		{
			static const std::filesystem::path LogPath = donut::app::GetDirectoryWithExecutable() / "prism.log";

			FILE* File = nullptr;
			if (_wfopen_s(&File, LogPath.c_str(), L"a") == 0 && File != nullptr)
			{
				fprintf(File, "%s\n", Message);
				fclose(File);
			}
		}

		// NVRHI / D3D12 的校验信息进入同一个日志，避免"设备丢失但日志空白"。
		class FLogMessageCallback final : public nvrhi::IMessageCallback
		{
		  public:
			void message(nvrhi::MessageSeverity Severity, const char* MessageText) override
			{
				switch (Severity)
				{
					case nvrhi::MessageSeverity::Error:
						donut::log::error("[nvrhi] %s", MessageText);
						break;
					case nvrhi::MessageSeverity::Warning:
						donut::log::warning("[nvrhi] %s", MessageText);
						break;
					default:
						donut::log::info("[nvrhi] %s", MessageText);
						break;
				}
			}
		};

		// 从可执行文件目录向上寻找仓库根的 assets 目录；找不到就返回空路径。
		std::filesystem::path FindAssetsDirectory()
		{
			std::filesystem::path Directory = donut::app::GetDirectoryWithExecutable();

			for (int Depth = 0; Depth < 6 && !Directory.empty(); ++Depth)
			{
				const std::filesystem::path Candidate = Directory / "assets";
				if (std::filesystem::is_directory(Candidate))
					return Candidate;

				const std::filesystem::path Parent = Directory.parent_path();
				if (Parent == Directory)
					break;

				Directory = Parent;
			}

			return std::filesystem::path();
		}
	} // namespace

	int RunApplication(std::unique_ptr<IExperiment> Experiment, int Argc, char** Argv)
	{
		donut::log::EnableOutputToMessageBox(false);
		donut::log::SetCallback(&LogToFile);

		const FCommandLine CommandLine = ParseCommandLine(Argc, Argv);
		if (CommandLine.bShowHelp)
		{
			donut::log::info("Prism\n%s", GetCommandLineUsage().c_str());
			return 0;
		}

		const std::string ExperimentName = Experiment ? Experiment->GetName() : "Experiment";
		donut::log::info("Prism: starting %s", ExperimentName.c_str());

		// --- configuration -----------------------------------------------------
		const std::filesystem::path ExecutablePath =
			(Argc > 0 && Argv) ? std::filesystem::path(Argv[0]) : std::filesystem::path();

		Host::FHostConfig Config = Host::LoadHostConfig(CommandLine.ConfigPath, ExecutablePath);

		if (!CommandLine.SceneSource.empty())
			Config.Scene.Source = CommandLine.SceneSource;

		if (!CommandLine.SceneAsset.empty())
			Config.Scene.Asset = CommandLine.SceneAsset;

		if (CommandLine.Width > 0)
			Config.Window.Width = CommandLine.Width;

		if (CommandLine.Height > 0)
			Config.Window.Height = CommandLine.Height;

		if (CommandLine.bDisableVsync)
			Config.Window.bVsync = false;

		if (CommandLine.bDisableGpuTiming)
			Config.Render.bEnableGpuTiming = false;

		if (!Config.Scene.Asset.empty())
			Config.Scene.Asset = Host::ResolveAssetPath(Config.Scene.Asset).string();

		const std::string ApplicationName = "Prism | " + ExperimentName;

		// --- device and swap chain --------------------------------------------
		donut::app::DeviceCreationParameters DeviceParams;
		DeviceParams.backBufferWidth = Config.Window.Width;
		DeviceParams.backBufferHeight = Config.Window.Height;
		DeviceParams.vsyncEnabled = Config.Window.bVsync;
		DeviceParams.swapChainBufferCount = 3;
		DeviceParams.startMaximized = false;
		DeviceParams.swapChainSampleCount = 1;
		DeviceParams.depthBufferFormat = nvrhi::Format::UNKNOWN; // the UI layer does not need a depth buffer
#ifdef _DEBUG
		DeviceParams.enableDebugRuntime = true;
		DeviceParams.enableNvrhiValidationLayer = true;
#endif

		FLogMessageCallback MessageCallback;
		DeviceParams.messageCallback = &MessageCallback;

		std::unique_ptr<donut::app::DeviceManager> DeviceManager(
			donut::app::DeviceManager::Create(nvrhi::GraphicsAPI::D3D12));

		struct FDeviceLifetime
		{
			donut::app::DeviceManager* Manager;
			~FDeviceLifetime()
			{
				if (Manager)
					Manager->Shutdown();
			}
		} DeviceLifetime{DeviceManager.get()};

		if (!DeviceManager || !DeviceManager->CreateWindowDeviceAndSwapChain(DeviceParams, ApplicationName.c_str()))
		{
			donut::log::fatal("Prism: failed to create the D3D12 device or swap chain.");
			return 1;
		}

		DeviceManager->SetInformativeWindowTitle(ApplicationName.c_str());

		nvrhi::IDevice* Device = DeviceManager->GetDevice();

		// --- shader mounts ----------------------------------------------------
		// /shaders/donut      : Donut 框架 shader（blit、forward shading、UI）
		// /shaders/prism: package-owned Prism bytecode.
		const std::filesystem::path ShaderTypeName = donut::app::GetShaderTypeName(DeviceManager->GetGraphicsAPI());
		const std::filesystem::path ShaderRoot = donut::app::GetDirectoryWithExecutable() / "shaders";

		auto RootFileSystem = std::make_shared<donut::vfs::RootFileSystem>();
		RootFileSystem->mount("/shaders/donut", ShaderRoot / "framework" / ShaderTypeName);
		RootFileSystem->mount("/shaders/prism", ShaderRoot / "prism" / ShaderTypeName);

		auto ShaderFactory = std::make_shared<donut::engine::ShaderFactory>(Device, RootFileSystem, "/shaders");
		auto CommonPasses = std::make_shared<donut::engine::CommonRenderPasses>(Device, ShaderFactory);

		// --- shared services --------------------------------------------------
		Gpu::FShaderLibrary ShaderLibrary(Device, ShaderFactory);
		Gpu::FTextureCache RenderTargets(Device);
		Gpu::FBufferCache Buffers(Device);
		Gpu::FResourceTable Resources(RenderTargets, Buffers);
		Gpu::FGpuProfiler Profiler(Device);
		Profiler.SetEnabled(Config.Render.bEnableGpuTiming);

		// --- host state -------------------------------------------------------
		FHostStats Stats;
		Stats.RendererDescription = DeviceManager->GetRendererString();
		Stats.ConfigDescription =
			Config.bLoadedFromFile ? Config.SourcePath.string() : std::string("(built-in defaults)");

		FHostServices Services;
		Services.ExecutablePath = std::filesystem::absolute(ExecutablePath);
		Services.Device = Device;
		Services.ShaderFactory = ShaderFactory;
		Services.CommonPasses = CommonPasses;
		Services.Shaders = &ShaderLibrary;
		Services.Targets = &RenderTargets;
		Services.Buffers = &Buffers;
		Services.Resources = &Resources;
		Services.Profiler = &Profiler;
		Services.Config = &Config;
		Services.AssetsDirectory = FindAssetsDirectory();

		// --- render passes ----------------------------------------------------
		auto ExperimentPass = std::make_shared<FExperimentRenderPass>(DeviceManager.get(), Stats, std::move(Experiment),
																	  Services, CommandLine);

		const FStatus ExperimentStatus = ExperimentPass->Initialize();
		if (ExperimentStatus.IsError())
		{
			donut::log::fatal("Prism: %s", ExperimentStatus.ToStringWithCode().c_str());
			ExperimentPass.reset();
			return 1;
		}

		auto UiPass = std::make_shared<FUiOverlay>(DeviceManager.get(), Stats, *ExperimentPass);
		if (!UiPass->Initialize(ShaderFactory))
		{
			donut::log::fatal("Prism: failed to initialize the UI overlay.");
			donut::log::SetCallback(&LogToFile);
			UiPass.reset();
			ExperimentPass.reset();
			return 1;
		}

		// Order: the scene renders first, the UI second; input events are dispatched in the reverse
		// order so the UI gets the first chance to consume them.
		DeviceManager->AddRenderPassToBack(ExperimentPass.get());
		DeviceManager->AddRenderPassToBack(UiPass.get());

		donut::log::info("Prism: startup complete.");

		DeviceManager->RunMessageLoop();

		DeviceManager->RemoveRenderPass(UiPass.get());
		DeviceManager->RemoveRenderPass(ExperimentPass.get());

		// 指标 CSV：--bench 在测量结束时已经写过，这里兜住 --metrics 的其他用法。
		ExperimentPass->WriteMetricsIfRequested();

		const bool bExperimentFailed = ExperimentPass->HasFailed();
		const bool bVerificationFailed =
			ExperimentPass->GetExperiment() && !ExperimentPass->GetExperiment()->PassedVerification();
		const bool bAnalysisFailed = ExperimentPass->HasAnalysisFailure();

		// Passes and the shader factory own NVRHI objects, so they must be gone before the device dies.
		donut::log::SetCallback(&LogToFile);
		UiPass.reset();
		ExperimentPass.reset();

		// The console installs a global log callback pointing into its own buffer, so replace it
		// before logging again, otherwise the message lands in freed memory.
		donut::log::SetCallback(&LogToFile);

		ShaderFactory.reset();
		CommonPasses.reset();

		// DeviceManager only releases the swap chain framebuffers inside Shutdown(); destroying it
		// without that call tears the framebuffers down after the device resources are already gone.

		const bool bFailed = bExperimentFailed || bVerificationFailed || bAnalysisFailed;
		donut::log::info("Prism: exited %s.", bFailed ? "with errors" : "cleanly");
		return bFailed ? 1 : 0;
	}

	int Run(int Argc, char** Argv)
	{
		return RunApplication(CreateExperiment(), Argc, Argv);
	}
} // namespace Prism::Host
