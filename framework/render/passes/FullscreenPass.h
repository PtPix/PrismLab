#pragma once
#include "RasterPass.h"
#include <donut/engine/CommonRenderPasses.h>

namespace Prism::Gpu
{
	class FFullscreenPass
	{
	  public:
		FStatus Initialize(nvrhi::IDevice* Device, FShaderLibrary& Shaders, donut::engine::CommonRenderPasses& Common,
						   FShaderEntry PixelShader, const nvrhi::BindingLayoutVector& Layouts = {},
						   nvrhi::RenderState State = {});
		FStatus Record(nvrhi::ICommandList* Commands, nvrhi::IFramebuffer* Target,
					   const nvrhi::BindingSetVector& Bindings = {}, nvrhi::ViewportState Viewport = {});
		nvrhi::BindingSetHandle GetOrCreateBindingSet(const nvrhi::BindingSetDesc& Desc, nvrhi::IBindingLayout* Layout)
		{
			return Raster.GetOrCreateBindingSet(Desc, Layout);
		}
		void ClearBindings()
		{
			Raster.ClearBindings();
		}

	  private:
		FRasterPass Raster;
	};
} // namespace Prism::Gpu
