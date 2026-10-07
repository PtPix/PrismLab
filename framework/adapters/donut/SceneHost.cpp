#include "SceneHost.h"

#include "LightAdapter.h"
#include "ProceduralScene.h"

#include <donut/core/log.h>

#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace Prism::Adapter
{
	namespace
	{
		bool EndsWith(const std::string& Value, const char* Suffix)
		{
			const size_t SuffixLength = std::strlen(Suffix);
			return Value.size() >= SuffixLength &&
				   Value.compare(Value.size() - SuffixLength, SuffixLength, Suffix) == 0;
		}
	} // namespace

	FStatus FSceneHost::Load(nvrhi::IDevice* InDevice,
							 const std::shared_ptr<donut::engine::ShaderFactory>& InShaderFactory,
							 const std::shared_ptr<donut::vfs::IFileSystem>& InFileSystem,
							 const FScenePreset& InScenePreset, const FLightingPreset& InLightingPreset)
	{
		Reset();

		Device = InDevice;
		FileSystem = InFileSystem;

		const std::string& Source = InScenePreset.Source;
		const bool bWantsAssetScene = Source == "gltf" || Source == "glb" || EndsWith(Source, ".gltf") ||
									  EndsWith(Source, ".glb") || !InScenePreset.Asset.empty();

		if (bWantsAssetScene)
		{
			if (InScenePreset.Asset.empty())
				return FStatus::Error(EErrorCode::InvalidArgument,
									  "scene.asset must name a scene .json, .gltf or .glb file");

			if (!InShaderFactory)
				return FStatus::Error(EErrorCode::NotInitialized,
									  "a shader factory is required to load an asset scene");

			TextureCache = std::make_shared<donut::engine::TextureCache>(Device, FileSystem, nullptr);
			LoadedScene = std::make_unique<donut::engine::Scene>(Device, *InShaderFactory, FileSystem, TextureCache,
																 nullptr, nullptr);

			donut::log::info("Prism: loading scene asset '%s'...", InScenePreset.Asset.c_str());

			if (!LoadedScene->Load(InScenePreset.Asset))
			{
				LoadedScene.reset();
				TextureCache.reset();
				return FStatus::Error(EErrorCode::ResourceMissing,
									  "failed to load scene asset: " + InScenePreset.Asset);
			}

			LoadedScene->FinishedLoading(0);

			nvrhi::CommandListHandle Commands = Device->createCommandList();
			Commands->open();
			LoadedScene->Refresh(Commands, 0);
			Commands->close();
			Device->executeCommandList(Commands);
			Device->waitForIdle();

			Scene.Graph = LoadedScene->GetSceneGraph();
			Scene.SharedBuffers = nullptr;
			Scene.Description = "asset scene: " + InScenePreset.Asset;
			Scene.Lights = CollectLights(*Scene.Graph);

			BuildGeometryBatchFromSceneGraph();
			CollectStats();

			donut::log::info("Prism: asset scene ready -- %u meshes / %u instances / %u lights / %u triangles.",
							 Scene.Stats.Meshes, Scene.Stats.Instances, Scene.Stats.Lights, Scene.Stats.Triangles);

			return FStatus::Ok();
		}

		nvrhi::CommandListHandle Commands = Device->createCommandList();
		Commands->open();
		Scene = CreateProceduralScene(Device, Commands, InLightingPreset);
		Commands->close();
		Device->executeCommandList(Commands);

		return FStatus::Ok();
	}

	void FSceneHost::Reset()
	{
		Scene = FSceneData{};
		LoadedScene.reset();
		TextureCache.reset();
	}

	void FSceneHost::Update(nvrhi::ICommandList* Commands, uint32_t FrameIndex)
	{
		if (!LoadedScene)
			return;

		LoadedScene->Refresh(Commands, FrameIndex);
	}

	void FSceneHost::BuildGeometryBatchFromSceneGraph()
	{
		Scene.Geometry = Gpu::FGeometryBatch{};
		Scene.Materials.clear();

		if (!Scene.Graph)
			return;

		// 同一批几何体共享一组缓冲；用指针标识分组，避免假设整个场景只有一组。
		std::unordered_map<const donut::engine::BufferGroup*, uint32_t> BufferGroupIndices;
		std::unordered_map<int, uint32_t> MaterialIndices;

		dm::box3 WorldBounds = dm::box3::empty();
		uint32_t InstanceIndex = 0;

		for (const std::shared_ptr<donut::engine::MeshInstance>& Instance : Scene.Graph->GetMeshInstances())
		{
			if (!Instance)
				continue;

			const std::shared_ptr<donut::engine::MeshInfo>& Mesh = Instance->GetMesh();
			if (!Mesh || !Mesh->buffers)
				continue;

			// 首版只支持不透明、无形变的三角形；蒙皮与曲线几何在此明确跳过。
			if (Mesh->type != donut::engine::MeshType::Triangles)
				continue;

			const donut::engine::BufferGroup* Group = Mesh->buffers.get();
			auto GroupIt = BufferGroupIndices.find(Group);
			if (GroupIt == BufferGroupIndices.end())
			{
				Gpu::FGeometryBuffers Buffers;
				Buffers.VertexBuffer = Mesh->buffers->vertexBuffer;
				Buffers.IndexBuffer = Mesh->buffers->indexBuffer;
				Buffers.PositionRange = Mesh->buffers->getVertexBufferRange(donut::engine::VertexAttribute::Position);
				Buffers.TexCoordRange = Mesh->buffers->getVertexBufferRange(donut::engine::VertexAttribute::TexCoord1);
				Buffers.NormalRange = Mesh->buffers->getVertexBufferRange(donut::engine::VertexAttribute::Normal);
				Buffers.TangentRange = Mesh->buffers->getVertexBufferRange(donut::engine::VertexAttribute::Tangent);

				if (!Buffers.IsValid())
					continue;

				const uint32_t NewIndex = uint32_t(Scene.Geometry.BufferGroups.size());
				Scene.Geometry.BufferGroups.push_back(Buffers);
				GroupIt = BufferGroupIndices.emplace(Group, NewIndex).first;
			}

			const uint32_t BufferGroupIndex = GroupIt->second;
			const dm::affine3 ObjectToWorld =
				Instance->GetNode() ? Instance->GetNode()->GetLocalToWorldTransformFloat() : dm::affine3::identity();
			const dm::affine3 PrevObjectToWorld =
				Instance->GetNode() ? Instance->GetNode()->GetPrevLocalToWorldTransformFloat() : ObjectToWorld;

			for (const std::shared_ptr<donut::engine::MeshGeometry>& Geometry : Mesh->geometries)
			{
				if (!Geometry || Geometry->type != donut::engine::MeshGeometryPrimitiveType::Triangles)
					continue;

				if (!Geometry->material)
					continue;

				auto MaterialIt = MaterialIndices.find(Geometry->material->materialID);
				if (MaterialIt == MaterialIndices.end())
				{
					const uint32_t NewIndex = uint32_t(Scene.Materials.size());
					Scene.Materials.push_back(Geometry->material);
					MaterialIt = MaterialIndices.emplace(Geometry->material->materialID, NewIndex).first;
				}

				Gpu::FDrawRecord Draw;
				Draw.DebugName = Mesh->name;
				Draw.BufferGroupIndex = BufferGroupIndex;
				Draw.MeshIndex = uint32_t(Mesh->globalMeshIndex);
				Draw.InstanceIndex = InstanceIndex;
				Draw.MaterialIndex = MaterialIt->second;
				Draw.FirstIndex = Mesh->indexOffset + Geometry->indexOffsetInMesh;
				Draw.IndexCount = Geometry->numIndices;
				Draw.BaseVertex = int32_t(Mesh->vertexOffset + Geometry->vertexOffsetInMesh);
				Draw.ObjectToWorld = ObjectToWorld;
				Draw.PrevObjectToWorld = PrevObjectToWorld;
				Draw.WorldBounds = Gpu::TransformBounds(Mesh->objectSpaceBounds, ObjectToWorld);

				if (!Draw.WorldBounds.isempty())
					WorldBounds = WorldBounds.isempty() ? Draw.WorldBounds : (WorldBounds | Draw.WorldBounds);

				Scene.Geometry.Draws.push_back(std::move(Draw));
			}

			++InstanceIndex;
		}

		Scene.Geometry.WorldBounds = WorldBounds;
	}

	void FSceneHost::CollectStats()
	{
		FSceneStats Stats;
		Stats.Lights = uint32_t(Scene.Lights.size());

		if (Scene.Graph)
		{
			std::unordered_set<const donut::engine::MeshInfo*> UniqueMeshes;

			for (const std::shared_ptr<donut::engine::MeshInstance>& Instance : Scene.Graph->GetMeshInstances())
			{
				if (!Instance || !Instance->GetMesh())
					continue;

				++Stats.Instances;

				const donut::engine::MeshInfo* Mesh = Instance->GetMesh().get();
				if (UniqueMeshes.insert(Mesh).second)
				{
					++Stats.Meshes;
					Stats.Vertices += Mesh->totalVertices;
					Stats.Triangles += Mesh->totalIndices / 3;
				}
			}
		}

		Scene.Stats = Stats;
	}
} // namespace Prism::Adapter
