#include <framework/tools/replay/ReplayController.h>
#include <framework/adapters/donut/CameraController.h>
#include <donut/core/log.h>
#include <fstream>
#include <cmath>

bool RunReplayTests(const std::filesystem::path& directory)
{
    using namespace prism;
    host::ReplayController replay;
    Json::Value parameters; parameters["variant"] = 3;
    replay.captureParameters = [&] { return parameters; };
    replay.restoreParameters = [&](const Json::Value& value) { parameters = value; };
    bool passed = true;
    auto check = [&](bool value, const char* name) {
        if (!value) { donut::log::error("Replay test failed: %s", name); passed = false; }
    };
    replay.SetFixedStep(true, 0.02); replay.SetSeed(123); replay.Record();
    CameraPose pose;
    for (uint64_t i = 0; i < 4; ++i)
    {
        const auto f = replay.BeginFrame(0.37f);
        check(f.tick == i && std::abs(f.time - double(i) * .02) < 1e-8 && f.seed == 123, "record clock");
        pose.position.x = float(i); parameters["variant"] = int(i); replay.EndFrame(pose);
    }
    replay.Pause(true); replay.BeginFrame(.1f); replay.EndFrame(pose);
    check(replay.Track().samples.size() == 4, "pause does not duplicate samples");
    replay.Step(); check(replay.BeginFrame(.1f).tick == 4, "single step"); replay.EndFrame(pose);
    replay.Stop();
    const auto file = directory / "replay-test.json";
    check(bool(replay.Save(file)), "save");
    host::ReplayController loaded;
    check(bool(loaded.Load(file)) && loaded.Track().samples.size() == 5, "round trip");
    check(loaded.Track().seed == 123 && loaded.Track().samples[2].camera.position.x == 2, "round trip values");
    check(replay.Play(), "play");
    replay.BeginFrame(.4f); check(parameters["variant"].asInt() == 0, "restore first parameters");
    replay.BeginFrame(.4f); check(parameters["variant"].asInt() == 1, "restore next parameters");
    for (int i = 0; i < 6; ++i) replay.BeginFrame(.4f);
    check(replay.IsPaused() && replay.GetFrame().tick == 4, "pause at track end");
    replay.Restart(); check(replay.BeginFrame(.4f).tick == 0 && replay.ConsumeReset(), "restart resets history");
    const auto invalid = directory / "replay-invalid.json";
    { std::ofstream out(invalid); out << R"({"version":1,"seed":1,"fixedDelta":0,"samples":[]})"; }
    check(!loaded.Load(invalid) && loaded.Track().samples.size() == 5, "invalid load is transactional");
    { std::ofstream out(invalid); out << "[]"; }
    check(!loaded.Load(invalid), "invalid root type");
    adapter::CameraController camera;
    camera.Initialize(CameraPreset{});
    pose.up = dm::normalize(dm::float3(1, 1, 0));
    camera.ApplyPose(pose); camera.Update(0.f, {640, 360}, false);
    check(dm::length(camera.GetCameraData().up - pose.up) < 1e-5f, "replay preserves camera roll");
    replay.Record(); replay.BeginFrame(.1f); replay.EndFrame(pose); replay.Restart();
    check(replay.Track().samples.empty(), "record restart clears old samples");
    if (passed) donut::log::info("Replay tests passed.");
    return passed;
}
