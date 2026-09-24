#pragma once
#include <framework/core/Types.h>
#include <string>
namespace prism
{
    struct LightingPreset
    {
        dm::float3 sunDirection = dm::float3(0.45f, -1.f, 0.35f);
        float sunIrradiance = 2.2f;
        float ambientIntensity = 0.18f;
    };
    struct ScenePreset
    {
        // Used only by samples choosing a scene adapter.
        std::string source = "procedural";
        std::string asset;
    };
}
