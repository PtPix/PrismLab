#pragma once
#include "PassSupport.h"

namespace prism::gpu
{
    class ComputePass final : public ShaderPass
    {
    public:
        Status Initialize(nvrhi::IDevice* device, ShaderLibrary& shaders, ShaderEntry shader,
            const nvrhi::BindingLayoutVector& layouts = {}, dm::uint3 threads = dm::uint3(8, 8, 1));
        Status Dispatch(nvrhi::ICommandList* commands, const nvrhi::BindingSetVector& bindings, dm::uint3 groups) const;
        Status DispatchExtent(nvrhi::ICommandList* commands, const nvrhi::BindingSetVector& bindings, dm::uint3 extent) const;
        nvrhi::IComputePipeline* GetPipeline() const { return m_Pipeline; }
        Status PrepareShaders(ShaderLibrary& candidate) override;
        void CommitShaders() override;
        void DiscardShaders() override { m_Candidate = nullptr; }
    private:
        ShaderEntry m_Shader;
        nvrhi::BindingLayoutVector m_Layouts;
        dm::uint3 m_Threads = dm::uint3(1);
        nvrhi::ComputePipelineHandle m_Pipeline, m_Candidate;
    };
}
