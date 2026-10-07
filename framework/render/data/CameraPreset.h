#pragma once
#include <framework/core/Types.h>
namespace Prism
{
	struct FCameraPreset
	{
		dm::float3 Position = dm::float3(0.f, 1.6f, -6.f);
		dm::float3 Target = dm::float3(0.f, 0.8f, 0.f);
		float FovDegrees = 60.f;
		float ZNear = 0.05f;
		float ZFar = 100.f;
		float MoveSpeed = 2.f;
	};
} // namespace Prism
