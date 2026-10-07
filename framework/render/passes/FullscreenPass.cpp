#include "FullscreenPass.h"

namespace Prism::Gpu
{
	FStatus FFullscreenPass::Initialize(nvrhi::IDevice* Device, FShaderLibrary& Shaders,
										donut::engine::CommonRenderPasses& Common, FShaderEntry PixelShader,
										const nvrhi::BindingLayoutVector& Layouts, nvrhi::RenderState State)
	{
		if (PixelShader.Stage != nvrhi::ShaderType::Pixel)
			return FStatus::Error(EErrorCode::InvalidArgument, "fullscreen shader must be a pixel shader");
		nvrhi::GraphicsPipelineDesc Desc;
		Desc.VS = Common.m_FullscreenVS;
		Desc.primType = nvrhi::PrimitiveType::TriangleStrip;
		Desc.bindingLayouts = Layouts;
		Desc.renderState = State;
		Desc.renderState.depthStencilState.setDepthTestEnable(false).setDepthWriteEnable(false);
		return Raster.Initialize(Device, Shaders, Desc, {std::move(PixelShader)});
	}
	FStatus FFullscreenPass::Record(nvrhi::ICommandList* Commands, nvrhi::IFramebuffer* Target,
									const nvrhi::BindingSetVector& Bindings, nvrhi::ViewportState Viewport)
	{
		nvrhi::GraphicsState State;
		State.framebuffer = Target;
		State.bindings = Bindings;
		State.viewport = Viewport;
		return Raster.Draw(Commands, State, nvrhi::DrawArguments().setVertexCount(4));
	}
} // namespace Prism::Gpu
