#pragma once
#include "ReplayTrack.h"
#include <functional>

namespace Prism::Host
{
	class FReplayController
	{
	  public:
		enum class EMode
		{
			Live,
			Recording,
			Playback
		};
		struct FFrame
		{
			uint64_t Tick = 0;
			double Time = 0;
			float Delta = 0;
			uint32_t Seed = 1;
		};
		// Register sample-owned parameters. Scene animation consumes Frame::time independently.
		std::function<Json::Value()> CaptureParameters;
		std::function<void(const Json::Value&)> RestoreParameters;
		void Record();
		bool Play();
		void Stop();
		void Restart();
		void Pause(bool bInPaused)
		{
			bPaused = bInPaused;
		}
		void Step()
		{
			bPaused = true;
			bStep = true;
		}
		bool IsPaused() const
		{
			return bPaused;
		}
		EMode GetMode() const
		{
			return Mode;
		}
		bool BlocksInput() const
		{
			return Mode == EMode::Playback || bPaused;
		}
		void SetFixedStep(bool bEnabled, double Delta);
		bool IsFixedStep() const
		{
			return bFixed;
		}
		double FixedDelta() const
		{
			return ReplayTrack.FixedDelta;
		}
		void SetSeed(uint32_t Seed)
		{
			if (Mode == EMode::Live)
			{
				ReplayTrack.Seed = Seed;
				bReset = true;
			}
		}
		const FFrame& BeginFrame(float RealDelta);
		void EndFrame(const FCameraPose& Camera);
		const FReplaySample* PlaybackSample() const;
		bool ConsumeReset()
		{
			bool bValue = bReset;
			bReset = false;
			return bValue;
		}
		const FFrame& GetFrame() const
		{
			return CurrentFrame;
		}
		const FReplayTrack& GetTrack() const
		{
			return ReplayTrack;
		}
		FStatus Save(const std::filesystem::path& Path) const
		{
			return ReplayTrack.Save(Path);
		}
		FStatus Load(const std::filesystem::path& Path);

	  private:
		FReplayTrack ReplayTrack;
		FFrame CurrentFrame;
		EMode Mode = EMode::Live;
		bool bFixed = false;
		bool bPaused = false;
		bool bStep = false;
		bool bFirst = true;
		bool bReset = true;
	};
} // namespace Prism::Host
