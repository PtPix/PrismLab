#pragma once
#include "PassSupport.h"

namespace Prism::Gpu
{
	class FComputePass final : public FShaderPass
	{
	  public:
		FStatus Initialize(nvrhi::IDevice* InDevice, FShaderLibrary& InShaderLibrary, FShaderEntry InShaderEntry,
						   const nvrhi::BindingLayoutVector& InBindingLayouts = {},
						   dm::uint3 InThreadGroupSize = dm::uint3(8, 8, 1));
		FStatus Dispatch(nvrhi::ICommandList* Commands, const nvrhi::BindingSetVector& Bindings,
						 dm::uint3 Groups) const;
		FStatus DispatchExtent(nvrhi::ICommandList* Commands, const nvrhi::BindingSetVector& Bindings,
							   dm::uint3 Extent) const;
		nvrhi::IComputePipeline* GetPipeline() const
		{
			return Pipeline;
		}
		FStatus PrepareShaders(FShaderLibrary& CandidateLibrary) override;
		void CommitShaders() override;
		void DiscardShaders() override
		{
			CandidatePipeline = nullptr;
		}

	  private:
		FShaderEntry ShaderEntry;
		nvrhi::BindingLayoutVector BindingLayouts;
		dm::uint3 ThreadGroupSize = dm::uint3(1);
		nvrhi::ComputePipelineHandle Pipeline;
		nvrhi::ComputePipelineHandle CandidatePipeline;
	};
} // namespace Prism::Gpu
