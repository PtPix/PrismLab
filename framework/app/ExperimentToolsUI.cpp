#include "ExperimentTools.h"
#include <imgui.h>

namespace Prism::Host
{
	void FExperimentTools::BuildUI()
	{
		if (ImGui::CollapsingHeader("Shaders"))
		{
			ImGui::BeginDisabled(Reload.Running());
			if (ImGui::Button("Compile and reload (F6)"))
				Reload.Request();
			ImGui::EndDisabled();
			ImGui::TextWrapped("%s", Reload.GetMessage().c_str());
		}
		if (ImGui::CollapsingHeader("Comparison"))
		{
			const char* Modes[] = {"Off", "A", "B", "Side by side", "Wipe", "Absolute difference"};
			int Mode = int(Comparison.Settings.Mode);
			if (ImGui::Combo("Mode", &Mode, Modes, 6))
				Comparison.Settings.Mode = Gpu::EComparisonMode(Mode);
			auto SourcePicker = [&](const char* Label, std::string& Selected)
			{
				if (!ImGui::BeginCombo(Label, Selected.c_str()))
					return;
				for (const FComparisonController::FSource& Source : Comparison.GetSources())
					if (ImGui::Selectable(Source.Id.c_str(), Source.Id == Selected))
						Selected = Source.Id;
				ImGui::EndCombo();
			};
			SourcePicker("Image A", Comparison.SourceA);
			SourcePicker("Image B", Comparison.SourceB);
			ImGui::SliderFloat("Split", &Comparison.Settings.Split, 0.f, 1.f);
			ImGui::SliderFloat("Difference gain", &Comparison.Settings.Gain, 0.1f, 100.f, "%.2f",
							   ImGuiSliderFlags_Logarithmic);
			if (ImGui::Button("Freeze B"))
				Comparison.RequestFreeze();
			ImGui::SameLine();
			if (ImGui::Button("Release frozen B"))
				Comparison.ClearFrozen();
			ImGui::Checkbox("Use frozen B", &Comparison.bUseFrozenB);
			ImGui::TextWrapped("%s", Comparison.GetMessage().c_str());
		}
		if (ImGui::CollapsingHeader("Replay"))
		{
			ImGui::Text("Tick %llu | %.3f s | %zu samples", (unsigned long long)Replay.GetFrame().Tick,
						Replay.GetFrame().Time, Replay.GetTrack().Samples.size());
			bool bFixed = Replay.IsFixedStep();
			float Rate = float(1.0 / Replay.FixedDelta());
			ImGui::BeginDisabled(Replay.GetMode() != FReplayController::EMode::Live);
			if (ImGui::Checkbox("Fixed timestep", &bFixed))
				Replay.SetFixedStep(bFixed, Replay.FixedDelta());
			if (ImGui::SliderFloat("Simulation Hz", &Rate, 1.f, 240.f))
				Replay.SetFixedStep(bFixed, 1.0 / Rate);
			int Seed = int(Replay.GetTrack().Seed & 0x7fffffff);
			if (ImGui::InputInt("Seed", &Seed))
				Replay.SetSeed(uint32_t(Seed));
			ImGui::EndDisabled();
			if (ImGui::Button("Record"))
				Replay.Record();
			ImGui::SameLine();
			if (ImGui::Button("Play"))
			{
				if (!Replay.Play())
					Message = "No recorded samples.";
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop"))
				Replay.Stop();
			if (ImGui::Button(Replay.IsPaused() ? "Resume" : "Pause"))
				Replay.Pause(!Replay.IsPaused());
			ImGui::SameLine();
			if (ImGui::Button("Step"))
				Replay.Step();
			ImGui::SameLine();
			if (ImGui::Button("Restart"))
			{
				if (Replay.GetMode() == FReplayController::EMode::Recording)
					Replay.Record();
				else
					Replay.Restart();
			}
			ImGui::InputText("Replay file", ReplayPath, sizeof(ReplayPath));
			if (ImGui::Button("Save track"))
				Message = Replay.Save(ReplayPath).ToStringWithCode();
			ImGui::SameLine();
			if (ImGui::Button("Load track"))
				Message = Replay.Load(ReplayPath).ToStringWithCode();
			ImGui::TextWrapped("%s", Message.c_str());
		}
	}
} // namespace Prism::Host
