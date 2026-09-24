#pragma once
#include <framework/render/RenderServices.h>
#include <framework/scene/SceneStats.h>
#include <framework/scene/SceneSurfacePipeline.h>
#include <framework/render/presentation/DisplayChain.h>
#include <framework/render/temporal/TemporalServices.h>
#include "HostConfig.h"
#include <filesystem>
#include <functional>
namespace prism::host
{
    class ComparisonController;
    class ReplayController;
    class DebugViewRegistry;
    class Metrics;

    struct ExperimentCallbacks
    {

        std::function<void(prism::HistoryResetReason)> requestHistoryReset;

        std::function<bool(nvrhi::ITexture*, const std::filesystem::path&, nvrhi::ResourceStates)> saveTexture;

        std::function<void()> requestQuit;
    };


    struct ExperimentContext
    {

        struct SceneServices
        {
            // Optional external producers; the host does not populate these resources.
            const prism::SceneFrameData* frameData = nullptr;
            const gpu::SceneGpuData* gpuData = nullptr;
            pipeline::ISceneSurfacePipeline* surfacePipeline = nullptr;

            SceneStats stats;
            std::string description = "(none)";
        };

        struct OutputServices
        {
            gpu::IDisplayChain* displayChain = nullptr;
            ColorSpace colorSpace = ColorSpace::SceneLinear;
        };
        struct ToolServices
        {
            ComparisonController* comparison = nullptr;
            ReplayController* replay = nullptr;
            DebugViewRegistry* debugViews = nullptr;
            Metrics* metrics = nullptr;
        };
        struct TemporalState
        {
            gpu::ITemporalServices* services = nullptr;
            uint32_t jitterSampleCount = 0;
        };
        gpu::RenderServices gpu;
        SceneServices scene;
        OutputServices output;
        ToolServices tools;
        TemporalState temporal;
        ExperimentCallbacks callbacks;

        const host::HostConfig* config = nullptr;
        std::filesystem::path assetsDirectory;
    };

}
