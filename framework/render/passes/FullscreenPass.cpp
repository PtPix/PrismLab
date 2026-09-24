#include "FullscreenPass.h"

namespace prism::gpu
{
    Status FullscreenPass::Initialize(nvrhi::IDevice* device, ShaderLibrary& shaders, donut::engine::CommonRenderPasses& common,
        ShaderEntry pixelShader, const nvrhi::BindingLayoutVector& layouts, nvrhi::RenderState state)
    {
        if (pixelShader.stage != nvrhi::ShaderType::Pixel)
            return Status::Error(ErrorCode::InvalidArgument, "fullscreen shader must be a pixel shader");
        nvrhi::GraphicsPipelineDesc desc;
        desc.VS = common.m_FullscreenVS; desc.primType = nvrhi::PrimitiveType::TriangleStrip;
        desc.bindingLayouts = layouts; desc.renderState = state;
        desc.renderState.depthStencilState.setDepthTestEnable(false).setDepthWriteEnable(false);
        return m_Raster.Initialize(device, shaders, desc, {std::move(pixelShader)});
    }
    Status FullscreenPass::Record(nvrhi::ICommandList* commands, nvrhi::IFramebuffer* target,
        const nvrhi::BindingSetVector& bindings, nvrhi::ViewportState viewport)
    {
        nvrhi::GraphicsState state; state.framebuffer = target; state.bindings = bindings; state.viewport = viewport;
        return m_Raster.Draw(commands, state, nvrhi::DrawArguments().setVertexCount(4));
    }
}
