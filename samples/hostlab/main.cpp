// RenderLab host sample (roadmap milestone M0)
//
// Responsibilities:
//   * Window / NVRHI device / swap chain / message loop -- Donut DeviceManager
//   * Camera / UI / logging / scene graph / view and render targets -- this file
//   * Rendering algorithm -- Donut ForwardShadingPass for now, custom passes from M2/M3 on
//
// Conventions (declared explicitly at M0, to be codified by the M1 contract layer):
//   * World space: Y up, meters, right-handed geometry
//   * View space: left-handed, D3D style projection (z in [0, 1]), no reverse Z
//   * Depth clear value: 1.0, depth comparison: Less
//   * Color: linear HDR (RGBA16_FLOAT) offscreen target, blitted to the swap chain; no tone mapping yet

#include <donut/app/ApplicationBase.h>
#include <donut/app/Camera.h>
#include <donut/app/DeviceManager.h>
#include <donut/app/imgui_console.h>
#include <donut/app/imgui_renderer.h>
#include <donut/core/log.h>
#include <donut/core/math/math.h>
#include <donut/core/vfs/VFS.h>
#include <donut/engine/BindingCache.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/ConsoleInterpreter.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <donut/render/DrawStrategy.h>
#include <donut/render/ForwardShadingPass.h>
#include <donut/render/GeometryPasses.h>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

#include "host_config.h"
#include "procedural_scene.h"

namespace dm = donut::math;

namespace
{
    constexpr uint32_t kSmokeTestFrameCount = 3;

    // GUI builds have no console, so mirror everything into a file next to the executable.
    void LogToFile(donut::log::Severity, const char* message)
    {
        FILE* file = nullptr;
        if (fopen_s(&file, "hostlab_debug.log", "a") == 0 && file != nullptr)
        {
            fprintf(file, "%s\n", message);
            fclose(file);
        }
    }

    struct HostStats
    {
        uint32_t width = 0;
        uint32_t height = 0;
        float frameTimeMs = 0.f;
        float framesPerSecond = 0.f;
        dm::float3 cameraPosition = dm::float3(0.f);
        dm::float3 cameraDirection = dm::float3(0.f, 0.f, 1.f);
        uint32_t meshCount = 0;
        uint32_t instanceCount = 0;
        uint32_t lightCount = 0;
        uint32_t triangleCount = 0;
        bool firstPerson = true;
        std::string configSource = "(built-in defaults)";
    };

    class HostRenderPass : public donut::app::IRenderPass
    {
    public:
        HostRenderPass(
            donut::app::DeviceManager* deviceManager,
            HostStats& stats,
            const renderlab::HostConfig& config,
            bool smokeTest)
            : donut::app::IRenderPass(deviceManager)
            , m_Stats(stats)
            , m_Config(config)
            , m_SmokeTest(smokeTest)
        {}

        bool Init(std::shared_ptr<donut::engine::ShaderFactory> shaderFactory)
        {
            auto device = GetDevice();

            m_CommandList = device->createCommandList();
            m_CommonPasses = std::make_shared<donut::engine::CommonRenderPasses>(device, shaderFactory);
            m_BindingCache = std::make_unique<donut::engine::BindingCache>(device);

            m_ForwardPass = std::make_shared<donut::render::ForwardShadingPass>(device, m_CommonPasses);
            m_ForwardPass->Init(*shaderFactory, donut::render::ForwardShadingPass::CreateParameters());

            // Building the scene performs the first resource uploads, recorded into this command list.
            m_CommandList->open();
            m_Scene = renderlab::CreateProceduralScene(device, m_CommandList, m_Config.lighting);
            m_CommandList->close();
            device->executeCommandList(m_CommandList);

            m_AmbientTop = dm::float3(m_Config.lighting.ambientIntensity);
            m_AmbientBottom = dm::float3(m_Config.lighting.ambientIntensity * 0.6f);

            // Initial camera state comes from the config preset
            m_Camera.GetFirstPersonCamera().LookAt(m_Config.camera.position, m_Config.camera.target);
            m_Camera.GetFirstPersonCamera().SetMoveSpeed(m_Config.camera.moveSpeed);

            const float orbitDistance = dm::length(m_Config.camera.position - m_Config.camera.target);
            m_Camera.GetThirdPersonCamera().SetTargetPosition(m_Config.camera.target);
            m_Camera.GetThirdPersonCamera().SetDistance(std::max(orbitDistance, 0.5f));
            m_Camera.SwitchToFirstPerson(false);

            m_RenderSize = dm::uint2(m_Config.window.width, m_Config.window.height);

            m_Stats.meshCount = m_Scene.meshCount;
            m_Stats.instanceCount = m_Scene.instanceCount;
            m_Stats.lightCount = m_Scene.lightCount;
            m_Stats.triangleCount = m_Scene.triangleCount;
            m_Stats.firstPerson = m_Camera.IsFirstPersonActive();

            return true;
        }

        void Animate(float elapsedTimeSeconds) override
        {
            m_Camera.Animate(elapsedTimeSeconds);

            const float aspectRatio = float(m_RenderSize.x) / float(std::max(m_RenderSize.y, 1u));
            m_Projection = dm::perspProjD3DStyle(
                dm::radians(m_Config.camera.fovDegrees),
                aspectRatio,
                m_Config.camera.zNear,
                m_Config.camera.zFar);

            m_View.SetViewport(nvrhi::Viewport(float(m_RenderSize.x), float(m_RenderSize.y)));
            m_View.SetMatrices(m_Camera.GetWorldToViewMatrix(), m_Projection);
            m_View.UpdateCache();

            // The orbit camera needs viewport and projection matrices to convert drag deltas
            m_Camera.GetThirdPersonCamera().SetView(m_View);

            const float frameTimeMs = elapsedTimeSeconds * 1000.f;
            m_SmoothedFrameTimeMs = (m_SmoothedFrameTimeMs <= 0.f)
                ? frameTimeMs
                : (m_SmoothedFrameTimeMs * 0.9f + frameTimeMs * 0.1f);

            m_Stats.width = m_RenderSize.x;
            m_Stats.height = m_RenderSize.y;
            m_Stats.frameTimeMs = m_SmoothedFrameTimeMs;
            m_Stats.framesPerSecond = (m_SmoothedFrameTimeMs > 0.f) ? (1000.f / m_SmoothedFrameTimeMs) : 0.f;
            m_Stats.firstPerson = m_Camera.IsFirstPersonActive();

            if (const donut::app::BaseCamera* camera = m_Camera.GetActiveUserCamera())
            {
                m_Stats.cameraPosition = camera->GetPosition();
                m_Stats.cameraDirection = camera->GetDir();
            }
        }

        void Render(nvrhi::IFramebuffer* framebuffer) override
        {
            if (!m_Framebuffer)
                return;

            auto device = GetDevice();

            m_CommandList->open();

            const nvrhi::TextureSubresourceSet subresources = nvrhi::TextureSubresourceSet(0, 1, 0, 1);
            m_CommandList->clearTextureFloat(m_Color, subresources, nvrhi::Color(0.04f, 0.05f, 0.07f, 1.f));
            m_CommandList->clearDepthStencilTexture(m_Depth, subresources, true, 1.f, false, 0);

            donut::render::ForwardShadingPass::Context context;
            m_ForwardPass->PrepareLights(
                context,
                m_CommandList,
                m_Scene.graph->GetLights(),
                m_AmbientTop,
                m_AmbientBottom,
                {});

            m_DrawStrategy.PrepareForView(m_Scene.graph->GetRootNode(), m_View);

            donut::render::RenderView(
                m_CommandList,
                &m_View,
                &m_View,
                m_Framebuffer,
                m_DrawStrategy,
                *m_ForwardPass,
                context);

            m_CommonPasses->BlitTexture(m_CommandList, framebuffer, m_Color, m_BindingCache.get());

            m_CommandList->close();
            device->executeCommandList(m_CommandList);

            if (m_SmokeTest && (++m_FrameCounter >= kSmokeTestFrameCount))
            {
                donut::log::info("HostLab: smoke test finished %u frames, shutting down.", m_FrameCounter);
                glfwSetWindowShouldClose(GetDeviceManager()->GetWindow(), 1);
            }
        }

        void BackBufferResizing() override
        {
            // Wait for the GPU before recreating: a command list returning does not mean
            // the GPU is done with these resources.
            GetDevice()->waitForIdle();

            m_Color = nullptr;
            m_Depth = nullptr;
            m_Framebuffer = nullptr;
            m_BindingCache->Clear();
        }

        void BackBufferResized(const uint32_t width, const uint32_t height, const uint32_t sampleCount) override
        {
            m_RenderSize = dm::uint2(std::max(width, 1u), std::max(height, 1u));
            CreateRenderTargets(m_RenderSize.x, m_RenderSize.y);

            donut::log::info("HostLab: render targets rebuilt at %u x %u.", m_RenderSize.x, m_RenderSize.y);
        }

        // During a smoke test the window may not have focus, but we still need to render a few frames
        bool ShouldAnimateUnfocused() override { return m_SmokeTest; }
        bool ShouldRenderUnfocused() override { return m_SmokeTest; }

        bool KeyboardUpdate(int key, int scancode, int action, int mods) override
        {
            return m_Camera.KeyboardUpdate(key, scancode, action, mods);
        }

        bool MousePosUpdate(double xpos, double ypos) override
        {
            return m_Camera.MousePosUpdate(xpos, ypos);
        }

        bool MouseButtonUpdate(int button, int action, int mods) override
        {
            return m_Camera.MouseButtonUpdate(button, action, mods);
        }

        bool MouseScrollUpdate(double xoffset, double yoffset) override
        {
            return m_Camera.MouseScrollUpdate(xoffset, yoffset);
        }

        void SwitchToFirstPerson() { m_Camera.SwitchToFirstPerson(true); }
        void SwitchToThirdPerson() { m_Camera.SwitchToThirdPerson(true); }

    private:
        void CreateRenderTargets(uint32_t width, uint32_t height)
        {
            auto device = GetDevice();

            nvrhi::TextureDesc colorDesc;
            colorDesc.width = width;
            colorDesc.height = height;
            colorDesc.format = nvrhi::Format::RGBA16_FLOAT;
            colorDesc.debugName = "HostSceneColor";
            colorDesc.isRenderTarget = true;
            colorDesc.initialState = nvrhi::ResourceStates::RenderTarget;
            colorDesc.keepInitialState = true;
            colorDesc.clearValue = nvrhi::Color(0.f);
            m_Color = device->createTexture(colorDesc);

            nvrhi::TextureDesc depthDesc;
            depthDesc.width = width;
            depthDesc.height = height;
            depthDesc.format = nvrhi::Format::D32;
            depthDesc.debugName = "HostSceneDepth";
            depthDesc.isRenderTarget = true;
            depthDesc.initialState = nvrhi::ResourceStates::DepthWrite;
            depthDesc.keepInitialState = true;
            depthDesc.clearValue = nvrhi::Color(1.f);
            m_Depth = device->createTexture(depthDesc);

            nvrhi::FramebufferDesc framebufferDesc;
            framebufferDesc.addColorAttachment(m_Color);
            framebufferDesc.setDepthAttachment(m_Depth);
            m_Framebuffer = device->createFramebuffer(framebufferDesc);
        }

        HostStats& m_Stats;
        renderlab::HostConfig m_Config;
        bool m_SmokeTest = false;

        nvrhi::CommandListHandle m_CommandList;
        nvrhi::TextureHandle m_Color;
        nvrhi::TextureHandle m_Depth;
        nvrhi::FramebufferHandle m_Framebuffer;

        std::shared_ptr<donut::engine::CommonRenderPasses> m_CommonPasses;
        std::shared_ptr<donut::render::ForwardShadingPass> m_ForwardPass;
        std::unique_ptr<donut::engine::BindingCache> m_BindingCache;

        renderlab::ProceduralScene m_Scene;
        donut::render::InstancedOpaqueDrawStrategy m_DrawStrategy;

        donut::app::SwitchableCamera m_Camera;
        donut::engine::PlanarView m_View;
        dm::float4x4 m_Projection = dm::diagonal(dm::float4(1.f));

        dm::float3 m_AmbientTop = dm::float3(0.f);
        dm::float3 m_AmbientBottom = dm::float3(0.f);

        dm::uint2 m_RenderSize = dm::uint2(1);
        float m_SmoothedFrameTimeMs = 0.f;
        uint32_t m_FrameCounter = 0;
    };

    class HostUiPass : public donut::app::ImGui_Renderer
    {
    public:
        HostUiPass(donut::app::DeviceManager* deviceManager, HostStats& stats, HostRenderPass& scenePass)
            : donut::app::ImGui_Renderer(deviceManager)
            , m_Stats(stats)
            , m_ScenePass(scenePass)
        {
            donut::app::ImGui_Console::Options consoleOptions;
            consoleOptions.capture_log = true;   // take over donut::log so messages land in the console window
            consoleOptions.show_info = true;

            m_Console = std::make_unique<donut::app::ImGui_Console>(
                std::make_shared<donut::engine::console::Interpreter>(),
                consoleOptions);
        }

    protected:
        void buildUI() override
        {
            ImGui::SetNextWindowPos(ImVec2(20.f, 20.f), ImGuiCond_FirstUseEver);
            ImGui::Begin("RenderLab | Host", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Text("Renderer: %s", GetDeviceManager()->GetRendererString());
            ImGui::Text("Resolution: %u x %u", m_Stats.width, m_Stats.height);
            ImGui::Text("Frame: %.2f ms (%.1f FPS)", m_Stats.frameTimeMs, m_Stats.framesPerSecond);

            ImGui::SeparatorText("Camera");
            ImGui::Text("Mode: %s", m_Stats.firstPerson ? "First person" : "Orbit (third person)");
            ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                m_Stats.cameraPosition.x, m_Stats.cameraPosition.y, m_Stats.cameraPosition.z);
            ImGui::Text("Direction: (%.2f, %.2f, %.2f)",
                m_Stats.cameraDirection.x, m_Stats.cameraDirection.y, m_Stats.cameraDirection.z);
            if (ImGui::Button("First person"))
                m_ScenePass.SwitchToFirstPerson();
            ImGui::SameLine();
            if (ImGui::Button("Orbit"))
                m_ScenePass.SwitchToThirdPerson();

            ImGui::SeparatorText("Scene");
            ImGui::Text("Meshes: %u   Instances: %u", m_Stats.meshCount, m_Stats.instanceCount);
            ImGui::Text("Lights: %u   Triangles: %u", m_Stats.lightCount, m_Stats.triangleCount);
            ImGui::Text("Config: %s", m_Stats.configSource.c_str());

            ImGui::SeparatorText("Controls");
            ImGui::BulletText("First person: WASD/QE to move, drag with left button to look");
            ImGui::BulletText("Orbit: drag with left button to rotate, wheel to zoom");

            ImGui::End();

            bool consoleOpen = true;
            m_Console->Render(&consoleOpen);
        }

    private:
        HostStats& m_Stats;
        HostRenderPass& m_ScenePass;
        std::unique_ptr<donut::app::ImGui_Console> m_Console;
    };
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // A GUI app has no console and a modal dialog would block the headless smoke test,
    // so everything goes through the log file and the in-app console.
    donut::log::EnableOutputToMessageBox(false);

    donut::log::SetCallback(&LogToFile);

    donut::log::info("HostLab: WinMain start");

    bool smokeTest = false;
    std::filesystem::path configPath;

    for (int argIndex = 0; argIndex < __argc; ++argIndex)
    {
        const std::string argument = __argv[argIndex];
        if (argument == "--smoke-test")
            smokeTest = true;
        else if (argument == "--config" && (argIndex + 1) < __argc)
            configPath = __argv[++argIndex];
    }

    const renderlab::HostConfig config = renderlab::LoadHostConfig(configPath);

    donut::app::DeviceCreationParameters deviceParams;
    deviceParams.backBufferWidth = config.window.width;
    deviceParams.backBufferHeight = config.window.height;
    deviceParams.vsyncEnabled = config.window.vsync;
    deviceParams.swapChainBufferCount = 3;
    deviceParams.startMaximized = false;
    deviceParams.swapChainSampleCount = 1;
    deviceParams.depthBufferFormat = nvrhi::Format::UNKNOWN; // the scene has its own depth, the UI layer does not need one
#ifdef _DEBUG
    deviceParams.enableDebugRuntime = true;
    deviceParams.enableNvrhiValidationLayer = true;
#endif

    const std::shared_ptr<donut::app::DeviceManager> deviceManager(
        donut::app::DeviceManager::Create(nvrhi::GraphicsAPI::D3D12));

    if (!deviceManager->CreateWindowDeviceAndSwapChain(deviceParams, "RenderLab Host"))
    {
        donut::log::fatal("HostLab: failed to create the D3D12 device or swap chain.");
        return 1;
    }

    deviceManager->SetInformativeWindowTitle("RenderLab Host");

    // Donut framework shaders (blit, forward shading, ...) live in <exe>/shaders/framework/<dxil|dxbc|spirv>
    auto rootFileSystem = std::make_shared<donut::vfs::RootFileSystem>();
    const std::filesystem::path frameworkShaderPath = donut::app::GetDirectoryWithExecutable()
        / "shaders" / "framework" / donut::app::GetShaderTypeName(deviceManager->GetGraphicsAPI());
    rootFileSystem->mount("/shaders/donut", frameworkShaderPath);

    auto shaderFactory = std::make_shared<donut::engine::ShaderFactory>(
        deviceManager->GetDevice(), rootFileSystem, "/shaders");

    HostStats stats;
    stats.configSource = config.loadedFromFile ? config.sourcePath.string() : std::string("(built-in defaults)");

    auto scenePass = std::make_shared<HostRenderPass>(deviceManager.get(), stats, config, smokeTest);
    if (!scenePass->Init(shaderFactory))
    {
        donut::log::fatal("HostLab: failed to initialize the scene render pass.");
        return 1;
    }

    auto uiPass = std::make_shared<HostUiPass>(deviceManager.get(), stats, *scenePass);
    if (!uiPass->Init(shaderFactory))
    {
        donut::log::fatal("HostLab: failed to initialize the UI pass.");
        return 1;
    }

    // Order: scene renders first, UI second; input events are dispatched in the reverse order
    // so the UI gets the first chance to consume them.
    deviceManager->AddRenderPassToBack(scenePass.get());
    deviceManager->AddRenderPassToBack(uiPass.get());

    donut::log::info("HostLab: startup complete (smoke test = %s).", smokeTest ? "on" : "off");

    deviceManager->RunMessageLoop();

    deviceManager->RemoveRenderPass(uiPass.get());
    deviceManager->RemoveRenderPass(scenePass.get());

    // Passes and the shader factory own NVRHI objects, so they must be gone before the device dies.
    uiPass.reset();
    scenePass.reset();
    shaderFactory.reset();

    // The console installs a global log callback pointing into its own buffer, so replace it
    // before logging again, otherwise the message lands in freed memory.
    donut::log::SetCallback(&LogToFile);

    // DeviceManager only releases the swap chain framebuffers inside Shutdown(); destroying it
    // without that call tears the framebuffers down after the device resources are already gone.
    deviceManager->Shutdown();

    donut::log::info("HostLab: exited cleanly.");
    return 0;
}
