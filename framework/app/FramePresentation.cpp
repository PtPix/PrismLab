#include "FramePresentation.h"
#include "Experiment.h"

namespace prism::host
{
    Status FramePresentation::Record(ExperimentContext& context, nvrhi::ICommandList* commands, nvrhi::IFramebuffer* swapchain,
        nvrhi::ITexture* scene, ColorSpace space, float delta, uint64_t frame)
    {
        const auto& attachment = swapchain->getDesc().colorAttachments[0].texture->getDesc();
        if (!m_Texture || m_Texture->getDesc().width != attachment.width || m_Texture->getDesc().height != attachment.height || m_Texture->getDesc().format != attachment.format)
        {
            Reset();
            nvrhi::TextureDesc desc;
            desc.width = attachment.width; desc.height = attachment.height; desc.format = attachment.format;
            desc.isRenderTarget = true; desc.initialState = nvrhi::ResourceStates::RenderTarget;
            desc.keepInitialState = true; desc.debugName = "Presentation.Output";
            m_Texture = context.gpu.device->createTexture(desc);
            if (!m_Texture) return Status::Error(ErrorCode::DeviceError, "presentation allocation failed");
            m_Framebuffer = context.gpu.device->createFramebuffer(nvrhi::FramebufferDesc().addColorAttachment(m_Texture));
            m_Bindings = std::make_unique<donut::engine::BindingCache>(context.gpu.device);
        }
        if (!m_Framebuffer) return Status::Error(ErrorCode::DeviceError, "presentation framebuffer failed");
        if (context.output.displayChain && space != ColorSpace::DisplayEncoded)
        {
            gpu::DisplayInput input; input.sceneColor = scene; input.colorSpace = space;
            input.outputTarget = m_Framebuffer; input.outputSize = {attachment.width, attachment.height};
            input.deltaTimeSeconds = delta; input.frameIndex = frame;
            auto status = context.output.displayChain->Record(context.gpu, commands, input);
            if (!status) return status;
        }
        else context.gpu.commonPasses->BlitTexture(commands, m_Framebuffer, scene, m_Bindings.get());
        context.gpu.commonPasses->BlitTexture(commands, swapchain, m_Texture, m_Bindings.get());
        return Status::Ok();
    }
}
