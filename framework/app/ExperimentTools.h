#pragma once
#include "framework/tools/comparison/ComparisonController.h"
#include "framework/tools/replay/ReplayController.h"
#include "framework/tools/shaders/ShaderReload.h"
#include <framework/adapters/donut/CameraController.h>

namespace prism::host
{
    class ExperimentTools
    {
    public:
        Status Initialize(nvrhi::IDevice* device, gpu::ShaderLibrary& shaders,
            donut::engine::CommonRenderPasses& common, const std::filesystem::path& executable);
        bool PrepareFrame(float elapsed, adapter::CameraController& camera, Extent2D size);
        void EndFrame(const CameraData& camera) { replay.EndFrame(CameraPose::From(camera)); }
        void BuildUI();
        ComparisonController comparison;
        ReplayController replay;
        ShaderReload reload;
    private:
        char m_ReplayPath[512] = "replay.json";
        std::string m_Message;
    };
}
