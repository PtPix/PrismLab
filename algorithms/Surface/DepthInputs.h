#pragma once

#include <nvrhi/nvrhi.h>

#include <array>
#include <cstdint>
#include <vector>

namespace Prism::Surface
{
	struct FDepthBuffers
	{
		nvrhi::IBuffer* Positions = nullptr;
		nvrhi::BufferRange PositionRange;
		nvrhi::IBuffer* Indices = nullptr;
		nvrhi::Format IndexFormat = nvrhi::Format::R32_UINT;
	};

	struct FDepthDraw
	{
		uint32_t BufferGroupIndex = 0;
		uint32_t FirstIndex = 0;
		uint32_t IndexCount = 0;
		uint32_t BaseVertex = 0;
		// Row-major, row-vector object-to-raster-clip matrix (16 floats).
		std::array<float, 16> ObjectToClip{};
	};

	struct FDepthBatch
	{
		std::vector<FDepthBuffers> BufferGroups;
		std::vector<FDepthDraw> Draws;
	};

	struct FDepthInputs
	{
		const FDepthBatch& Geometry;
		nvrhi::IFramebuffer* Target = nullptr;
	};
} // namespace Prism::Surface
