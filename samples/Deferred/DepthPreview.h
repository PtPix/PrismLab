#pragma once

#include <framework/render/RenderServices.h>
#include <framework/render/data/CameraData.h>
#include <framework/render/passes/FullscreenPass.h>

namespace Prism::Samples
{
	// Sample-local visualization; reusable depth drawing lives in algorithms/Surface.
	class FDepthPreview
	{
	  public:
		FStatus Initialize(Gpu::FRenderServices& Gpu);
		nvrhi::ITexture* Record(Gpu::FRenderServices& Gpu, nvrhi::ICommandList* Commands,
								 nvrhi::ITexture* Depth, const FCameraData& Camera, FExtent2D Size, int Mode);
		void OnResize()
		{
			Pass.ClearBindings();
		}

	  private:
		Gpu::FFullscreenPass Pass;
		Gpu::FPassConstants Constants;
		Gpu::FTextureRequest OutputRequest;
		nvrhi::BindingLayoutHandle Layout;
	};
} // namespace Prism::Samples
