#pragma once
#include <framework/render/data/CameraPose.h>
#include <framework/core/Status.h>
#include <donut/core/json.h>
#include <filesystem>
#include <vector>

namespace prism::host
{
    struct ReplaySample
    {
        CameraPose camera;
        Json::Value parameters;
    };
    struct ReplayTrack
    {
        double fixedDelta = 1.0 / 60.0;
        uint32_t seed = 1;
        std::vector<ReplaySample> samples;
        Status Save(const std::filesystem::path& path) const;
        Status Load(const std::filesystem::path& path);
    };
}
