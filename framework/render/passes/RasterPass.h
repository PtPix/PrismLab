#pragma once
#include "PassSupport.h"

namespace Prism::Gpu
{
	class FRasterPass final : public FShaderPass
	{
	  public:
		FStatus Initialize(nvrhi::IDevice* InDevice, FShaderLibrary& InShaderLibrary,
						   const nvrhi::GraphicsPipelineDesc& InPipelineDescription,
						   std::vector<FShaderEntry> InShaderStages);
		FStatus Bind(nvrhi::ICommandList* Commands, nvrhi::GraphicsState State);
		FStatus Draw(nvrhi::ICommandList* Commands, nvrhi::GraphicsState State, const nvrhi::DrawArguments& Args,
					 bool bIndexed = false);
		FStatus PrepareShaders(FShaderLibrary& CandidateLibrary) override;
		void CommitShaders() override;
		void DiscardShaders() override;

	  private:
		struct FPipeline
		{
			nvrhi::FramebufferInfo Format;
			nvrhi::GraphicsPipelineHandle Handle;
		};
		nvrhi::GraphicsPipelineDesc PipelineDescription;
		nvrhi::GraphicsPipelineDesc CandidatePipelineDescription;
		std::vector<FShaderEntry> ShaderStages;
		std::vector<FPipeline> Pipelines;
		std::vector<FPipeline> CandidatePipelines;
	};
} // namespace Prism::Gpu
