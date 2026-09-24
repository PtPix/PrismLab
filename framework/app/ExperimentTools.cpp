#include "ExperimentTools.h"

namespace prism::host
{
    Status ExperimentTools::Initialize(nvrhi::IDevice* device, gpu::ShaderLibrary& shaders,
        donut::engine::CommonRenderPasses& common, const std::filesystem::path& executable)
    {
        reload.Initialize(device, shaders, executable);
        return comparison.Initialize(device, shaders, common);
    }
    bool ExperimentTools::PrepareFrame(float elapsed, adapter::CameraController& camera, Extent2D size)
    {
        const bool reloaded = reload.Poll();
        const auto& frame = replay.BeginFrame(elapsed);
        if (const auto* sample = replay.PlaybackSample()) camera.ApplyPose(sample->camera);
        const bool animate = replay.GetMode() != ReplayController::Mode::Playback && frame.delta > 0.f;
        camera.Update(frame.delta, size, animate);
        comparison.BeginFrame();
        return replay.ConsumeReset() || reloaded;
    }
}
