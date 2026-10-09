#pragma once
#include <nvrhi/nvrhi.h>

#include <cstdint>

namespace Prism::Surface
{
	struct FDepthDebugInfo
	{
		uint32_t DrawCount = 0;
		uint64_t TriangleCount = 0;
		bool bCleared = false;
	};

	struct FDepthOutputs
	{
		nvrhi::ITexture* Depth = nullptr;
		FDepthDebugInfo Debug;
	};
} // namespace Prism::Surface
