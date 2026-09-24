#pragma once

// Pipelines layer: the shared "scene -> texture" draw path.
//
// An experiment that needs a rendered scene (shadow receivers, AO, SSR, tonemapping experiments) does not
// wire up geometry passes: it declares a render target and calls RenderScene. The pass split inside
// this pipeline is Donut's forward shading, which is the baseline the roadmap compares against.

#include <framework/core/Types.h>

#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <donut/render/DrawStrategy.h>
#include <donut/render/ForwardShadingPass.h>

#include <memory>

namespace prism::pipeline
{
    class SceneForwardPipeline
    {
    public:
        bool Initialize(
            nvrhi::IDevice* device,
            const std::shared_ptr<donut::engine::ShaderFactory>& shaderFactory);

        // 清理目标并按共享前向路径绘制不透明场景。
        // ambientTop/Bottom 来自宿主配置的 lighting 段，已经乘上环境强度。
        void RenderScene(
            nvrhi::ICommandList* commands,
            const donut::engine::SceneGraph& graph,
            const donut::engine::IView& view,
            const donut::engine::IView& previousView,
            nvrhi::IFramebuffer* framebuffer,
            const dm::float3& ambientTop,
            const dm::float3& ambientBottom);

        // 需要自己插 Pass 的实验：先准备光源，再自行调用 RenderView。
        void PrepareLights(
            donut::render::ForwardShadingPass::Context& context,
            nvrhi::ICommandList* commands,
            const donut::engine::SceneGraph& graph,
            const dm::float3& ambientTop,
            const dm::float3& ambientBottom);

        [[nodiscard]] donut::render::ForwardShadingPass& GetForwardPass() { return *m_ForwardPass; }
        [[nodiscard]] donut::render::InstancedOpaqueDrawStrategy& GetDrawStrategy() { return m_DrawStrategy; }

    private:
        std::shared_ptr<donut::render::ForwardShadingPass> m_ForwardPass;
        donut::render::InstancedOpaqueDrawStrategy m_DrawStrategy;
    };
}
