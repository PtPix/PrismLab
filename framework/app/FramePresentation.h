#pragma once
#include "framework/render/presentation/DisplayChain.h"
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/BindingCache.h>

namespace Prism::Host
{
	struct FExperimentContext;
	// Capturable final pixels. The display transform remains application supplied.
	class FFramePresentation
	{
	  public:
		FStatus Record(FExperimentContext& Context, nvrhi::ICommandList* Commands, nvrhi::IFramebuffer* Swapchain,
					   nvrhi::ITexture* Scene, EColorSpace Space, float Delta, uint64_t Frame);
		nvrhi::ITexture* Output() const
		{
			return Texture;
		}
		void Reset()
		{
			Framebuffer = nullptr;
			Texture = nullptr;
			if (Bindings)
				Bindings->Clear();
		}

	  private:
		nvrhi::TextureHandle Texture;
		nvrhi::FramebufferHandle Framebuffer;
		std::unique_ptr<donut::engine::BindingCache> Bindings;
	};
} // namespace Prism::Host
