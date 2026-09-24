#include "RasterPass.h"

namespace prism::gpu
{
    Status RasterPass::Initialize(nvrhi::IDevice* device, ShaderLibrary& shaders,
        const nvrhi::GraphicsPipelineDesc& description, std::vector<ShaderEntry> stages)
    {
        if (!device) return Status::Error(ErrorCode::InvalidArgument, "raster device is null");
        Attach(device, shaders); m_Description = description; m_Stages = std::move(stages);
        m_Pipelines.clear();
        auto status = PrepareShaders(shaders);
        if (status) CommitShaders();
        return status;
    }
    Status RasterPass::PrepareShaders(ShaderLibrary& candidate)
    {
        DiscardShaders();
        m_CandidateDescription = m_Description;
        for (const auto& stage : m_Stages)
        {
            auto shader = stage.Load(candidate);
            if (!shader) return Status::Error(ErrorCode::ShaderCompileFailed, candidate.GetLastError());
            switch (stage.stage)
            {
            case nvrhi::ShaderType::Vertex: m_CandidateDescription.VS = shader; break;
            case nvrhi::ShaderType::Pixel: m_CandidateDescription.PS = shader; break;
            case nvrhi::ShaderType::Geometry: m_CandidateDescription.GS = shader; break;
            case nvrhi::ShaderType::Hull: m_CandidateDescription.HS = shader; break;
            case nvrhi::ShaderType::Domain: m_CandidateDescription.DS = shader; break;
            default: return Status::Error(ErrorCode::Unsupported, "unsupported raster shader stage");
            }
        }
        if (!m_CandidateDescription.VS)
            return Status::Error(ErrorCode::ResourceMissing, "raster vertex shader is missing");
        for (const auto& old : m_Pipelines)
        {
            auto pipeline = m_Device->createGraphicsPipeline(m_CandidateDescription, old.format);
            if (!pipeline) return Status::Error(ErrorCode::PipelineCreationFailed, "raster reload failed");
            m_CandidatePipelines.push_back({old.format, pipeline});
        }
        return Status::Ok();
    }
    void RasterPass::CommitShaders()
    {
        m_Description = std::move(m_CandidateDescription);
        m_Pipelines = std::move(m_CandidatePipelines); ClearBindings();
    }
    void RasterPass::DiscardShaders() { m_CandidatePipelines.clear(); m_CandidateDescription = {}; }
    Status RasterPass::Bind(nvrhi::ICommandList* commands, nvrhi::GraphicsState state)
    {
        if (!commands || !state.framebuffer || !m_Description.VS || state.bindings.size() != m_Description.bindingLayouts.size())
            return Status::Error(ErrorCode::InvalidArgument, "raster state is incomplete");
        const auto format = state.framebuffer->getFramebufferInfo();
        state.pipeline = nullptr;
        for (const auto& pipeline : m_Pipelines)
            if (pipeline.format == format) { state.pipeline = pipeline.handle; break; }
        if (!state.pipeline)
        {
            auto pipeline = m_Device->createGraphicsPipeline(m_Description, format);
            if (!pipeline) return Status::Error(ErrorCode::PipelineCreationFailed, "raster pipeline creation failed");
            m_Pipelines.push_back({format, pipeline}); state.pipeline = pipeline;
        }
        if (state.viewport.viewports.empty()) state.viewport.addViewportAndScissorRect(format.getViewport());
        commands->setGraphicsState(state);
        return Status::Ok();
    }
    Status RasterPass::Draw(nvrhi::ICommandList* commands, nvrhi::GraphicsState state, const nvrhi::DrawArguments& args, bool indexed)
    {
        auto status = Bind(commands, state);
        if (!status) return status;
        if (indexed) commands->drawIndexed(args); else commands->draw(args);
        return Status::Ok();
    }
}
