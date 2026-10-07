#include "ReplayController.h"
#include <algorithm>
#include <cmath>

namespace Prism::Host
{
	void FReplayController::Restart()
	{
		if (Mode == EMode::Recording)
			ReplayTrack.Samples.clear();
		CurrentFrame = {};
		bFirst = true;
		bReset = true;
		bStep = false;
	}
	void FReplayController::Record()
	{
		ReplayTrack.Samples.clear();
		Mode = EMode::Recording;
		bPaused = false;
		Restart();
	}
	bool FReplayController::Play()
	{
		if (ReplayTrack.Samples.empty())
			return false;
		Mode = EMode::Playback;
		bPaused = false;
		Restart();
		return true;
	}
	void FReplayController::Stop()
	{
		Mode = EMode::Live;
		bPaused = false;
		Restart();
	}
	void FReplayController::SetFixedStep(bool bEnabled, double Delta)
	{
		if (Mode != EMode::Live || !std::isfinite(Delta) || Delta <= 0 || Delta > 1)
			return;
		bFixed = bEnabled;
		ReplayTrack.FixedDelta = Delta;
		Restart();
	}
	const FReplayController::FFrame& FReplayController::BeginFrame(float RealDelta)
	{
		const bool bAdvance = !bFirst && (!bPaused || bStep);
		CurrentFrame.Delta = 0;
		if (bAdvance)
		{
			if (Mode == EMode::Playback && CurrentFrame.Tick + 1 >= ReplayTrack.Samples.size())
				bPaused = true;
			else
			{
				++CurrentFrame.Tick;
				CurrentFrame.Delta =
					float((bFixed || Mode != EMode::Live) ? ReplayTrack.FixedDelta : std::max(0.f, RealDelta));
				CurrentFrame.Time = (bFixed || Mode != EMode::Live) ? double(CurrentFrame.Tick) * ReplayTrack.FixedDelta
																	: CurrentFrame.Time + CurrentFrame.Delta;
			}
		}
		CurrentFrame.Seed = ReplayTrack.Seed;
		bStep = false;
		bFirst = false;
		if (const auto* Sample = PlaybackSample(); Sample && RestoreParameters)
			if (!CaptureParameters || CaptureParameters() != Sample->Parameters)
				RestoreParameters(Sample->Parameters);
		return CurrentFrame;
	}
	void FReplayController::EndFrame(const FCameraPose& Camera)
	{
		if (Mode == EMode::Recording && CurrentFrame.Tick == ReplayTrack.Samples.size())
			ReplayTrack.Samples.push_back({Camera, CaptureParameters ? CaptureParameters() : Json::Value()});
	}
	const FReplaySample* FReplayController::PlaybackSample() const
	{
		return Mode == EMode::Playback && CurrentFrame.Tick < ReplayTrack.Samples.size()
				   ? &ReplayTrack.Samples[size_t(CurrentFrame.Tick)]
				   : nullptr;
	}
	FStatus FReplayController::Load(const std::filesystem::path& Path)
	{
		if (Mode == EMode::Recording)
			return FStatus::Error(EErrorCode::InvalidArgument, "stop recording before loading");
		FStatus Status = ReplayTrack.Load(Path);
		if (Status)
			Stop();
		return Status;
	}
} // namespace Prism::Host
