#include "Application.h"

#include "CommandLine.h"
#include "LabHost.h"
#include "UiOverlay.h"

#include <adapters/donut/HostConfig.h>
#include <adapters/donut/SceneHost.h>

#include <backends/nvrhi/common/GpuProfiler.h>
#include <backends/nvrhi/common/RenderTargetPool.h>
#include <backends/nvrhi/common/ShaderLibrary.h>

#include <pipelines/SceneForwardPipeline.h>

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

namespace renderlab::host
{
    namespace
    {
        // GUI 应用没有控制台，把日志同时写进可执行文件旁边的文件。
        void LogToFile(donut::log::Severity, const char* message)
        {
            static const std::filesystem::path logPath = donut::app::GetDirectoryWithExecutable() / "renderlab.log";

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

    int RunApplication(std::unique_ptr<Lab> lab, int argc, char** argv)
    {
        donut::log::EnableOutputToMessageBox(false);
        donut::log::SetCallback(&LogToFile);

        const CommandLine commandLine = ParseCommandLine(argc, argv);
        if (commandLine.showHelp)
        {
            donut::log::info("RenderLab\n%s", GetCommandLineUsage().c_str());
            return 0;
        }

        const std::string labName = lab ? lab->GetName() : "Lab";
        donut::log::info("RenderLab: starting %s", labName.c_str());

        // --- configuration -----------------------------------------------------
        adapter::HostConfig config = adapter::LoadHostConfig(commandLine.configPath);

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
            config.scene.asset = adapter::ResolveAssetPath(config.scene.asset).string();

        const std::string applicationName = "RenderLab | " + labName;

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

        if (!deviceManager || !deviceManager->CreateWindowDeviceAndSwapChain(deviceParams, applicationName.c_str()))
        {
            donut::log::fatal("RenderLab: failed to create the D3D12 device or swap chain.");
            return 1;
        }

        deviceManager->SetInformativeWindowTitle(applicationName.c_str());

        nvrhi::IDevice* device = deviceManager->GetDevice();

        // --- shader mounts ----------------------------------------------------
        // /shaders/donut      : Donut 框架 shader（blit、forward shading、UI）
        // /shaders/renderlab  : 本仓库编译出的 HLSL（见 algorithms/shaders 与各 samples 的 shaders.cfg）
        const std::filesystem::path shaderTypeName = donut::app::GetShaderTypeName(deviceManager->GetGraphicsAPI());
        const std::filesystem::path shaderRoot = donut::app::GetDirectoryWithExecutable() / "shaders";

        auto rootFileSystem = std::make_shared<donut::vfs::RootFileSystem>();
        rootFileSystem->mount("/shaders/donut", shaderRoot / "framework" / shaderTypeName);
        rootFileSystem->mount("/shaders/renderlab", shaderRoot / "renderlab" / shaderTypeName);

        auto shaderFactory = std::make_shared<donut::engine::ShaderFactory>(device, rootFileSystem, "/shaders");
        auto commonPasses = std::make_shared<donut::engine::CommonRenderPasses>(device, shaderFactory);

        // --- shared services --------------------------------------------------
        gpu::ShaderLibrary shaderLibrary(device, shaderFactory);
        gpu::RenderTargetPool renderTargets(device);
        gpu::GpuProfiler profiler(device);
        profiler.SetEnabled(config.render.enableGpuTiming);

        pipeline::SceneForwardPipeline scenePipeline;
        if (!scenePipeline.Initialize(device, shaderFactory))
        {
            donut::log::fatal("RenderLab: failed to initialize the shared scene pipeline.");
            deviceManager->Shutdown();
            return 1;
        }

        adapter::SceneHost sceneHost;
        {
            auto sceneFileSystem = std::make_shared<donut::vfs::NativeFileSystem>();
            const Status sceneStatus = sceneHost.Load(device, shaderFactory, sceneFileSystem, config);
            if (sceneStatus.IsError())
            {
                donut::log::fatal("RenderLab: scene setup failed -- %s", sceneStatus.ToStringWithCode().c_str());
                deviceManager->Shutdown();
                return 1;
            }
        }

        // --- host state -------------------------------------------------------
        HostStats stats;
        stats.rendererDescription = deviceManager->GetRendererString();
        stats.sceneDescription = sceneHost.GetData().description;
        stats.scene = sceneHost.GetData().stats;
        stats.configDescription = config.loadedFromFile ? config.sourcePath.string() : std::string("(built-in defaults)");

        HostServices services;
        services.device = device;
        services.shaderFactory = shaderFactory;
        services.commonPasses = commonPasses;
        services.shaders = &shaderLibrary;
        services.targets = &renderTargets;
        services.profiler = &profiler;
        services.scenePipeline = &scenePipeline;
        services.sceneHost = &sceneHost;
        services.config = &config;
        services.assetsDirectory = FindAssetsDirectory();

        // --- render passes ----------------------------------------------------
        auto labPass = std::make_shared<LabRenderPass>(
            deviceManager.get(), stats, std::move(lab), services, commandLine);

        const Status labStatus = labPass->Initialize();
        if (labStatus.IsError())
        {
            donut::log::fatal("RenderLab: %s", labStatus.ToStringWithCode().c_str());
            labPass.reset();
            deviceManager->Shutdown();
            return 1;
        }

        auto uiPass = std::make_shared<UiOverlay>(deviceManager.get(), stats, *labPass);
        if (!uiPass->Initialize(shaderFactory))
        {
            donut::log::fatal("RenderLab: failed to initialize the UI overlay.");
            uiPass.reset();
            labPass.reset();
            deviceManager->Shutdown();
            return 1;
        }

        // Order: the scene renders first, the UI second; input events are dispatched in the reverse
        // order so the UI gets the first chance to consume them.
        deviceManager->AddRenderPassToBack(labPass.get());
        deviceManager->AddRenderPassToBack(uiPass.get());

        donut::log::info("RenderLab: startup complete.");

        deviceManager->RunMessageLoop();

        deviceManager->RemoveRenderPass(uiPass.get());
        deviceManager->RemoveRenderPass(labPass.get());

        const bool labFailed = labPass->HasFailed();
        const bool verificationFailed = labPass->GetLab() && !labPass->GetLab()->PassedVerification();

        // Passes and the shader factory own NVRHI objects, so they must be gone before the device dies.
        uiPass.reset();
        labPass.reset();

        // The console installs a global log callback pointing into its own buffer, so replace it
        // before logging again, otherwise the message lands in freed memory.
        donut::log::SetCallback(&LogToFile);

        shaderFactory.reset();
        commonPasses.reset();

        // DeviceManager only releases the swap chain framebuffers inside Shutdown(); destroying it
        // without that call tears the framebuffers down after the device resources are already gone.
        deviceManager->Shutdown();

        donut::log::info("RenderLab: exited %s.", (labFailed || verificationFailed) ? "with errors" : "cleanly");
        return (labFailed || verificationFailed) ? 1 : 0;
    }

    int Run(int argc, char** argv)
    {
        return RunApplication(CreateLab(), argc, argv);
    }
}
