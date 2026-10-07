#pragma once

#include "framework/render/data/FrameInfo.h"
#include <cstddef>

namespace Prism
{
	using FSceneId = uint64_t;

	struct FInstanceFrameData
	{
		FSceneId Id = 0;
		dm::affine3 Current = dm::affine3::identity();
		dm::affine3 Previous = dm::affine3::identity();
		dm::box3 WorldBounds;
		uint32_t MaterialIndex = 0;
		bool bHasPrevious = false;
		bool bChanged = false;
	};

	// Borrowed data. The producer keeps it valid until frame recording ends.
	struct FSceneFrameData
	{
		uint64_t Revision = 0;
		const FInstanceFrameData* Instances = nullptr;
		size_t InstanceCount = 0;
		bool bTransformsChanged = false;
		bool bMaterialsChanged = false;
		bool bLightsChanged = false;
		bool bTopologyChanged = false;
	};
} // namespace Prism
