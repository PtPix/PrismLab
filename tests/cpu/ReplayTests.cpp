#include <framework/tools/replay/ReplayController.h>
#include <framework/adapters/donut/CameraController.h>
#include <donut/core/log.h>
#include <fstream>
#include <cmath>

bool RunReplayTests(const std::filesystem::path& Directory)
{
	using namespace Prism;
	Host::FReplayController Replay;
	Json::Value Parameters;
	Parameters["variant"] = 3;
	Replay.CaptureParameters = [&]
	{
		return Parameters;
	};
	Replay.RestoreParameters = [&](const Json::Value& Value)
	{
		Parameters = Value;
	};
	bool bPassed = true;
	auto Check = [&](bool bValue, const char* Name)
	{
		if (!bValue)
		{
			donut::log::error("Replay test failed: %s", Name);
			bPassed = false;
		}
	};
	Replay.SetFixedStep(true, 0.02);
	Replay.SetSeed(123);
	Replay.Record();
	FCameraPose Pose;
	for (uint64_t I = 0; I < 4; ++I)
	{
		const auto F = Replay.BeginFrame(0.37f);
		Check(F.Tick == I && std::abs(F.Time - double(I) * .02) < 1e-8 && F.Seed == 123, "record clock");
		Pose.Position.x = float(I);
		Parameters["variant"] = int(I);
		Replay.EndFrame(Pose);
	}
	Replay.Pause(true);
	Replay.BeginFrame(.1f);
	Replay.EndFrame(Pose);
	Check(Replay.GetTrack().Samples.size() == 4, "pause does not duplicate samples");
	Replay.Step();
	Check(Replay.BeginFrame(.1f).Tick == 4, "single step");
	Replay.EndFrame(Pose);
	Replay.Stop();
	const auto File = Directory / "replay-test.json";
	Check(bool(Replay.Save(File)), "save");
	Host::FReplayController Loaded;
	Check(bool(Loaded.Load(File)) && Loaded.GetTrack().Samples.size() == 5, "round trip");
	Check(Loaded.GetTrack().Seed == 123 && Loaded.GetTrack().Samples[2].Camera.Position.x == 2, "round trip values");
	Check(Replay.Play(), "play");
	Replay.BeginFrame(.4f);
	Check(Parameters["variant"].asInt() == 0, "restore first parameters");
	Replay.BeginFrame(.4f);
	Check(Parameters["variant"].asInt() == 1, "restore next parameters");
	for (int I = 0; I < 6; ++I)
		Replay.BeginFrame(.4f);
	Check(Replay.IsPaused() && Replay.GetFrame().Tick == 4, "pause at track end");
	Replay.Restart();
	Check(Replay.BeginFrame(.4f).Tick == 0 && Replay.ConsumeReset(), "restart resets history");
	const auto Invalid = Directory / "replay-invalid.json";
	{
		std::ofstream Out(Invalid);
		Out << R"({"version":1,"seed":1,"fixedDelta":0,"samples":[]})";
	}
	Check(!Loaded.Load(Invalid) && Loaded.GetTrack().Samples.size() == 5, "invalid load is transactional");
	{
		std::ofstream Out(Invalid);
		Out << "[]";
	}
	Check(!Loaded.Load(Invalid), "invalid root type");
	Adapter::FCameraController Camera;
	Camera.Initialize(FCameraPreset{});
	Pose.Up = dm::normalize(dm::float3(1, 1, 0));
	Camera.ApplyPose(Pose);
	Camera.Update(0.f, {640, 360}, false);
	Check(dm::length(Camera.GetCameraData().Up - Pose.Up) < 1e-5f, "replay preserves camera roll");
	Replay.Record();
	Replay.BeginFrame(.1f);
	Replay.EndFrame(Pose);
	Replay.Restart();
	Check(Replay.GetTrack().Samples.empty(), "record restart clears old samples");
	if (bPassed)
		donut::log::info("Replay tests passed.");
	return bPassed;
}
