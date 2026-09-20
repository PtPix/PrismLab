#include "SceneForwardPipeline.h"

#include <donut/core/log.h>
#include <donut/render/GeometryPasses.h>

namespace renderlab::pipeline
{
    bool SceneForwardPipeline::Initialize(
        nvrhi::IDevice* device,
        const std::shared_ptr<donut::engine::ShaderFactory>& shaderFactory)
    {
        if (!device || !shaderFactory)
        {
            donut::log::error("SceneForwardPipeline: device and shader factory are required.");
            return false;
        }

        auto commonPasses = std::make_shared<donut::engine::CommonRenderPasses>(device, shaderFactory);

        m_ForwardPass = std::make_shared<donut::render::ForwardShadingPass>(device, commonPasses);
        m_ForwardPass->Init(*shaderFactory, donut::render::ForwardShadingPass::CreateParameters());

        return true;
    }

    void SceneForwardPipeline::PrepareLights(
        donut::render::ForwardShadingPass::Context& context,
        nvrhi::ICommandList* commands,
        const donut::engine::SceneGraph& graph,
        const dm::float3& ambientTop,
        const dm::float3& ambientBottom)
    {
        if (!m_ForwardPass)
            return;

        m_ForwardPass->PrepareLights(context, commands, graph.GetLights(), ambientTop, ambientBottom, {});
    }

    void SceneForwardPipeline::RenderScene(
        nvrhi::ICommandList* commands,
        const donut::engine::SceneGraph& graph,
        const donut::engine::IView& view,
        const donut::engine::IView& previousView,
        nvrhi::IFramebuffer* framebuffer,
        const dm::float3& ambientTop,
        const dm::float3& ambientBottom)
    {
        if (!m_ForwardPass || !framebuffer)
            return;

        donut::render::ForwardShadingPass::Context context;
        PrepareLights(context, commands, graph, ambientTop, ambientBottom);

        m_DrawStrategy.PrepareForView(graph.GetRootNode(), view);

        donut::render::RenderView(
            commands,
            &view,
            &previousView,
            framebuffer,
            m_DrawStrategy,
            *m_ForwardPass,
            context);
    }
}
