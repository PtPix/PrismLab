#include "ExperimentTools.h"
#include <imgui.h>

namespace prism::host
{
    void ExperimentTools::BuildUI()
    {
        if (ImGui::CollapsingHeader("Shaders"))
        {
            ImGui::BeginDisabled(reload.Running());
            if (ImGui::Button("Compile and reload (F6)")) reload.Request();
            ImGui::EndDisabled();
            ImGui::TextWrapped("%s", reload.Message().c_str());
        }
        if (ImGui::CollapsingHeader("Comparison"))
        {
            const char* modes[] = {"Off", "A", "B", "Side by side", "Wipe", "Absolute difference"};
            int mode = int(comparison.settings.mode);
            if (ImGui::Combo("Mode", &mode, modes, 6)) comparison.settings.mode = gpu::ComparisonMode(mode);
            auto sourcePicker = [&](const char* label, std::string& selected)
            {
                if (!ImGui::BeginCombo(label, selected.c_str())) return;
                for (const auto& source : comparison.Sources())
                    if (ImGui::Selectable(source.id.c_str(), source.id == selected)) selected = source.id;
                ImGui::EndCombo();
            };
            sourcePicker("Image A", comparison.sourceA); sourcePicker("Image B", comparison.sourceB);
            ImGui::SliderFloat("Split", &comparison.settings.split, 0.f, 1.f);
            ImGui::SliderFloat("Difference gain", &comparison.settings.gain, 0.1f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
            if (ImGui::Button("Freeze B")) comparison.RequestFreeze();
            ImGui::SameLine(); if (ImGui::Button("Release frozen B")) comparison.ClearFrozen();
            ImGui::Checkbox("Use frozen B", &comparison.useFrozenB);
            ImGui::TextWrapped("%s", comparison.Message().c_str());
        }
        if (ImGui::CollapsingHeader("Replay"))
        {
            ImGui::Text("Tick %llu | %.3f s | %zu samples", (unsigned long long)replay.GetFrame().tick,
                replay.GetFrame().time, replay.Track().samples.size());
            bool fixed = replay.IsFixedStep(); float rate = float(1.0 / replay.FixedDelta());
            ImGui::BeginDisabled(replay.GetMode() != ReplayController::Mode::Live);
            if (ImGui::Checkbox("Fixed timestep", &fixed)) replay.SetFixedStep(fixed, replay.FixedDelta());
            if (ImGui::SliderFloat("Simulation Hz", &rate, 1.f, 240.f)) replay.SetFixedStep(fixed, 1.0 / rate);
            int seed = int(replay.Track().seed & 0x7fffffff);
            if (ImGui::InputInt("Seed", &seed)) replay.SetSeed(uint32_t(seed));
            ImGui::EndDisabled();
            if (ImGui::Button("Record")) replay.Record();
            ImGui::SameLine(); if (ImGui::Button("Play")) { if (!replay.Play()) m_Message = "No recorded samples."; }
            ImGui::SameLine(); if (ImGui::Button("Stop")) replay.Stop();
            if (ImGui::Button(replay.IsPaused() ? "Resume" : "Pause")) replay.Pause(!replay.IsPaused());
            ImGui::SameLine(); if (ImGui::Button("Step")) replay.Step();
            ImGui::SameLine(); if (ImGui::Button("Restart"))
            { if (replay.GetMode() == ReplayController::Mode::Recording) replay.Record(); else replay.Restart(); }
            ImGui::InputText("Replay file", m_ReplayPath, sizeof(m_ReplayPath));
            if (ImGui::Button("Save track")) m_Message = replay.Save(m_ReplayPath).ToStringWithCode();
            ImGui::SameLine(); if (ImGui::Button("Load track")) m_Message = replay.Load(m_ReplayPath).ToStringWithCode();
            ImGui::TextWrapped("%s", m_Message.c_str());
        }
    }
}
