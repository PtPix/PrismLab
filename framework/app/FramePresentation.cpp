#include "FramePresentation.h"
#include "Experiment.h"

namespace Prism::Host
{
	FStatus FFramePresentation::Record(FExperimentContext& Context, nvrhi::ICommandList* Commands,
									   nvrhi::IFramebuffer* Swapchain, nvrhi::ITexture* Scene, EColorSpace Space,
									   float Delta, uint64_t Frame)
	{
		const auto& Attachment = Swapchain->getDesc().colorAttachments[0].texture->getDesc();
		if (!Texture || Texture->getDesc().width != Attachment.width ||
			Texture->getDesc().height != Attachment.height || Texture->getDesc().format != Attachment.format)
		{
			Reset();
			nvrhi::TextureDesc Desc;
			Desc.width = Attachment.width;
			Desc.height = Attachment.height;
			Desc.format = Attachment.format;
			Desc.isRenderTarget = true;
			Desc.initialState = nvrhi::ResourceStates::RenderTarget;
			Desc.keepInitialState = true;
			Desc.debugName = "Presentation.Output";
			Texture = Context.Gpu.Device->createTexture(Desc);
			if (!Texture)
				return FStatus::Error(EErrorCode::DeviceError, "presentation allocation failed");
			Framebuffer = Context.Gpu.Device->createFramebuffer(nvrhi::FramebufferDesc().addColorAttachment(Texture));
			Bindings = std::make_unique<donut::engine::BindingCache>(Context.Gpu.Device);
		}
		if (!Framebuffer)
			return FStatus::Error(EErrorCode::DeviceError, "presentation framebuffer failed");
		if (Context.Output.DisplayChain && Space != EColorSpace::DisplayEncoded)
		{
			Gpu::FDisplayInput Input;
			Input.SceneColor = Scene;
			Input.ColorSpace = Space;
			Input.OutputTarget = Framebuffer;
			Input.OutputSize = {Attachment.width, Attachment.height};
			Input.DeltaTimeSeconds = Delta;
			Input.FrameIndex = Frame;
			auto Status = Context.Output.DisplayChain->Record(Context.Gpu, Commands, Input);
			if (!Status)
				return Status;
		}
		else
			Context.Gpu.CommonPasses->BlitTexture(Commands, Framebuffer, Scene, Bindings.get());
		Context.Gpu.CommonPasses->BlitTexture(Commands, Swapchain, Texture, Bindings.get());
		return FStatus::Ok();
	}
} // namespace Prism::Host
