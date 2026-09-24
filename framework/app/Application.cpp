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

namespace prism::host
{
    namespace
    {
        // GUI 应用没有控制台，把日志同时写进可执行文件旁边的文件。
        void LogToFile(donut::log::Severity, const char* message)
        {
            static const std::filesystem::path logPath = donut::app::GetDirectoryWithExecutable() / "prism.log";

            FILE* file = nullptr;
            if (_wfopen_s(&file, logPath.c_str(), L"a") == 0 && file != nullptr)
            {
                fprintf(file, "%s\n", message);
                fclose(file);
            }
        }

        // NVRHI / D3D12 的校验信息进入同一个日志，避免"设备丢失但日志空白"。
        class LogMessageCallback final : public nvrhi::IMessageCallback
        {
        public:
            void message(nvrhi::MessageSeverity severity, const char* messageText) override
            {
                switch (severity)
                {
                case nvrhi::MessageSeverity::Error:
                    donut::log::error("[nvrhi] %s", messageText);
                    break;
                case nvrhi::MessageSeverity::Warning:
                    donut::log::warning("[nvrhi] %s", messageText);
                    break;
                default:
                    donut::log::info("[nvrhi] %s", messageText);
                    break;
                }
            }
        };

        // 从可执行文件目录向上寻找仓库根的 assets 目录；找不到就返回空路径。
        std::filesystem::path FindAssetsDirectory()
        {
            std::filesystem::path directory = donut::app::GetDirectoryWithExecutable();

            for (int depth = 0; depth < 6 && !directory.empty(); ++depth)
            {
                const std::filesystem::path candidate = directory / "assets";
                if (std::filesystem::is_directory(candidate))
                    return candidate;

                const std::filesystem::path parent = directory.parent_path();
                if (parent == directory)
                    break;

                directory = parent;
            }

            return std::filesystem::path();
        }
    }

    int RunApplication(std::unique_ptr<Experiment> experiment, int argc, char** argv)
    {
        donut::log::EnableOutputToMessageBox(false);
        donut::log::SetCallback(&LogToFile);

        const CommandLine commandLine = ParseCommandLine(argc, argv);
        if (commandLine.showHelp)
        {
            donut::log::info("Prism\n%s", GetCommandLineUsage().c_str());
            return 0;
        }

        const std::string experimentName = experiment ? experiment->GetName() : "Experiment";
        donut::log::info("Prism: starting %s", experimentName.c_str());

        // --- configuration -----------------------------------------------------
        const std::filesystem::path executablePath =
            (argc > 0 && argv) ? std::filesystem::path(argv[0]) : std::filesystem::path();

        host::HostConfig config = host::LoadHostConfig(commandLine.configPath, executablePath);

        if (!commandLine.sceneSource.empty())
            config.scene.source = commandLine.sceneSource;

        if (!commandLine.sceneAsset.empty())
            config.scene.asset = commandLine.sceneAsset;

        if (commandLine.width > 0)
            config.window.width = commandLine.width;

        if (commandLine.height > 0)
            config.window.height = commandLine.height;

        if (commandLine.disableVsync)
            config.window.vsync = false;

        if (commandLine.disableGpuTiming)
            config.render.enableGpuTiming = false;

        if (!config.scene.asset.empty())
            config.scene.asset = host::ResolveAssetPath(config.scene.asset).string();

        const std::string applicationName = "Prism | " + experimentName;

        // --- device and swap chain --------------------------------------------
        donut::app::DeviceCreationParameters deviceParams;
        deviceParams.backBufferWidth = config.window.width;
        deviceParams.backBufferHeight = config.window.height;
        deviceParams.vsyncEnabled = config.window.vsync;
        deviceParams.swapChainBufferCount = 3;
        deviceParams.startMaximized = false;
        deviceParams.swapChainSampleCount = 1;
        deviceParams.depthBufferFormat = nvrhi::Format::UNKNOWN; // the UI layer does not need a depth buffer
#ifdef _DEBUG
        deviceParams.enableDebugRuntime = true;
        deviceParams.enableNvrhiValidationLayer = true;
#endif

        LogMessageCallback messageCallback;
        deviceParams.messageCallback = &messageCallback;

        std::unique_ptr<donut::app::DeviceManager> deviceManager(
            donut::app::DeviceManager::Create(nvrhi::GraphicsAPI::D3D12));

        struct DeviceLifetime
        {
            donut::app::DeviceManager* manager;
            ~DeviceLifetime() { if (manager) manager->Shutdown(); }
        } deviceLifetime{deviceManager.get()};

        if (!deviceManager || !deviceManager->CreateWindowDeviceAndSwapChain(deviceParams, applicationName.c_str()))
        {
            donut::log::fatal("Prism: failed to create the D3D12 device or swap chain.");
            return 1;
        }

        deviceManager->SetInformativeWindowTitle(applicationName.c_str());

        nvrhi::IDevice* device = deviceManager->GetDevice();

        // --- shader mounts ----------------------------------------------------
        // /shaders/donut      : Donut 框架 shader（blit、forward shading、UI）
        // /shaders/prism: package-owned Prism bytecode.
        const std::filesystem::path shaderTypeName = donut::app::GetShaderTypeName(deviceManager->GetGraphicsAPI());
        const std::filesystem::path shaderRoot = donut::app::GetDirectoryWithExecutable() / "shaders";

        auto rootFileSystem = std::make_shared<donut::vfs::RootFileSystem>();
        rootFileSystem->mount("/shaders/donut", shaderRoot / "framework" / shaderTypeName);
        rootFileSystem->mount("/shaders/prism", shaderRoot / "prism" / shaderTypeName);

        auto shaderFactory = std::make_shared<donut::engine::ShaderFactory>(device, rootFileSystem, "/shaders");
        auto commonPasses = std::make_shared<donut::engine::CommonRenderPasses>(device, shaderFactory);

        // --- shared services --------------------------------------------------
        gpu::ShaderLibrary shaderLibrary(device, shaderFactory);
        gpu::TextureCache renderTargets(device);
        gpu::BufferCache buffers(device);
        gpu::ResourceTable resources(renderTargets, buffers);
        gpu::GpuProfiler profiler(device);
        profiler.SetEnabled(config.render.enableGpuTiming);

        // --- host state -------------------------------------------------------
        HostStats stats;
        stats.rendererDescription = deviceManager->GetRendererString();
        stats.configDescription = config.loadedFromFile ? config.sourcePath.string() : std::string("(built-in defaults)");

        HostServices services;
        services.executablePath = std::filesystem::absolute(executablePath);
        services.device = device;
        services.shaderFactory = shaderFactory;
        services.commonPasses = commonPasses;
        services.shaders = &shaderLibrary;
        services.targets = &renderTargets;
        services.buffers = &buffers;
        services.resources = &resources;
        services.profiler = &profiler;
        services.config = &config;
        services.assetsDirectory = FindAssetsDirectory();

        // --- render passes ----------------------------------------------------
        auto experimentPass = std::make_shared<ExperimentRenderPass>(
            deviceManager.get(), stats, std::move(experiment), services, commandLine);

        const Status experimentStatus = experimentPass->Initialize();
        if (experimentStatus.IsError())
        {
            donut::log::fatal("Prism: %s", experimentStatus.ToStringWithCode().c_str());
            experimentPass.reset();
            return 1;
        }

        auto uiPass = std::make_shared<UiOverlay>(deviceManager.get(), stats, *experimentPass);
        if (!uiPass->Initialize(shaderFactory))
        {
            donut::log::fatal("Prism: failed to initialize the UI overlay.");
            donut::log::SetCallback(&LogToFile);
            uiPass.reset();
            experimentPass.reset();
            return 1;
        }

        // Order: the scene renders first, the UI second; input events are dispatched in the reverse
        // order so the UI gets the first chance to consume them.
        deviceManager->AddRenderPassToBack(experimentPass.get());
        deviceManager->AddRenderPassToBack(uiPass.get());

        donut::log::info("Prism: startup complete.");

        deviceManager->RunMessageLoop();

        deviceManager->RemoveRenderPass(uiPass.get());
        deviceManager->RemoveRenderPass(experimentPass.get());

        // 指标 CSV：--bench 在测量结束时已经写过，这里兜住 --metrics 的其他用法。
        experimentPass->WriteMetricsIfRequested();

        const bool experimentFailed = experimentPass->HasFailed();
        const bool verificationFailed = experimentPass->GetExperiment() && !experimentPass->GetExperiment()->PassedVerification();
        const bool analysisFailed = experimentPass->HasAnalysisFailure();

        // Passes and the shader factory own NVRHI objects, so they must be gone before the device dies.
        donut::log::SetCallback(&LogToFile);
        uiPass.reset();
        experimentPass.reset();

        // The console installs a global log callback pointing into its own buffer, so replace it
        // before logging again, otherwise the message lands in freed memory.
        donut::log::SetCallback(&LogToFile);

        shaderFactory.reset();
        commonPasses.reset();

        // DeviceManager only releases the swap chain framebuffers inside Shutdown(); destroying it
        // without that call tears the framebuffers down after the device resources are already gone.

        const bool failed = experimentFailed || verificationFailed || analysisFailed;
        donut::log::info("Prism: exited %s.", failed ? "with errors" : "cleanly");
        return failed ? 1 : 0;
    }

    int Run(int argc, char** argv)
    {
        return RunApplication(CreateExperiment(), argc, argv);
    }
}
