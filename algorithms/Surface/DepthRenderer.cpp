#include "DepthRenderer.h"
#include "shaders/SurfaceDepthCb.h"

#include <algorithm>

namespace Prism::Surface
{
	static_assert(sizeof(FSurfaceDepthConstants) == 16 * sizeof(float), "depth constants layout changed");

	FDepthResult FDepthRenderer::Initialize(nvrhi::IDevice* InDevice, nvrhi::IShader* InVertexShader)
	{
		if (!InDevice || !InVertexShader)
		{
			return {EDepthError::InvalidArgument, "depth device or shader is null"};
		}

		Device = InDevice;
		VertexShader = InVertexShader;
		for (auto& Convention : Pipelines)
		{
			for (auto& CullMode : Convention)
			{
				CullMode.clear();
			}
		}

		const nvrhi::VertexAttributeDesc Attribute = nvrhi::VertexAttributeDesc()
														 .setName("POSITION")
														 .setFormat(nvrhi::Format::RGB32_FLOAT)
														 .setBufferIndex(0)
														 .setOffset(0)
														 .setElementStride(3 * sizeof(float));
		InputLayout = Device->createInputLayout(&Attribute, 1, VertexShader);
		if (!InputLayout)
		{
			return {EDepthError::DeviceError, "depth input layout creation failed"};
		}

		nvrhi::BindingLayoutDesc Layout;
		Layout.visibility = nvrhi::ShaderType::Vertex;
		Layout.bindings = {nvrhi::BindingLayoutItem::PushConstants(0, sizeof(FSurfaceDepthConstants))};
		BindingLayout = Device->createBindingLayout(Layout);
		if (!BindingLayout)
		{
			return {EDepthError::DeviceError, "depth binding layout creation failed"};
		}

		nvrhi::BindingSetDesc Bindings;
		Bindings.bindings = {nvrhi::BindingSetItem::PushConstants(0, sizeof(FSurfaceDepthConstants))};
		BindingSet = Device->createBindingSet(Bindings, BindingLayout);
		if (!BindingSet)
		{
			return {EDepthError::DeviceError, "depth binding set creation failed"};
		}
		return {};
	}

	FDepthResult FDepthRenderer::Record(nvrhi::ICommandList* Commands, const FDepthInputs& Inputs,
										const FDepthSettings& Settings, FDepthOutputs& Outputs)
	{
		Outputs = {};
		if (!Commands || !Inputs.Target || !Device || !BindingSet ||
			static_cast<uint32_t>(Settings.CullMode) >= static_cast<uint32_t>(EDepthCullMode::Count))
		{
			return {EDepthError::InvalidArgument, "invalid depth pass inputs"};
		}

		const auto& TargetDesc = Inputs.Target->getDesc();
		nvrhi::ITexture* Depth = TargetDesc.depthAttachment.texture;
		if (!Depth || !TargetDesc.colorAttachments.empty() ||
			TargetDesc.depthAttachment.subresources.baseMipLevel != 0 ||
			TargetDesc.depthAttachment.subresources.baseArraySlice != 0 ||
			TargetDesc.depthAttachment.subresources.numMipLevels != 1 ||
			TargetDesc.depthAttachment.subresources.numArraySlices != 1 ||
			Depth->getDesc().dimension != nvrhi::TextureDimension::Texture2D || Depth->getDesc().sampleCount != 1 ||
			Depth->getDesc().format != nvrhi::Format::D32)
		{
			return {EDepthError::InvalidArgument, "depth needs a D32 single-sample depth-only target"};
		}

		const auto& Geometry = Inputs.Geometry;
		for (const auto& Draw : Geometry.Draws)
		{
			if (Draw.BufferGroupIndex >= Geometry.BufferGroups.size() || !Draw.IndexCount)
			{
				return {EDepthError::InvalidArgument, "depth draw range is invalid"};
			}

			const auto& Buffers = Geometry.BufferGroups[Draw.BufferGroupIndex];
			const uint32_t IndexStride = Buffers.IndexFormat == nvrhi::Format::R16_UINT ? 2u : 4u;
			if (!Buffers.Positions || !Buffers.Indices ||
				(Buffers.IndexFormat != nvrhi::Format::R16_UINT && Buffers.IndexFormat != nvrhi::Format::R32_UINT) ||
				!Buffers.PositionRange.byteSize || Buffers.PositionRange.byteSize % (3 * sizeof(float)) != 0 ||
				Draw.BaseVertex >= Buffers.PositionRange.byteSize / (3 * sizeof(float)) ||
				Buffers.PositionRange.byteOffset > Buffers.Positions->getDesc().byteSize ||
				Buffers.PositionRange.byteSize >
					Buffers.Positions->getDesc().byteSize - Buffers.PositionRange.byteOffset ||
				uint64_t(Draw.FirstIndex) + Draw.IndexCount > Buffers.Indices->getDesc().byteSize / IndexStride)
			{
				return {EDepthError::InvalidArgument, "depth geometry buffers are invalid"};
			}
		}

		if (Settings.bClearDepth)
		{
			Commands->clearDepthStencilTexture(Depth, nvrhi::TextureSubresourceSet(0, 1, 0, 1), true,
											   Settings.bReverseZ ? 0.f : 1.f, false, 0);
		}
		Outputs.Depth = Depth;
		Outputs.Debug.bCleared = Settings.bClearDepth;
		if (Geometry.Draws.empty())
		{
			return {};
		}

		const auto Format = Inputs.Target->getFramebufferInfo();
		const uint32_t DepthIndex = Settings.bReverseZ ? 1u : 0u;
		const uint32_t CullIndex = static_cast<uint32_t>(Settings.CullMode);
		auto& Cache = Pipelines[DepthIndex][CullIndex];
		nvrhi::IGraphicsPipeline* ActivePipeline = nullptr;
		for (const auto& Cached : Cache)
		{
			if (Cached.Format == Format)
			{
				ActivePipeline = Cached.Handle;
				break;
			}
		}
		if (!ActivePipeline)
		{
			constexpr nvrhi::RasterCullMode CullModes[] = {nvrhi::RasterCullMode::None, nvrhi::RasterCullMode::Front,
														   nvrhi::RasterCullMode::Back};
			nvrhi::GraphicsPipelineDesc Pipeline;
			Pipeline.VS = VertexShader;
			Pipeline.inputLayout = InputLayout;
			Pipeline.bindingLayouts = {BindingLayout};
			Pipeline.primType = nvrhi::PrimitiveType::TriangleList;
			Pipeline.renderState.depthStencilState.setDepthTestEnable(true).setDepthWriteEnable(true).setDepthFunc(
				Settings.bReverseZ ? nvrhi::ComparisonFunc::Greater : nvrhi::ComparisonFunc::Less);
			Pipeline.renderState.rasterState.setCullMode(CullModes[CullIndex]);
			auto Handle = Device->createGraphicsPipeline(Pipeline, Format);
			if (!Handle)
			{
				return {EDepthError::DeviceError, "depth pipeline creation failed"};
			}
			Cache.push_back({Format, Handle});
			ActivePipeline = Cache.back().Handle;
		}

		uint64_t TriangleCount = 0;
		for (const auto& Draw : Geometry.Draws)
		{
			const auto& Buffers = Geometry.BufferGroups[Draw.BufferGroupIndex];
			nvrhi::GraphicsState State;
			State.framebuffer = Inputs.Target;
			State.pipeline = ActivePipeline;
			State.bindings = {BindingSet};
			State.viewport.addViewportAndScissorRect(Format.getViewport());
			State.vertexBuffers = {{Buffers.Positions, 0, Buffers.PositionRange.byteOffset}};
			State.indexBuffer = {Buffers.Indices, Buffers.IndexFormat, 0};
			Commands->setGraphicsState(State);

			FSurfaceDepthConstants Constants{};
			std::copy(Draw.ObjectToClip.begin(), Draw.ObjectToClip.end(), Constants.ObjectToClip);
			Commands->setPushConstants(&Constants, sizeof(Constants));
			Commands->drawIndexed(nvrhi::DrawArguments()
									  .setVertexCount(Draw.IndexCount)
									  .setStartIndexLocation(Draw.FirstIndex)
									  .setStartVertexLocation(Draw.BaseVertex));
			TriangleCount += Draw.IndexCount / 3;
		}

		Outputs.Debug.DrawCount = static_cast<uint32_t>(Geometry.Draws.size());
		Outputs.Debug.TriangleCount = TriangleCount;
		return {};
	}
} // namespace Prism::Surface
