#pragma once

#include <framework/core/Status.h>
#include <nvrhi/nvrhi.h>

namespace Prism::Gpu
{
	struct FSceneBufferView
	{
		nvrhi::IBuffer* Buffer = nullptr;
		uint64_t Offset = 0;
		uint64_t Count = 0;
		uint32_t Stride = 0;
		uint64_t Generation = 0;

		FStatus Validate() const
		{
			if (!Buffer || !Stride || !Count)
				return FStatus::Error(EErrorCode::ResourceMissing, "scene buffer is incomplete");
			const uint64_t Bytes = Buffer->getDesc().byteSize;
			if (Offset > Bytes || Count > (Bytes - Offset) / Stride)
				return FStatus::Error(EErrorCode::InvalidArgument, "scene buffer range is out of bounds");
			return FStatus::Ok();
		}
	};

	// No allocation or packing is performed here. Layout is a producer/consumer contract.
	struct FSceneGpuData
	{
		FSceneBufferView Vertices, Indices, Instances, Materials, Lights;
		nvrhi::IDescriptorTable* Textures = nullptr;
		uint64_t Revision = 0;
		const char* LayoutId = nullptr;
	};
} // namespace Prism::Gpu
