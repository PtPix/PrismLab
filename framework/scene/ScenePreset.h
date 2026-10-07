#pragma once
#include <framework/core/Types.h>
#include <string>
namespace Prism
{
	struct FLightingPreset
	{
		dm::float3 SunDirection = dm::float3(0.45f, -1.f, 0.35f);
		float SunIrradiance = 2.2f;
		float AmbientIntensity = 0.18f;
	};
	struct FScenePreset
	{
		// Used only by samples choosing a scene adapter.
		std::string Source = "procedural";
		std::string Asset;
	};
} // namespace Prism
