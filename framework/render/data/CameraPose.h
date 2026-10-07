#pragma once
#include "CameraData.h"

namespace Prism
{
	struct FCameraPose
	{
		dm::float3 Position = dm::float3(0.f);
		dm::float3 Direction = dm::float3(0.f, 0.f, 1.f);
		dm::float3 Up = dm::float3(0.f, 1.f, 0.f);
		float FovRadians = dm::radians(60.f);
		float NearPlane = 0.05f;
		float FarPlane = 100.f;
		static FCameraPose From(const FCameraData& C)
		{
			return {C.Position, C.Forward, C.Up, C.VerticalFovRadians, C.ZNearMeters, C.ZFarMeters};
		}
	};
} // namespace Prism
