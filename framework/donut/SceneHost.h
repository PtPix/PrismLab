#pragma once

// Donut facilities: owns the active scene and keeps it up to date.
//
// Two sources are supported:
//   * procedural  —— built in code, no assets (default for algorithm work)
//   * gltf        —— loaded through Donut's scene loader, for realism later on
//
// Everything the labs see goes through SceneData: the Donut graph for the shared scene pipeline, plus
// contract-level light records and a geometry batch for algorithm-side passes.

#include "HostConfig.h"
#include "SceneData.h"

#include <framework/types/Status.h>

#include <donut/engine/Scene.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/TextureCache.h>

#include <memory>

namespace donut::vfs
{
    class IFileSystem;
}

namespace renderlab::adapter
{
    class SceneHost
    {
    public:
        // Creates the scene and records its first uploads. Does not return a command list to the caller:
        // the scene is ready to draw when the call returns.
        Status Load(
            nvrhi::IDevice* device,
            const std::shared_ptr<donut::engine::ShaderFactory>& shaderFactory,
            const std::shared_ptr<donut::vfs::IFileSystem>& fileSystem,
            const HostConfig& config);

        // 每帧调用（在已打开的命令列表中）：刷新动画、变换和缓冲。程序化场景是空实现。
        void Update(nvrhi::ICommandList* commands, uint32_t frameIndex);

        void Reset();

        [[nodiscard]] const SceneData& GetData() const { return m_Scene; }
        [[nodiscard]] bool IsLoaded() const { return m_Scene.IsLoaded(); }
        [[nodiscard]] bool IsAssetScene() const { return m_LoadedScene != nullptr; }

    private:
        void BuildGeometryBatchFromSceneGraph();
        void CollectStats();

        nvrhi::IDevice* m_Device = nullptr;
        std::shared_ptr<donut::vfs::IFileSystem> m_FileSystem;
        std::shared_ptr<donut::engine::TextureCache> m_TextureCache;
        std::unique_ptr<donut::engine::Scene> m_LoadedScene;
        SceneData m_Scene;
    };
}
