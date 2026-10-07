#include "RasterPass.h"

namespace Prism::Gpu
{
	FStatus FRasterPass::Initialize(nvrhi::IDevice* InDevice, FShaderLibrary& InShaderLibrary,
									const nvrhi::GraphicsPipelineDesc& InPipelineDescription,
									std::vector<FShaderEntry> InShaderStages)
	{
		if (!InDevice)
			return FStatus::Error(EErrorCode::InvalidArgument, "raster device is null");
		Attach(InDevice, InShaderLibrary);
		PipelineDescription = InPipelineDescription;
		ShaderStages = std::move(InShaderStages);
		Pipelines.clear();
		FStatus Status = PrepareShaders(InShaderLibrary);
		if (Status)
			CommitShaders();
		return Status;
	}
	FStatus FRasterPass::PrepareShaders(FShaderLibrary& CandidateLibrary)
	{
		DiscardShaders();
		CandidatePipelineDescription = PipelineDescription;
		for (const FShaderEntry& Stage : ShaderStages)
		{
			nvrhi::ShaderHandle ShaderHandle = Stage.Load(CandidateLibrary);
			if (!ShaderHandle)
				return FStatus::Error(EErrorCode::ShaderCompileFailed, CandidateLibrary.GetLastError());
			switch (Stage.Stage)
			{
				case nvrhi::ShaderType::Vertex:
					CandidatePipelineDescription.VS = ShaderHandle;
					break;
				case nvrhi::ShaderType::Pixel:
					CandidatePipelineDescription.PS = ShaderHandle;
					break;
				case nvrhi::ShaderType::Geometry:
					CandidatePipelineDescription.GS = ShaderHandle;
					break;
				case nvrhi::ShaderType::Hull:
					CandidatePipelineDescription.HS = ShaderHandle;
					break;
				case nvrhi::ShaderType::Domain:
					CandidatePipelineDescription.DS = ShaderHandle;
					break;
				default:
					return FStatus::Error(EErrorCode::Unsupported, "unsupported raster shader stage");
			}
		}
		if (!CandidatePipelineDescription.VS)
			return FStatus::Error(EErrorCode::ResourceMissing, "raster vertex shader is missing");
		for (const auto& Old : Pipelines)
		{
			nvrhi::GraphicsPipelineHandle PipelineHandle =
				Device->createGraphicsPipeline(CandidatePipelineDescription, Old.Format);
			if (!PipelineHandle)
				return FStatus::Error(EErrorCode::PipelineCreationFailed, "raster reload failed");
			CandidatePipelines.push_back({Old.Format, PipelineHandle});
		}
		return FStatus::Ok();
	}
	void FRasterPass::CommitShaders()
	{
		PipelineDescription = std::move(CandidatePipelineDescription);
		Pipelines = std::move(CandidatePipelines);
		ClearBindings();
	}
	void FRasterPass::DiscardShaders()
	{
		CandidatePipelines.clear();
		CandidatePipelineDescription = {};
	}
	FStatus FRasterPass::Bind(nvrhi::ICommandList* Commands, nvrhi::GraphicsState State)
	{
		if (!Commands || !State.framebuffer || !PipelineDescription.VS ||
			State.bindings.size() != PipelineDescription.bindingLayouts.size())
			return FStatus::Error(EErrorCode::InvalidArgument, "raster state is incomplete");
		const auto Format = State.framebuffer->getFramebufferInfo();
		State.pipeline = nullptr;
		for (const auto& Pipeline : Pipelines)
			if (Pipeline.Format == Format)
			{
				State.pipeline = Pipeline.Handle;
				break;
			}
		if (!State.pipeline)
		{
			nvrhi::GraphicsPipelineHandle PipelineHandle = Device->createGraphicsPipeline(PipelineDescription, Format);
			if (!PipelineHandle)
				return FStatus::Error(EErrorCode::PipelineCreationFailed, "raster pipeline creation failed");
			Pipelines.push_back({Format, PipelineHandle});
			State.pipeline = PipelineHandle;
		}
		if (State.viewport.viewports.empty())
			State.viewport.addViewportAndScissorRect(Format.getViewport());
		Commands->setGraphicsState(State);
		return FStatus::Ok();
	}
	FStatus FRasterPass::Draw(nvrhi::ICommandList* Commands, nvrhi::GraphicsState State,
							  const nvrhi::DrawArguments& Args, bool bIndexed)
	{
		auto Status = Bind(Commands, State);
		if (!Status)
			return Status;
		if (bIndexed)
			Commands->drawIndexed(Args);
		else
			Commands->draw(Args);
		return FStatus::Ok();
	}
} // namespace Prism::Gpu
