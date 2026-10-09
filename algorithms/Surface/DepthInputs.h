#pragma once
#include <framework/render/data/CameraData.h>
#include <nvrhi/nvrhi.h>
#include <vector>

namespace Prism::Surface
{
    struct FDepthBuffers
    {
        nvrhi::IBuffer*    Positions = nullptr;
        nvrhi::BufferRange PositionRange;
        uint32_t           PositionStride = sizeof(dm::float3);

        nvrhi::IBuffer*    Indices = nullptr;
        nvrhi::Format      IndexFormat = nvrhi::Format::R32_UINT;
    };

    struct FDepthDraw
    {
        uint32_t    BufferGroupIndex = 0;
        uint32_t    FirstIndex = 0;
        uint32_t    IndexCount = 0;
        uint32_t    BaseVertex = 0;
        dm::affine3 ObjectToWorld = dm::affine3::identity();
    };

    struct FDepthBatch
    {
        std::vector<FDepthBuffers> BufferGroups;
        std::vector<FDepthDraw>    Draws;
    };

    struct FDepthInputs
    {
        const FDepthBatch& Geometry;
        const FCameraData& Camera;
        nvrhi::IFramebuffer* Target = nullptr;
    };
}