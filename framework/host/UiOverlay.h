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

namespace prism::host
{
    class UiOverlay final : public donut::app::ImGui_Renderer
    {
    public:
        UiOverlay(donut::app::DeviceManager* deviceManager, HostStats& stats, ExperimentRenderPass& host);

        bool Initialize(const std::shared_ptr<donut::engine::ShaderFactory>& shaderFactory);

    protected:
        void buildUI() override;

    private:
        void BuildHeaderSection();
        void BuildCameraSection();
        void BuildSceneSection();
        void BuildTimingSection();
        void BuildDebugViewSection();
        void BuildMetricsSection();

        HostStats& m_Stats;
        ExperimentRenderPass& m_Host;
        std::unique_ptr<donut::app::ImGui_Console> m_Console;
        bool m_TimingEnabled = true;
    };
}
