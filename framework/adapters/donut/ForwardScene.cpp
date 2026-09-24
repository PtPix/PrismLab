#include "ForwardScene.h"
#include <donut/core/vfs/VFS.h>
namespace prism::adapter
{
    Status ForwardScene::Initialize(gpu::RenderServices& gpu, const ScenePreset& scene, const LightingPreset& lighting)
    {
        auto factory = gpu.shaders->GetFactory();
        if (!m_Pipeline.Initialize(gpu.device, factory))
            return Status::Error(ErrorCode::PipelineCreationFailed, "legacy forward pipeline failed");
        m_AmbientTop = dm::float3(lighting.ambientIntensity);
        m_AmbientBottom = m_AmbientTop * 0.6f;
        return m_Scene.Load(gpu.device, factory, std::make_shared<donut::vfs::NativeFileSystem>(), scene, lighting);
    }
    void ForwardScene::Record(nvrhi::ICommandList* commands, uint64_t submission,
        const donut::engine::IView& view, const donut::engine::IView& previous, nvrhi::IFramebuffer* target)
    {
        m_Scene.Update(commands, uint32_t(submission));
        if (m_Scene.GetData().graph)
            m_Pipeline.RenderScene(commands, *m_Scene.GetData().graph, view, previous, target, m_AmbientTop, m_AmbientBottom);
    }
}
