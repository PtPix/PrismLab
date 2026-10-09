#include "DepthRenderer.h"

using donut::math::float4x4;
#include "shaders/SurfaceDepthCb.h"

namespace dm = donut::math;

namespace Prism::Surface
{
    static_assert(sizeof(FSurfaceDepthConstants) == sizeof(dm::float4x4), "depth constants layout changed");

    FStatus FDepthRenderer::Initialize(nvrhi::IDevice* Device, Gpu::FShaderLibrary& Shaders)
    {
        if (!Device)
        {
            return FStatus::Error(EErrorCode::InvalidArgument, "depth renderer device is null");
        }
            
        // Load vertex shaders, path is prism/<CMake Target>/<Shader File>.hlsl
        const Gpu::FShaderEntry VertexShader = {"prism/PrismSurface/SurfaceDepth.hlsl", "main_vs", nvrhi::ShaderType::Vertex, {}};
        auto Shader = VertexShader.Load(Shaders);
        if (!Shader)
        {
            return FStatus::Error(EErrorCode::ShaderCompileFailed, Shaders.GetLastError());
        }

        // Create input layout.
        const nvrhi::VertexAttributeDesc Attribute = nvrhi::VertexAttributeDesc()
            .setName("POSITION")
            .setFormat(nvrhi::Format::RGB32_FLOAT)
            .setBufferIndex(0)
            .setOffset(0)
            .setElementStride(sizeof(dm::float3));
        InputLayout = Device->createInputLayout(&Attribute, 1, Shader);
        if (!InputLayout)
        {
            return FStatus::Error(EErrorCode::PipelineCreationFailed, "depth input layout failed");
        }
            
        // Create binding layout for push constants.
        nvrhi::BindingLayoutDesc Layout;
        Layout.visibility = nvrhi::ShaderType::Vertex;
        Layout.bindings = {nvrhi::BindingLayoutItem::PushConstants(0, sizeof(FSurfaceDepthConstants))};
        BindingLayout = Device->createBindingLayout(Layout);
        if (!BindingLayout)
        {
            return FStatus::Error(EErrorCode::PipelineCreationFailed, "binding layout failed");
        }

        // Create passes for each depth convention and cull mode.
        nvrhi::GraphicsPipelineDesc Pipeline;
        Pipeline.inputLayout = InputLayout;
        Pipeline.bindingLayouts = {BindingLayout};
        Pipeline.primType = nvrhi::PrimitiveType::TriangleList;
        Pipeline.renderState.depthStencilState
            .setDepthTestEnable(true)
            .setDepthWriteEnable(true);

        constexpr nvrhi::RasterCullMode CullModes[] = {
            nvrhi::RasterCullMode::None,
            nvrhi::RasterCullMode::Front,
            nvrhi::RasterCullMode::Back
        };

        for (uint32_t DepthIndex = 0; DepthIndex < Passes.size(); ++DepthIndex)
        {
            const auto Convention = DepthIndex == 0
                ? EDepthConvention::ForwardZ0To1 : EDepthConvention::ReversedZ0To1;
            Pipeline.renderState.depthStencilState.setDepthFunc(Gpu::GetDepthCompare(Convention));

            for (uint32_t CullIndex = 0; CullIndex < Passes[DepthIndex].size(); ++CullIndex)
            {
                Pipeline.renderState.rasterState.setCullMode(CullModes[CullIndex]);
                const auto Status = Passes[DepthIndex][CullIndex].Initialize(Device, Shaders, Pipeline, {VertexShader});
                if (!Status)
                {
                    return Status;
                }
            }
        }
        return FStatus::Ok();
    }

    FStatus FDepthRenderer::Record(nvrhi::ICommandList* Commands, const FDepthInputs& Inputs,
                                    const FDepthSettings& Settings, FDepthOutputs& Outputs)
    {
        Outputs = {};
        const auto& Geometry = Inputs.Geometry;
        const auto& Camera = Inputs.Camera;
        nvrhi::IFramebuffer* Target = Inputs.Target;

        if (!Commands || !Target || !BindingLayout ||
            static_cast<uint32_t>(Settings.CullMode) >= static_cast<uint32_t>(EDepthCullMode::Count))
        {
            return FStatus::Error(EErrorCode::InvalidArgument, "invalid depth pass inputs");
        }

        const auto& TargetDesc = Target->getDesc();
        nvrhi::ITexture* Depth = TargetDesc.depthAttachment.texture;
        if (!Depth || !TargetDesc.colorAttachments.empty() ||
            TargetDesc.depthAttachment.subresources.baseMipLevel != 0 ||
            TargetDesc.depthAttachment.subresources.baseArraySlice != 0 ||
            TargetDesc.depthAttachment.subresources.numMipLevels != 1 ||
            TargetDesc.depthAttachment.subresources.numArraySlices != 1 ||
            Depth->getDesc().dimension != nvrhi::TextureDimension::Texture2D ||
            Depth->getDesc().sampleCount != 1 ||
            Depth->getDesc().format != nvrhi::Format::D32)
        {
            return FStatus::Error(EErrorCode::InvalidArgument,
                                  "depth pass needs a D32 single-sample depth-only target at mip 0, slice 0");
        }

        // Per draw check validity.
        for (const auto& Draw : Geometry.Draws)
        {
            if (Draw.BufferGroupIndex >= Geometry.BufferGroups.size() || !Draw.IndexCount)
            {
                return FStatus::Error(EErrorCode::InvalidArgument, "depth draw range is invalid");
            }
            
            const auto& Buffers = Geometry.BufferGroups[Draw.BufferGroupIndex];
            const uint32_t IndexStride = Buffers.IndexFormat == nvrhi::Format::R16_UINT ? 2u : 4u;
            if (!Buffers.Positions || !Buffers.Indices ||
                (Buffers.IndexFormat != nvrhi::Format::R16_UINT && Buffers.IndexFormat != nvrhi::Format::R32_UINT) ||
                Buffers.PositionStride != sizeof(dm::float3) || !Buffers.PositionRange.byteSize ||
                Buffers.PositionRange.byteSize % Buffers.PositionStride != 0 ||
                Draw.BaseVertex >= Buffers.PositionRange.byteSize / Buffers.PositionStride ||
                Buffers.PositionRange.byteOffset > Buffers.Positions->getDesc().byteSize ||
                Buffers.PositionRange.byteSize > Buffers.Positions->getDesc().byteSize - Buffers.PositionRange.byteOffset ||
                uint64_t(Draw.FirstIndex) + Draw.IndexCount > Buffers.Indices->getDesc().byteSize / IndexStride)
                {
                    return FStatus::Error(EErrorCode::InvalidArgument, "depth geometry buffers are invalid");
                }  
        }

        if (Geometry.Draws.empty())
        {
            if (Settings.bClearDepth)
            {
                Commands->clearDepthStencilTexture(Depth, nvrhi::TextureSubresourceSet(0, 1, 0, 1), true,
                                                    GetDepthClearValue(Camera.DepthConvention), false, 0);
            }
            Outputs.Depth = Depth;
            Outputs.Debug.bCleared = Settings.bClearDepth;
            return FStatus::Ok();
        }

        // Choose PSO
        const auto Convention = Camera.DepthConvention;
        const auto& WorldToClip = Camera.Raster.WorldToClip;
        const uint32_t DepthIndex = Convention == EDepthConvention::ReversedZ0To1 ? 1u : 0u;
        const uint32_t CullIndex = static_cast<uint32_t>(Settings.CullMode);
        Gpu::FRasterPass& Pass = Passes[DepthIndex][CullIndex];

        // Create binding set for push constants.
        nvrhi::BindingSetDesc Bindings;
        Bindings.bindings = {nvrhi::BindingSetItem::PushConstants(0, sizeof(FSurfaceDepthConstants))};
        const auto BindingSet = Pass.GetOrCreateBindingSet(Bindings, BindingLayout);
        if (!BindingSet)
        {
            return FStatus::Error(EErrorCode::PipelineCreationFailed, "depth binding set failed");
        }

        // Depth clear
        if (Settings.bClearDepth)
        {
            Commands->clearDepthStencilTexture(Depth, nvrhi::TextureSubresourceSet(0, 1, 0, 1), true,
                                            GetDepthClearValue(Convention), false, 0);
        }

        uint64_t TriangleCount = 0;

        // Draw all geometry
        for (const auto& Draw : Geometry.Draws)
        {
            const auto& Buffers = Geometry.BufferGroups[Draw.BufferGroupIndex];

            nvrhi::GraphicsState State;
            State.framebuffer = Target;
            State.bindings = {BindingSet};
            State.vertexBuffers = {{Buffers.Positions, 0, Buffers.PositionRange.byteOffset}};
            State.indexBuffer = {Buffers.Indices, Buffers.IndexFormat, 0};
            const auto Status = Pass.Bind(Commands, State);
            if (!Status)
            {
                return Status;
            }

            FSurfaceDepthConstants Constants{};
            Constants.ObjectToClip = ToShaderMatrix(dm::affineToHomogeneous(Draw.ObjectToWorld) * WorldToClip);
            Commands->setPushConstants(&Constants, sizeof(Constants));
            Commands->drawIndexed(nvrhi::DrawArguments().setVertexCount(Draw.IndexCount)
                                      .setStartIndexLocation(Draw.FirstIndex).setStartVertexLocation(Draw.BaseVertex));

            TriangleCount += Draw.IndexCount / 3;
        }
        
        Outputs.Depth = Depth;
        Outputs.Debug.DrawCount = static_cast<uint32_t>(Geometry.Draws.size());
        Outputs.Debug.TriangleCount = TriangleCount;
        Outputs.Debug.bCleared = Settings.bClearDepth;

        return FStatus::Ok();
    }
} // namespace Prism::Surface
