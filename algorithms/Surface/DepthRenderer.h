#pragma once

#include <framework/render/passes/RasterPass.h>
#include <framework/render/RenderServices.h>
#include <framework/render/data/Conventions.h>

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

    class FDepthRenderer
    {
    public:
        FStatus Initialize(nvrhi::IDevice* Device, Gpu::FShaderLibrary& Shaders);

        FStatus Record(nvrhi::ICommandList* Commands, const FDepthBatch& Geometry,
                       const dm::float4x4& WorldToClip, EDepthConvention Convention,
                       nvrhi::ITexture* Depth, nvrhi::IFramebuffer* Target);

    private:
        Gpu::FRasterPass ForwardPass;
        Gpu::FRasterPass ReversePass;
        nvrhi::BindingLayoutHandle BindingLayout;
        nvrhi::InputLayoutHandle InputLayout;
    };
} // namespace Prism::Surface
