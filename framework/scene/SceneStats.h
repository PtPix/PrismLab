#pragma once
#include <cstdint>
namespace Prism
{
	struct FSceneStats
	{
		uint32_t Meshes = 0;
		uint32_t Instances = 0;
		uint32_t Lights = 0;
		uint32_t Vertices = 0;
		uint32_t Triangles = 0;
	};

} // namespace Prism
