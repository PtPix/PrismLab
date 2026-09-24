#pragma once
#include "SceneHost.h"
#include "SceneForwardPipeline.h"
#include <framework/render/RenderServices.h>
namespace prism::adapter
{
    // Optional legacy reference path. Samples own and select this adapter explicitly.
    class ForwardScene
    {
    public:
        Status Initialize(gpu::RenderServices& gpu, const ScenePreset& scene, const LightingPreset& lighting);
        void Record(nvrhi::ICommandList* commands, uint64_t submission,
            const donut::engine::IView& view, const donut::engine::IView& previous,
            nvrhi::IFramebuffer* target);
        const SceneData& Data() const { return m_Scene.GetData(); }
    private:
        SceneHost m_Scene;
        pipeline::SceneForwardPipeline m_Pipeline;
        dm::float3 m_AmbientTop{}, m_AmbientBottom{};
    };
}
