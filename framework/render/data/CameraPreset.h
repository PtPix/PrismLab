#pragma once
#include <framework/core/Types.h>
namespace prism
{
    struct CameraPreset
    {
        dm::float3 position = dm::float3(0.f, 1.6f, -6.f);
        dm::float3 target = dm::float3(0.f, 0.8f, 0.f);
        float fovDegrees = 60.f;
        float zNear = 0.05f;
        float zFar = 100.f;
        float moveSpeed = 2.f;
    };
}
