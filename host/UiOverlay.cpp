#include "UiOverlay.h"

#include <donut/core/log.h>
#include <donut/engine/ConsoleInterpreter.h>

#include <imgui.h>

namespace renderlab::host
{
    UiOverlay::UiOverlay(donut::app::DeviceManager* deviceManager, HostStats& stats, LabRenderPass& host)
        : donut::app::ImGui_Renderer(deviceManager)
        , m_Stats(stats)
        , m_Host(host)
    {
        donut::app::ImGui_Console::Options consoleOptions;
        consoleOptions.show_info = true;

        // 交互运行时日志转到控制台窗口；冒烟测试/截图运行时保留文件日志，
        // 否则 CI 看不到初始化与自检的输出。
        const CommandLine& commandLine = host.GetCommandLine();
        const bool headlessRun = commandLine.smokeTest || !commandLine.capturePath.empty();
        consoleOptions.capture_log = !headlessRun;

        m_Console = std::make_unique<donut::app::ImGui_Console>(
            std::make_shared<donut::engine::console::Interpreter>(),
            consoleOptions);
    }

    bool UiOverlay::Initialize(const std::shared_ptr<donut::engine::ShaderFactory>& shaderFactory)
    {
        return Init(shaderFactory);
    }

    void UiOverlay::buildUI()
    {
        Lab* lab = m_Host.GetLab();

        ImGui::SetNextWindowPos(ImVec2(20.f, 20.f), ImGuiCond_FirstUseEver);
        ImGui::Begin("RenderLab", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        BuildHeaderSection();
        BuildCameraSection();
        BuildSceneSection();
        BuildTimingSection();

        if (lab)
        {
            ImGui::SeparatorText("Experiment");
            if (lab->GetDescription() && lab->GetDescription()[0] != '\0')
                ImGui::TextWrapped("%s", lab->GetDescription());

            lab->BuildUI(m_Host.GetContext());
        }

        ImGui::SeparatorText("Controls");
        ImGui::BulletText("First person: WASD/QE to move, drag with left button to look");
        ImGui::BulletText("Orbit: drag with left button to rotate, wheel to zoom");

        ImGui::End();

        bool consoleOpen = true;
        m_Console->Render(&consoleOpen);
    }

    void UiOverlay::BuildHeaderSection()
    {
        ImGui::Text("Experiment: %s", m_Host.GetLab() ? m_Host.GetLab()->GetName() : "(none)");
        ImGui::Text("Renderer: %s", m_Stats.rendererDescription.c_str());
        ImGui::Text("Render: %u x %u   Output: %u x %u",
            m_Stats.renderSize.width, m_Stats.renderSize.height,
            m_Stats.outputSize.width, m_Stats.outputSize.height);
        ImGui::Text("Frame: %.2f ms (%.1f FPS)   Frame index: %llu",
            m_Stats.frameTimeMs, m_Stats.framesPerSecond, (unsigned long long)m_Stats.frameIndex);
        ImGui::Text("History reset: %s", m_Stats.historyResetDescription.c_str());
    }

    void UiOverlay::BuildCameraSection()
    {
        adapter::CameraController& camera = m_Host.GetCamera();

        ImGui::SeparatorText("Camera");
        ImGui::Text("Mode: %s", m_Stats.firstPerson ? "First person" : "Orbit (third person)");
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", m_Stats.cameraPosition.x, m_Stats.cameraPosition.y, m_Stats.cameraPosition.z);
        ImGui::Text("Direction: (%.2f, %.2f, %.2f)", m_Stats.cameraDirection.x, m_Stats.cameraDirection.y, m_Stats.cameraDirection.z);

        if (ImGui::Button("First person"))
            camera.SwitchToFirstPerson(true);

        ImGui::SameLine();
        if (ImGui::Button("Orbit"))
            camera.SwitchToThirdPerson(true);
    }

    void UiOverlay::BuildSceneSection()
    {
        ImGui::SeparatorText("Scene");
        ImGui::Text("Source: %s", m_Stats.sceneDescription.c_str());
        ImGui::Text("Meshes: %u   Instances: %u", m_Stats.scene.meshes, m_Stats.scene.instances);
        ImGui::Text("Lights: %u   Triangles: %u", m_Stats.scene.lights, m_Stats.scene.triangles);
        ImGui::Text("Config: %s", m_Stats.configDescription.c_str());
    }

    void UiOverlay::BuildTimingSection()
    {
        gpu::GpuProfiler* profiler = m_Host.GetContext().profiler;
        if (!profiler)
            return;

        ImGui::SeparatorText("GPU timing");

        if (ImGui::Checkbox("Enable timestamps", &m_TimingEnabled))
            profiler->SetEnabled(m_TimingEnabled);

        if (profiler->IsEnabled())
        {
            const std::vector<gpu::GpuProfiler::ScopeTiming>& timings = profiler->GetTimings();
            if (timings.empty())
            {
                ImGui::TextUnformatted("(waiting for the first resolved results)");
            }
            else if (ImGui::BeginTable("RendererTimings", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Scope");
                ImGui::TableSetupColumn("Last (ms)", ImGuiTableColumnFlags_WidthFixed, 80.f);
                ImGui::TableSetupColumn("Avg (ms)", ImGuiTableColumnFlags_WidthFixed, 80.f);
                ImGui::TableHeadersRow();

                for (const gpu::GpuProfiler::ScopeTiming& timing : timings)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(timing.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", timing.valid ? timing.milliseconds : 0.f);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", timing.valid ? timing.smoothedMilliseconds : 0.f);
                }

                ImGui::EndTable();
            }

            ImGui::Text("Measured total: %.3f ms", profiler->GetTotalMilliseconds());
        }
    }
}
