#include "ComputePass.h"

namespace Prism::Gpu
{
	FStatus FComputePass::Initialize(nvrhi::IDevice* InDevice, FShaderLibrary& InShaderLibrary,
									 FShaderEntry InShaderEntry, const nvrhi::BindingLayoutVector& InBindingLayouts,
									 dm::uint3 InThreadGroupSize)
	{
		if (!InDevice || InShaderEntry.Stage != nvrhi::ShaderType::Compute || !InThreadGroupSize.x ||
			!InThreadGroupSize.y || !InThreadGroupSize.z)
			return FStatus::Error(EErrorCode::InvalidArgument, "invalid compute pass description");
		Attach(InDevice, InShaderLibrary);
		ShaderEntry = std::move(InShaderEntry);
		BindingLayouts = InBindingLayouts;
		ThreadGroupSize = InThreadGroupSize;
		FStatus Status = PrepareShaders(InShaderLibrary);
		if (Status)
			CommitShaders();
		return Status;
	}
	FStatus FComputePass::PrepareShaders(FShaderLibrary& CandidateLibrary)
	{
		nvrhi::ShaderHandle ShaderHandle = ShaderEntry.Load(CandidateLibrary);
		if (!ShaderHandle)
			return FStatus::Error(EErrorCode::ShaderCompileFailed, CandidateLibrary.GetLastError());
		nvrhi::ComputePipelineDesc Desc;
		Desc.CS = ShaderHandle;
		Desc.bindingLayouts = BindingLayouts;
		CandidatePipeline = Device->createComputePipeline(Desc);
		return CandidatePipeline ? FStatus::Ok() : FStatus::Error(EErrorCode::PipelineCreationFailed, ShaderEntry.Path);
	}
	void FComputePass::CommitShaders()
	{
		Pipeline = std::move(CandidatePipeline);
		ClearBindings();
	}
	FStatus FComputePass::Dispatch(nvrhi::ICommandList* Commands, const nvrhi::BindingSetVector& Bindings,
								   dm::uint3 Groups) const
	{
		if (!Commands || !Pipeline || Bindings.size() != BindingLayouts.size())
			return FStatus::Error(EErrorCode::InvalidArgument, "compute state is incomplete");
		if (!Groups.x || !Groups.y || !Groups.z)
			return FStatus::Ok();
		nvrhi::ComputeState State;
		State.pipeline = Pipeline;
		State.bindings = Bindings;
		Commands->beginMarker(ShaderEntry.Path.c_str());
		Commands->setComputeState(State);
		Commands->dispatch(Groups.x, Groups.y, Groups.z);
		Commands->endMarker();
		return FStatus::Ok();
	}
	FStatus FComputePass::DispatchExtent(nvrhi::ICommandList* Commands, const nvrhi::BindingSetVector& Bindings,
										 dm::uint3 Extent) const
	{
		auto Ceil = [](uint32_t N, uint32_t D)
		{
			return N / D + uint32_t(N % D != 0);
		};
		return Dispatch(Commands, Bindings,
						dm::uint3(Ceil(Extent.x, ThreadGroupSize.x), Ceil(Extent.y, ThreadGroupSize.y),
								  Ceil(Extent.z, ThreadGroupSize.z)));
	}
} // namespace Prism::Gpu
