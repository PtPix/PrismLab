#pragma once

// Host layer: the shared ImGui overlay.
//
// Shows what every experiment would otherwise re-implement: renderer, resolution, frame time, camera,
// scene, per-pass GPU timings, controls, the log console. An experiment only contributes its own parameter
// section through Experiment::BuildUI.

#include "ExperimentHost.h"

#include <donut/app/imgui_console.h>
#include <donut/app/imgui_renderer.h>

#include <memory>

namespace Prism::Host
{
	class FUiOverlay final : public donut::app::ImGui_Renderer
	{
	  public:
		FUiOverlay(donut::app::DeviceManager* DeviceManager, FHostStats& Stats, FExperimentRenderPass& Host);

		bool Initialize(const std::shared_ptr<donut::engine::ShaderFactory>& ShaderFactory);

	  protected:
		void buildUI() override;

	  private:
		void BuildHeaderSection();
		void BuildCameraSection();
		void BuildSceneSection();
		void BuildTimingSection();
		void BuildDebugViewSection();
		void BuildMetricsSection();

		FHostStats& Stats;
		FExperimentRenderPass& Host;
		std::unique_ptr<donut::app::ImGui_Console> Console;
		bool bTimingEnabled = true;
	};
} // namespace Prism::Host
