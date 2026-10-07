#include "UiOverlay.h"

#include <donut/core/log.h>
#include <donut/engine/ConsoleInterpreter.h>

#include <imgui.h>

#include <algorithm>

namespace Prism::Host
{
	FUiOverlay::FUiOverlay(donut::app::DeviceManager* DeviceManager, FHostStats& Stats, FExperimentRenderPass& Host)
		: donut::app::ImGui_Renderer(DeviceManager), Stats(Stats), Host(Host)
	{
		donut::app::ImGui_Console::Options ConsoleOptions;
		ConsoleOptions.show_info = true;

		// 交互运行时日志转到控制台窗口；冒烟测试/截图/基准/参考图运行时保留文件日志，
		// 否则 CI 看不到初始化、自检与指标输出。
		ConsoleOptions.capture_log = !Host.GetCommandLine().WantsHeadlessRun();

		Console = std::make_unique<donut::app::ImGui_Console>(std::make_shared<donut::engine::console::Interpreter>(),
															  ConsoleOptions);
	}

	bool FUiOverlay::Initialize(const std::shared_ptr<donut::engine::ShaderFactory>& ShaderFactory)
	{
		return Init(ShaderFactory);
	}

	void FUiOverlay::buildUI()
	{
		IExperiment* Experiment = Host.GetExperiment();

		ImGui::SetNextWindowPos(ImVec2(20.f, 20.f), ImGuiCond_FirstUseEver);
		ImGui::Begin("Prism", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

		BuildHeaderSection();
		BuildCameraSection();
		BuildSceneSection();
		BuildTimingSection();
		BuildDebugViewSection();
		BuildMetricsSection();
		Host.GetTools().BuildUI();

		if (Experiment)
		{
			ImGui::SeparatorText("Experiment");
			if (Experiment->GetDescription() && Experiment->GetDescription()[0] != '\0')
				ImGui::TextWrapped("%s", Experiment->GetDescription());

			Experiment->BuildUI(Host.GetContext());
		}

		ImGui::SeparatorText("Controls");
		ImGui::BulletText("First person: WASD/QE to move, drag with left button to look");
		ImGui::BulletText("Orbit: drag with left button to rotate, wheel to zoom");

		ImGui::End();

		bool bConsoleOpen = true;
		Console->Render(&bConsoleOpen);
	}

	void FUiOverlay::BuildHeaderSection()
	{
		ImGui::Text("Experiment: %s", Host.GetExperiment() ? Host.GetExperiment()->GetName() : "(none)");
		ImGui::Text("Renderer: %s", Stats.RendererDescription.c_str());
		ImGui::Text("Render: %u x %u   Output: %u x %u", Stats.RenderSize.Width, Stats.RenderSize.Height,
					Stats.OutputSize.Width, Stats.OutputSize.Height);
		ImGui::Text("Frame: %.2f ms (%.1f FPS)   Frame index: %llu", Stats.FrameTimeMs, Stats.FramesPerSecond,
					(unsigned long long)Stats.FrameIndex);
		ImGui::Text("History reset: %s", Stats.HistoryResetDescription.c_str());
	}

	void FUiOverlay::BuildCameraSection()
	{
		Adapter::FCameraController& Camera = Host.GetCamera();

		ImGui::SeparatorText("Camera");
		ImGui::Text("Mode: %s", Stats.bFirstPerson ? "First person" : "Orbit (third person)");
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", Stats.CameraPosition.x, Stats.CameraPosition.y,
					Stats.CameraPosition.z);
		ImGui::Text("Direction: (%.2f, %.2f, %.2f)", Stats.CameraDirection.x, Stats.CameraDirection.y,
					Stats.CameraDirection.z);

		if (ImGui::Button("First person"))
			Camera.SwitchToFirstPerson(true);

		ImGui::SameLine();
		if (ImGui::Button("Orbit"))
			Camera.SwitchToThirdPerson(true);
	}

	void FUiOverlay::BuildSceneSection()
	{
		ImGui::SeparatorText("Scene");
		ImGui::Text("Source: %s", Stats.SceneDescription.c_str());
		ImGui::Text("Meshes: %u   Instances: %u", Stats.Scene.Meshes, Stats.Scene.Instances);
		ImGui::Text("Lights: %u   Triangles: %u", Stats.Scene.Lights, Stats.Scene.Triangles);
		ImGui::Text("Config: %s", Stats.ConfigDescription.c_str());
	}

	void FUiOverlay::BuildTimingSection()
	{
		Gpu::FGpuProfiler* Profiler = Host.GetContext().Gpu.Profiler;
		if (!Profiler)
			return;

		ImGui::SeparatorText("GPU timing");

		if (ImGui::Checkbox("Enable timestamps", &bTimingEnabled))
			Profiler->SetEnabled(bTimingEnabled);

		if (Profiler->IsEnabled())
		{
			const std::vector<Gpu::FGpuProfiler::FScopeTiming>& Timings = Profiler->GetTimings();
			if (Timings.empty())
			{
				ImGui::TextUnformatted("(waiting for the first resolved results)");
			}
			else if (ImGui::BeginTable("RendererTimings", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Scope");
				ImGui::TableSetupColumn("Last (ms)", ImGuiTableColumnFlags_WidthFixed, 80.f);
				ImGui::TableSetupColumn("Avg (ms)", ImGuiTableColumnFlags_WidthFixed, 80.f);
				ImGui::TableHeadersRow();

				for (const Gpu::FGpuProfiler::FScopeTiming& Timing : Timings)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(Timing.Name.c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%.3f", Timing.bValid ? Timing.Milliseconds : 0.f);
					ImGui::TableNextColumn();
					ImGui::Text("%.3f", Timing.bValid ? Timing.SmoothedMilliseconds : 0.f);
				}

				ImGui::EndTable();
			}

			ImGui::Text("Measured total: %.3f ms", Profiler->GetTotalMilliseconds());
		}
	}

	void FUiOverlay::BuildDebugViewSection()
	{
		FDebugViewRegistry& DebugViews = Host.GetDebugViews();
		const std::vector<FDebugViewEntry>& Entries = DebugViews.GetEntries();

		ImGui::SeparatorText("Debug view");

		if (Entries.empty())
		{
			ImGui::TextUnformatted("(the experiment publishes no intermediate results)");
			return;
		}

		// 标签字符串必须先落到本地存储里：GetExperimentel() 返回的是临时对象。
		std::vector<std::string> LabelStorage;
		LabelStorage.reserve(Entries.size());
		for (const FDebugViewEntry& Entry : Entries)
			LabelStorage.push_back(Entry.GetExperimentel());

		std::vector<const char*> Labels;
		Labels.reserve(LabelStorage.size() + 1);
		Labels.push_back("(experiment output)");
		for (const std::string& Label : LabelStorage)
			Labels.push_back(Label.c_str());

		int Selected = std::clamp(DebugViews.GetSelectedIndex(), 0, int(Labels.size()) - 1);

		if (ImGui::Combo("Texture", &Selected, Labels.data(), int(Labels.size())))
			DebugViews.SetSelectedIndex(Selected);

		if (DebugViews.GetSelected())
		{
			Gpu::FDebugViewSettings Settings = DebugViews.GetSelectedSettings();

			const char* const* ModeNames = Gpu::GetDebugViewModeNames();
			int Mode = int(Settings.Mode);

			if (ImGui::Combo("Channel", &Mode, ModeNames, int(Gpu::EDebugViewMode::Count)))
				Settings.Mode = Gpu::EDebugViewMode(Mode);

			ImGui::SliderFloat("Scale", &Settings.Scale, 0.01f, 16.f);
			ImGui::SliderFloat("Bias", &Settings.Bias, -1.f, 1.f);

			DebugViews.SetSelectedSettings(Settings);
		}
	}

	void FUiOverlay::BuildMetricsSection()
	{
		const FMetrics& Metrics = Host.GetMetrics();
		const std::vector<FMetrics::FSeries>& Series = Metrics.GetSeries();

		if (Series.empty())
			return;

		ImGui::SeparatorText("Metrics");

		if (!ImGui::BeginTable("PrismMetrics", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			return;

		ImGui::TableSetupColumn("Metric");
		ImGui::TableSetupColumn("Last", ImGuiTableColumnFlags_WidthFixed, 70.f);
		ImGui::TableSetupColumn("Mean", ImGuiTableColumnFlags_WidthFixed, 70.f);
		ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed, 70.f);
		ImGui::TableHeadersRow();

		for (const FMetrics::FSeries& Item : Series)
		{
			if (Item.Samples == 0)
				continue;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(Item.Name.c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", Item.Last);
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", Item.Mean);
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", Item.Max);
		}

		ImGui::EndTable();

		ImGui::Text("Measured frames: %llu", (unsigned long long)Metrics.GetMeasuredFrameCount());
	}
} // namespace Prism::Host
