#pragma once
#include "CameraData.h"

namespace prism
{
    struct CameraPose
    {
        dm::float3 position = dm::float3(0.f);
        dm::float3 direction = dm::float3(0.f, 0.f, 1.f);
        dm::float3 up = dm::float3(0.f, 1.f, 0.f);
        float fovRadians = dm::radians(60.f);
        float nearPlane = 0.05f;
        float farPlane = 100.f;
        static CameraPose From(const CameraData& c)
        { return {c.position, c.forward, c.up, c.verticalFovRadians, c.zNearMeters, c.zFarMeters}; }
    };
}
