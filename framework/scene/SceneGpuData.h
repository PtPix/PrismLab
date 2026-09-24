#pragma once

#include <framework/core/Status.h>
#include <nvrhi/nvrhi.h>

namespace prism::gpu
{
    struct SceneBufferView
    {
        nvrhi::IBuffer* buffer = nullptr;
        uint64_t offset = 0;
        uint64_t count = 0;
        uint32_t stride = 0;
        uint64_t generation = 0;

        Status Validate() const
        {
            if (!buffer || !stride || !count)
                return Status::Error(ErrorCode::ResourceMissing, "scene buffer is incomplete");
            const uint64_t bytes = buffer->getDesc().byteSize;
            if (offset > bytes || count > (bytes - offset) / stride)
                return Status::Error(ErrorCode::InvalidArgument, "scene buffer range is out of bounds");
            return Status::Ok();
        }
    };

    // No allocation or packing is performed here. Layout is a producer/consumer contract.
    struct SceneGpuData
    {
        SceneBufferView vertices, indices, instances, materials, lights;
        nvrhi::IDescriptorTable* textures = nullptr;
        uint64_t revision = 0;
        const char* layoutId = nullptr;
    };
}
