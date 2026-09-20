#include "SceneHost.h"

#include "LightAdapter.h"
#include "ProceduralScene.h"

#include <donut/core/log.h>

#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace renderlab::adapter
{
    namespace
    {
        bool EndsWith(const std::string& value, const char* suffix)
        {
            const size_t suffixLength = std::strlen(suffix);
            return value.size() >= suffixLength && value.compare(value.size() - suffixLength, suffixLength, suffix) == 0;
        }
    }

    Status SceneHost::Load(
        nvrhi::IDevice* device,
        const std::shared_ptr<donut::engine::ShaderFactory>& shaderFactory,
        const std::shared_ptr<donut::vfs::IFileSystem>& fileSystem,
        const HostConfig& config)
    {
        Reset();

        m_Device = device;
        m_FileSystem = fileSystem;

        const std::string& source = config.scene.source;
        const bool wantsAssetScene =
            source == "gltf" || source == "glb" || EndsWith(source, ".gltf") || EndsWith(source, ".glb") || !config.scene.asset.empty();

        if (wantsAssetScene)
        {
            if (config.scene.asset.empty())
                return Status::Error(ErrorCode::InvalidArgument, "scene.asset must name a scene .json, .gltf or .glb file");

            if (!shaderFactory)
                return Status::Error(ErrorCode::NotInitialized, "a shader factory is required to load an asset scene");

            m_TextureCache = std::make_shared<donut::engine::TextureCache>(device, fileSystem, nullptr);
            m_LoadedScene = std::make_unique<donut::engine::Scene>(
                device, *shaderFactory, fileSystem, m_TextureCache, nullptr, nullptr);

            donut::log::info("RenderLab: loading scene asset '%s'...", config.scene.asset.c_str());

            if (!m_LoadedScene->Load(config.scene.asset))
            {
                m_LoadedScene.reset();
                m_TextureCache.reset();
                return Status::Error(ErrorCode::ResourceMissing, "failed to load scene asset: " + config.scene.asset);
            }

            m_LoadedScene->FinishedLoading(0);

            nvrhi::CommandListHandle commands = device->createCommandList();
            commands->open();
            m_LoadedScene->Refresh(commands, 0);
            commands->close();
            device->executeCommandList(commands);
            device->waitForIdle();

            m_Scene.graph = m_LoadedScene->GetSceneGraph();
            m_Scene.sharedBuffers = nullptr;
            m_Scene.description = "asset scene: " + config.scene.asset;
            m_Scene.lights = CollectLights(*m_Scene.graph);

            BuildGeometryBatchFromSceneGraph();
            CollectStats();

            donut::log::info("RenderLab: asset scene ready -- %u meshes / %u instances / %u lights / %u triangles.",
                m_Scene.stats.meshes, m_Scene.stats.instances, m_Scene.stats.lights, m_Scene.stats.triangles);

            return Status::Ok();
        }

        nvrhi::CommandListHandle commands = device->createCommandList();
        commands->open();
        m_Scene = CreateProceduralScene(device, commands, config.lighting);
        commands->close();
        device->executeCommandList(commands);

        return Status::Ok();
    }

    void SceneHost::Reset()
    {
        m_Scene = SceneData{};
        m_LoadedScene.reset();
        m_TextureCache.reset();
    }

    void SceneHost::Update(nvrhi::ICommandList* commands, uint32_t frameIndex)
    {
        if (!m_LoadedScene)
            return;

        m_LoadedScene->Refresh(commands, frameIndex);
    }

    void SceneHost::BuildGeometryBatchFromSceneGraph()
    {
        m_Scene.geometry = gpu::GeometryBatch{};
        m_Scene.materials.clear();

        if (!m_Scene.graph)
            return;

        // 同一批几何体共享一组缓冲；用指针标识分组，避免假设整个场景只有一组。
        std::unordered_map<const donut::engine::BufferGroup*, uint32_t> bufferGroupIndices;
        std::unordered_map<int, uint32_t> materialIndices;

        dm::box3 worldBounds = dm::box3::empty();
        uint32_t instanceIndex = 0;

        for (const std::shared_ptr<donut::engine::MeshInstance>& instance : m_Scene.graph->GetMeshInstances())
        {
            if (!instance)
                continue;

            const std::shared_ptr<donut::engine::MeshInfo>& mesh = instance->GetMesh();
            if (!mesh || !mesh->buffers)
                continue;

            // 首版只支持不透明、无形变的三角形；蒙皮与曲线几何在此明确跳过。
            if (mesh->type != donut::engine::MeshType::Triangles)
                continue;

            const donut::engine::BufferGroup* group = mesh->buffers.get();
            auto groupIt = bufferGroupIndices.find(group);
            if (groupIt == bufferGroupIndices.end())
            {
                gpu::GeometryBuffers buffers;
                buffers.vertexBuffer = mesh->buffers->vertexBuffer;
                buffers.indexBuffer = mesh->buffers->indexBuffer;
                buffers.positionRange = mesh->buffers->getVertexBufferRange(donut::engine::VertexAttribute::Position);
                buffers.texCoordRange = mesh->buffers->getVertexBufferRange(donut::engine::VertexAttribute::TexCoord1);
                buffers.normalRange = mesh->buffers->getVertexBufferRange(donut::engine::VertexAttribute::Normal);
                buffers.tangentRange = mesh->buffers->getVertexBufferRange(donut::engine::VertexAttribute::Tangent);

                if (!buffers.IsValid())
                    continue;

                const uint32_t newIndex = uint32_t(m_Scene.geometry.bufferGroups.size());
                m_Scene.geometry.bufferGroups.push_back(buffers);
                groupIt = bufferGroupIndices.emplace(group, newIndex).first;
            }

            const uint32_t bufferGroupIndex = groupIt->second;
            const dm::affine3 objectToWorld = instance->GetNode() ? instance->GetNode()->GetLocalToWorldTransformFloat() : dm::affine3::identity();
            const dm::affine3 prevObjectToWorld = instance->GetNode() ? instance->GetNode()->GetPrevLocalToWorldTransformFloat() : objectToWorld;

            for (const std::shared_ptr<donut::engine::MeshGeometry>& geometry : mesh->geometries)
            {
                if (!geometry || geometry->type != donut::engine::MeshGeometryPrimitiveType::Triangles)
                    continue;

                if (!geometry->material)
                    continue;

                auto materialIt = materialIndices.find(geometry->material->materialID);
                if (materialIt == materialIndices.end())
                {
                    const uint32_t newIndex = uint32_t(m_Scene.materials.size());
                    m_Scene.materials.push_back(geometry->material);
                    materialIt = materialIndices.emplace(geometry->material->materialID, newIndex).first;
                }

                gpu::DrawRecord draw;
                draw.debugName = mesh->name;
                draw.bufferGroupIndex = bufferGroupIndex;
                draw.meshIndex = uint32_t(mesh->globalMeshIndex);
                draw.instanceIndex = instanceIndex;
                draw.materialIndex = materialIt->second;
                draw.firstIndex = mesh->indexOffset + geometry->indexOffsetInMesh;
                draw.indexCount = geometry->numIndices;
                draw.baseVertex = int32_t(mesh->vertexOffset + geometry->vertexOffsetInMesh);
                draw.objectToWorld = objectToWorld;
                draw.prevObjectToWorld = prevObjectToWorld;
                draw.worldBounds = gpu::TransformBounds(mesh->objectSpaceBounds, objectToWorld);

                if (!draw.worldBounds.isempty())
                    worldBounds = worldBounds.isempty() ? draw.worldBounds : (worldBounds | draw.worldBounds);

                m_Scene.geometry.draws.push_back(std::move(draw));
            }

            ++instanceIndex;
        }

        m_Scene.geometry.worldBounds = worldBounds;
    }

    void SceneHost::CollectStats()
    {
        SceneStats stats;
        stats.lights = uint32_t(m_Scene.lights.size());

        if (m_Scene.graph)
        {
            std::unordered_set<const donut::engine::MeshInfo*> uniqueMeshes;

            for (const std::shared_ptr<donut::engine::MeshInstance>& instance : m_Scene.graph->GetMeshInstances())
            {
                if (!instance || !instance->GetMesh())
                    continue;

                ++stats.instances;

                const donut::engine::MeshInfo* mesh = instance->GetMesh().get();
                if (uniqueMeshes.insert(mesh).second)
                {
                    ++stats.meshes;
                    stats.vertices += mesh->totalVertices;
                    stats.triangles += mesh->totalIndices / 3;
                }
            }
        }

        m_Scene.stats = stats;
    }
}
