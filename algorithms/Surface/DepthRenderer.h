#pragma once

#include <framework/render/passes/RasterPass.h>
#include <framework/render/RenderServices.h>
#include <framework/render/data/Conventions.h>

#include "DepthInputs.h"
#include "DepthOutputs.h"
#include "DepthSettings.h"

#include <array>

namespace Prism::Surface
{
	class FDepthRenderer
	{
	  public:
		FStatus Initialize(nvrhi::IDevice* Device, Gpu::FShaderLibrary& Shaders);

		FStatus Record(nvrhi::ICommandList* Commands, const FDepthInputs& Inputs, const FDepthSettings& Settings,
					   FDepthOutputs& Outputs);

	  private:
		std::array<std::array<Gpu::FRasterPass, 3>, 2> Passes;
		nvrhi::BindingLayoutHandle BindingLayout;
		nvrhi::InputLayoutHandle InputLayout;
	};
} // namespace Prism::Surface
