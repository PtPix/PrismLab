#include "ExperimentTools.h"

namespace Prism::Host
{
	FStatus FExperimentTools::Initialize(nvrhi::IDevice* Device, Gpu::FShaderLibrary& Shaders,
										 donut::engine::CommonRenderPasses& Common,
										 const std::filesystem::path& Executable)
	{
		Reload.Initialize(Device, Shaders, Executable);
		return Comparison.Initialize(Device, Shaders, Common);
	}
	bool FExperimentTools::PrepareFrame(float Elapsed, Adapter::FCameraController& Camera, FExtent2D Size)
	{
		const bool bReloaded = Reload.Poll();
		const FReplayController::FFrame& Frame = Replay.BeginFrame(Elapsed);
		if (const FReplaySample* Sample = Replay.PlaybackSample())
			Camera.ApplyPose(Sample->Camera);
		const bool bAnimate = Replay.GetMode() != FReplayController::EMode::Playback && Frame.Delta > 0.f;
		Camera.Update(Frame.Delta, Size, bAnimate);
		Comparison.BeginFrame();
		return Replay.ConsumeReset() || bReloaded;
	}
} // namespace Prism::Host
