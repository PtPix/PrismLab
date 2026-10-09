#pragma once

#include "DepthInputs.h"
#include "DepthOutputs.h"
#include "DepthSettings.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <vector>

namespace Prism::Surface
{
	enum class EDepthError
	{
		None,
		InvalidArgument,
		DeviceError
	};

	struct FDepthResult
	{
		EDepthError Error = EDepthError::None;
		const char* Message = "";

		explicit operator bool() const
		{
			return Error == EDepthError::None;
		}
	};

	class FDepthRenderer
	{
	  public:
		FDepthResult Initialize(nvrhi::IDevice* Device, nvrhi::IShader* VertexShader);
		FDepthResult Record(nvrhi::ICommandList* Commands, const FDepthInputs& Inputs, const FDepthSettings& Settings,
							FDepthOutputs& Outputs);

	  private:
		struct FPipeline
		{
			nvrhi::FramebufferInfo Format;
			nvrhi::GraphicsPipelineHandle Handle;
		};

		nvrhi::IDevice* Device = nullptr;
		nvrhi::ShaderHandle VertexShader;
		nvrhi::InputLayoutHandle InputLayout;
		nvrhi::BindingLayoutHandle BindingLayout;
		nvrhi::BindingSetHandle BindingSet;
		std::array<std::array<std::vector<FPipeline>, 3>, 2> Pipelines;
	};
} // namespace Prism::Surface
