#include "ComputePass.h"

namespace prism::gpu
{
    Status ComputePass::Initialize(nvrhi::IDevice* device, ShaderLibrary& shaders, ShaderEntry shader,
        const nvrhi::BindingLayoutVector& layouts, dm::uint3 threads)
    {
        if (!device || shader.stage != nvrhi::ShaderType::Compute || !threads.x || !threads.y || !threads.z)
            return Status::Error(ErrorCode::InvalidArgument, "invalid compute pass description");
        Attach(device, shaders);
        m_Shader = std::move(shader); m_Layouts = layouts; m_Threads = threads;
        auto status = PrepareShaders(shaders);
        if (status) CommitShaders();
        return status;
    }
    Status ComputePass::PrepareShaders(ShaderLibrary& candidate)
    {
        auto shader = m_Shader.Load(candidate);
        if (!shader) return Status::Error(ErrorCode::ShaderCompileFailed, candidate.GetLastError());
        nvrhi::ComputePipelineDesc desc;
        desc.CS = shader; desc.bindingLayouts = m_Layouts;
        m_Candidate = m_Device->createComputePipeline(desc);
        return m_Candidate ? Status::Ok() : Status::Error(ErrorCode::PipelineCreationFailed, m_Shader.path);
    }
    void ComputePass::CommitShaders() { m_Pipeline = std::move(m_Candidate); ClearBindings(); }
    Status ComputePass::Dispatch(nvrhi::ICommandList* commands, const nvrhi::BindingSetVector& bindings, dm::uint3 groups) const
    {
        if (!commands || !m_Pipeline || bindings.size() != m_Layouts.size())
            return Status::Error(ErrorCode::InvalidArgument, "compute state is incomplete");
        if (!groups.x || !groups.y || !groups.z) return Status::Ok();
        nvrhi::ComputeState state; state.pipeline = m_Pipeline; state.bindings = bindings;
        commands->beginMarker(m_Shader.path.c_str());
        commands->setComputeState(state);
        commands->dispatch(groups.x, groups.y, groups.z);
        commands->endMarker();
        return Status::Ok();
    }
    Status ComputePass::DispatchExtent(nvrhi::ICommandList* commands, const nvrhi::BindingSetVector& bindings, dm::uint3 extent) const
    {
        auto ceil = [](uint32_t n, uint32_t d) { return n / d + uint32_t(n % d != 0); };
        return Dispatch(commands, bindings, dm::uint3(ceil(extent.x, m_Threads.x), ceil(extent.y, m_Threads.y), ceil(extent.z, m_Threads.z)));
    }
}
