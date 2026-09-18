#pragma once

#include <donut/core/math/math.h>

#include <cstdint>
#include <filesystem>

namespace renderlab
{
    // Camera / window / lighting presets read by the host at startup.
    // Defaults come from configs/host/camera_default.json in the source tree and can be
    // overridden with --config <path>. If the file is missing, the values below are used.

    struct HostCameraPreset
    {
        dm::float3 position = dm::float3(0.f, 1.6f, -6.f);
        dm::float3 target = dm::float3(0.f, 0.8f, 0.f);
        float fovDegrees = 60.f;
        float zNear = 0.05f;
        float zFar = 100.f;
        float moveSpeed = 2.f;
    };

    struct HostWindowPreset
    {
        uint32_t width = 1280;
        uint32_t height = 720;
        bool vsync = true;
    };

    struct HostLightingPreset
    {
        dm::float3 sunDirection = dm::float3(0.45f, -1.f, 0.35f);
        float sunIrradiance = 2.2f;
        float ambientIntensity = 0.18f;
    };

    struct HostConfig
    {
        HostCameraPreset camera;
        HostWindowPreset window;
        HostLightingPreset lighting;

        std::filesystem::path sourcePath;
        bool loadedFromFile = false;
    };

    HostConfig LoadHostConfig(const std::filesystem::path& explicitPath = std::filesystem::path());
}
