#pragma once
#include "PassSupport.h"

namespace prism::gpu
{
    class RasterPass final : public ShaderPass
    {
    public:
        Status Initialize(nvrhi::IDevice* device, ShaderLibrary& shaders,
            const nvrhi::GraphicsPipelineDesc& description, std::vector<ShaderEntry> stages);
        Status Bind(nvrhi::ICommandList* commands, nvrhi::GraphicsState state);
        Status Draw(nvrhi::ICommandList* commands, nvrhi::GraphicsState state,
            const nvrhi::DrawArguments& args, bool indexed = false);
        Status PrepareShaders(ShaderLibrary& candidate) override;
        void CommitShaders() override;
        void DiscardShaders() override;
    private:
        struct Pipeline { nvrhi::FramebufferInfo format; nvrhi::GraphicsPipelineHandle handle; };
        nvrhi::GraphicsPipelineDesc m_Description, m_CandidateDescription;
        std::vector<ShaderEntry> m_Stages;
        std::vector<Pipeline> m_Pipelines, m_CandidatePipelines;
    };
}
