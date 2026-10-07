#pragma once
#include "framework/tools/comparison/ComparisonController.h"
#include "framework/tools/replay/ReplayController.h"
#include "framework/tools/shaders/ShaderReload.h"
#include <framework/adapters/donut/CameraController.h>

namespace Prism::Host
{
	class FExperimentTools
	{
	  public:
		FStatus Initialize(nvrhi::IDevice* Device, Gpu::FShaderLibrary& Shaders,
						   donut::engine::CommonRenderPasses& Common, const std::filesystem::path& Executable);
		bool PrepareFrame(float Elapsed, Adapter::FCameraController& Camera, FExtent2D Size);
		void EndFrame(const FCameraData& CameraData)
		{
			Replay.EndFrame(FCameraPose::From(CameraData));
		}
		void BuildUI();
		FComparisonController Comparison;
		FReplayController Replay;
		FShaderReload Reload;

	  private:
		char ReplayPath[512] = "replay.json";
		std::string Message;
	};
} // namespace Prism::Host
