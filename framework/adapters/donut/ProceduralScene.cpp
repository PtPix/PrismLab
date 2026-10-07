#include "ProceduralScene.h"

#include "LightAdapter.h"

#include <donut/core/log.h>
#include <donut/core/math/math.h>
#include <nvrhi/utils.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace dm = donut::math;

// Headers under donut/shaders are shared between HLSL and C++ and use names like uint / float3x4
// unqualified, so donut::math must be visible globally (same as Donut's own samples).
using namespace donut::math;

#include <donut/shaders/bindless.h>
#include <donut/shaders/material_cb.h>

namespace Prism::Adapter
{
	namespace
	{
		constexpr float KPi = 3.14159265358979323846f;

		struct FMeshSource
		{
			std::vector<dm::float3> Positions;
			std::vector<dm::float2> Texcoords;
			std::vector<dm::float3> Normals;
			std::vector<dm::float3> Tangents;
			std::vector<uint32_t> Indices;
		};

		dm::float3 Perpendicular(const dm::float3& Value)
		{
			// Unrelated to Donut's forward shading conventions: just builds a stable basis for the face.
			const dm::float3 Reference =
				(std::fabs(Value.y) < 0.9f) ? dm::float3(0.f, 1.f, 0.f) : dm::float3(1.f, 0.f, 0.f);

			return dm::normalize(dm::cross(Reference, Value));
		}

		// Winding convention: cross(v1 - v0, v2 - v0) == outward normal, i.e. front faces under
		// the default D3D culling mode.
		void AddBox(FMeshSource& Destination, const dm::float3& Center, const dm::float3& HalfSize)
		{
			static const dm::float3 FaceNormals[6] = {
				dm::float3(1.f, 0.f, 0.f),	dm::float3(-1.f, 0.f, 0.f), dm::float3(0.f, 1.f, 0.f),
				dm::float3(0.f, -1.f, 0.f), dm::float3(0.f, 0.f, 1.f),	dm::float3(0.f, 0.f, -1.f),
			};

			static const dm::float2 CornerUVs[4] = {
				dm::float2(0.f, 0.f),
				dm::float2(1.f, 0.f),
				dm::float2(1.f, 1.f),
				dm::float2(0.f, 1.f),
			};

			for (const dm::float3& Normal : FaceNormals)
			{
				const dm::float3 Tangent = Perpendicular(Normal);
				const dm::float3 Bitangent = dm::cross(Normal, Tangent);

				const float ExtentN = dm::dot(HalfSize, dm::abs(Normal));
				const float ExtentT = dm::dot(HalfSize, dm::abs(Tangent));
				const float ExtentB = dm::dot(HalfSize, dm::abs(Bitangent));

				const dm::float3 FaceCenter = Center + Normal * ExtentN;
				const dm::float3 Corners[4] = {
					FaceCenter - Tangent * ExtentT - Bitangent * ExtentB,
					FaceCenter + Tangent * ExtentT - Bitangent * ExtentB,
					FaceCenter + Tangent * ExtentT + Bitangent * ExtentB,
					FaceCenter - Tangent * ExtentT + Bitangent * ExtentB,
				};

				const uint32_t Base = uint32_t(Destination.Positions.size());

				for (int Corner = 0; Corner < 4; ++Corner)
				{
					Destination.Positions.push_back(Corners[Corner]);
					Destination.Texcoords.push_back(CornerUVs[Corner]);
					Destination.Normals.push_back(Normal);
					Destination.Tangents.push_back(Tangent);
				}

				Destination.Indices.push_back(Base + 0);
				Destination.Indices.push_back(Base + 1);
				Destination.Indices.push_back(Base + 2);
				Destination.Indices.push_back(Base + 0);
				Destination.Indices.push_back(Base + 2);
				Destination.Indices.push_back(Base + 3);
			}
		}

		void AddSphere(FMeshSource& Destination, const dm::float3& Center, float Radius, uint32_t Segments,
					   uint32_t Rings)
		{
			const uint32_t Base = uint32_t(Destination.Positions.size());

			for (uint32_t Ring = 0; Ring <= Rings; ++Ring)
			{
				const float Theta = KPi * float(Ring) / float(Rings);
				const float SinTheta = std::sin(Theta);
				const float CosTheta = std::cos(Theta);

				for (uint32_t Segment = 0; Segment <= Segments; ++Segment)
				{
					const float Phi = 2.f * KPi * float(Segment) / float(Segments);
					const dm::float3 Direction =
						dm::float3(SinTheta * std::cos(Phi), CosTheta, SinTheta * std::sin(Phi));

					Destination.Positions.push_back(Center + Direction * Radius);
					Destination.Texcoords.push_back(
						dm::float2(float(Segment) / float(Segments), float(Ring) / float(Rings)));
					Destination.Normals.push_back(Direction);
					Destination.Tangents.push_back(dm::normalize(dm::float3(-std::sin(Phi), 0.f, std::cos(Phi))));
				}
			}

			for (uint32_t Ring = 0; Ring < Rings; ++Ring)
			{
				for (uint32_t Segment = 0; Segment < Segments; ++Segment)
				{
					const uint32_t V00 = Base + Ring * (Segments + 1) + Segment;
					const uint32_t V01 = V00 + 1;			 // +phi (east)
					const uint32_t V10 = V00 + Segments + 1; // +theta (south)
					const uint32_t V11 = V10 + 1;

					Destination.Indices.push_back(V00);
					Destination.Indices.push_back(V01);
					Destination.Indices.push_back(V11);

					Destination.Indices.push_back(V00);
					Destination.Indices.push_back(V11);
					Destination.Indices.push_back(V10);
				}
			}
		}

		struct FPartDescription
		{
			std::string Name;
			uint32_t MaterialIndex = 0;
			uint32_t IndexOffset = 0;
			uint32_t IndexCount = 0;
			uint32_t VertexOffset = 0;
			uint32_t VertexCount = 0;
			dm::box3 Bounds;
		};

		struct FPlacement
		{
			uint32_t PartIndex = 0;
			dm::double3 Translation = dm::double3(0.0);
			dm::dquat Rotation = dm::dquat::identity();
			dm::double3 Scaling = dm::double3(1.0);
		};

		uint32_t CreateMaterial(nvrhi::IDevice* Device, nvrhi::ICommandList* CommandList,
								std::vector<std::shared_ptr<donut::engine::Material>>& Materials,
								const std::string& Name, const dm::float3& BaseColor, float Roughness, float Metalness,
								const dm::float3& EmissiveColor = dm::float3(0.f), float EmissiveIntensity = 1.f)
		{
			auto Material = std::make_shared<donut::engine::Material>();

			Material->name = Name;
			Material->domain = donut::engine::MaterialDomain::Opaque;
			Material->baseOrDiffuseColor = BaseColor;
			Material->roughness = Roughness;
			Material->metalness = Metalness;
			Material->emissiveColor = EmissiveColor;
			Material->emissiveIntensity = EmissiveIntensity;

			// Everything comes from constants, no textures are referenced.
			Material->enableBaseOrDiffuseTexture = false;
			Material->enableMetalRoughOrSpecularTexture = false;
			Material->enableNormalTexture = false;
			Material->enableEmissiveTexture = false;
			Material->enableOcclusionTexture = false;
			Material->enableTransmissionTexture = false;
			Material->enableOpacityTexture = false;

			// Material constants are written once at init, but a material may be edited by later
			// milestones, so this uses a volatile constant buffer (allows multiple versions per frame).
			const nvrhi::BufferDesc ConstantBufferDesc =
				nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(MaterialConstants), Name.c_str(), 16);
			Material->materialConstants = Device->createBuffer(ConstantBufferDesc);

			MaterialConstants Constants = {};
			Material->FillConstantBuffer(Constants);
			CommandList->writeBuffer(Material->materialConstants, &Constants, sizeof(Constants));

			Materials.push_back(Material);
			return uint32_t(Materials.size()) - 1;
		}
	} // namespace

	FSceneData CreateProceduralScene(nvrhi::IDevice* Device, nvrhi::ICommandList* CommandList,
									 const FLightingPreset& Lighting)
	{
		FSceneData Scene;

		FMeshSource Source;
		std::vector<FPartDescription> Parts;
		std::vector<FPlacement> Placements;

		auto BeginPart = [&](const char* Name, uint32_t MaterialIndex) -> uint32_t
		{
			FPartDescription Part;
			Part.Name = Name;
			Part.MaterialIndex = MaterialIndex;
			Part.IndexOffset = uint32_t(Source.Indices.size());
			Part.VertexOffset = uint32_t(Source.Positions.size());
			Parts.push_back(std::move(Part));
			return uint32_t(Parts.size()) - 1;
		};

		auto EndPart = [&](uint32_t PartIndex)
		{
			FPartDescription& Part = Parts[PartIndex];
			Part.IndexCount = uint32_t(Source.Indices.size()) - Part.IndexOffset;
			Part.VertexCount = uint32_t(Source.Positions.size()) - Part.VertexOffset;

			dm::float3 Minimum(std::numeric_limits<float>::max());
			dm::float3 Maximum(-std::numeric_limits<float>::max());
			for (size_t I = Part.VertexOffset; I < Source.Positions.size(); ++I)
			{
				Minimum = dm::min(Minimum, Source.Positions[I]);
				Maximum = dm::max(Maximum, Source.Positions[I]);
			}
			Part.Bounds = dm::box3(Minimum, Maximum);
		};

		// --- geometry and materials: each part is a separate mesh and may have several instances ---
		{
			const uint32_t Material = CreateMaterial(Device, CommandList, Scene.Materials, "GroundMaterial",
													 dm::float3(0.30f, 0.31f, 0.33f), 0.95f, 0.f);
			const uint32_t Part = BeginPart("Ground", Material);
			AddBox(Source, dm::float3(0.f, -0.05f, 0.f), dm::float3(10.f, 0.05f, 10.f));
			EndPart(Part);
			Placements.push_back({Part, dm::double3(0.0, 0.0, 0.0)});
		}

		{
			const uint32_t Material = CreateMaterial(Device, CommandList, Scene.Materials, "MetalSphereMaterial",
													 dm::float3(0.95f, 0.86f, 0.70f), 0.20f, 1.f);
			const uint32_t Part = BeginPart("MetalSphere", Material);
			AddSphere(Source, dm::float3(0.f), 0.8f, 48, 32);
			EndPart(Part);
			Placements.push_back({Part, dm::double3(0.0, 0.8, 0.0)});
		}

		// Four instances of the same mesh, to exercise instance indices and per-instance transforms.
		{
			const uint32_t Material = CreateMaterial(Device, CommandList, Scene.Materials, "CubeMaterial",
													 dm::float3(0.72f, 0.25f, 0.20f), 0.35f, 0.f);
			const uint32_t Part = BeginPart("Cube", Material);
			AddBox(Source, dm::float3(0.f), dm::float3(0.35f));
			EndPart(Part);

			Placements.push_back({Part, dm::double3(-2.2, 0.35, 1.4)});
			Placements.push_back({Part, dm::double3(2.2, 0.35, 1.4)});
			Placements.push_back({Part, dm::double3(-2.2, 0.35, -1.4)});
			Placements.push_back({Part, dm::double3(2.2, 0.35, -1.4)});
		}

		{
			const uint32_t Material = CreateMaterial(Device, CommandList, Scene.Materials, "BackWallMaterial",
													 dm::float3(0.45f, 0.48f, 0.52f), 0.80f, 0.f);
			const uint32_t Part = BeginPart("BackWall", Material);
			AddBox(Source, dm::float3(0.f), dm::float3(6.f, 1.6f, 0.1f));
			EndPart(Part);
			Placements.push_back({Part, dm::double3(0.0, 1.6, 4.0)});
		}

		{
			const uint32_t Material = CreateMaterial(Device, CommandList, Scene.Materials, "EmissiveCubeMaterial",
													 dm::float3(0.f), 0.5f, 0.f, dm::float3(1.f, 0.55f, 0.15f), 6.f);
			const uint32_t Part = BeginPart("EmissiveCube", Material);
			AddBox(Source, dm::float3(0.f), dm::float3(0.25f));
			EndPart(Part);
			Placements.push_back({Part, dm::double3(0.0, 0.25, -3.0)});
		}

		// --- vertex / index / instance buffers ---
		const uint32_t VertexCount = uint32_t(Source.Positions.size());
		const uint32_t IndexCount = uint32_t(Source.Indices.size());
		const uint32_t InstanceCount = uint32_t(Placements.size());

		// Donut's forward vertex shader reads packed normals / tangents (4-byte RGBA8_SNORM)
		// straight out of the raw buffers.
		std::vector<uint32_t> PackedNormals(VertexCount);
		std::vector<uint32_t> PackedTangents(VertexCount);
		for (uint32_t I = 0; I < VertexCount; ++I)
		{
			PackedNormals[I] = dm::vectorToSnorm8(dm::float4(Source.Normals[I], 0.f));
			PackedTangents[I] = dm::vectorToSnorm8(dm::float4(Source.Tangents[I], 1.f));
		}

		const uint64_t PositionSize = uint64_t(VertexCount) * sizeof(dm::float3);
		const uint64_t TexcoordSize = uint64_t(VertexCount) * sizeof(dm::float2);
		const uint64_t NormalSize = uint64_t(VertexCount) * sizeof(uint32_t);
		const uint64_t TangentSize = uint64_t(VertexCount) * sizeof(uint32_t);
		const uint64_t VertexBufferSize = PositionSize + TexcoordSize + NormalSize + TangentSize;

		const uint32_t PositionOffset = 0;
		const uint32_t TexcoordOffset = uint32_t(PositionSize);
		const uint32_t NormalOffset = uint32_t(PositionSize + TexcoordSize);
		const uint32_t TangentOffset = uint32_t(PositionSize + TexcoordSize + NormalSize);

		auto Buffers = std::make_shared<donut::engine::BufferGroup>();

		{
			nvrhi::BufferDesc Desc;
			Desc.byteSize = VertexBufferSize;
			Desc.debugName = "HostSceneVertexBuffer";
			Desc.canHaveRawViews = true;
			Desc.isVertexBuffer = true;
			Desc.initialState = nvrhi::ResourceStates::CopyDest;
			Buffers->vertexBuffer = Device->createBuffer(Desc);

			CommandList->beginTrackingBufferState(Buffers->vertexBuffer, nvrhi::ResourceStates::CopyDest);
			CommandList->writeBuffer(Buffers->vertexBuffer, Source.Positions.data(), PositionSize, PositionOffset);
			CommandList->writeBuffer(Buffers->vertexBuffer, Source.Texcoords.data(), TexcoordSize, TexcoordOffset);
			CommandList->writeBuffer(Buffers->vertexBuffer, PackedNormals.data(), NormalSize, NormalOffset);
			CommandList->writeBuffer(Buffers->vertexBuffer, PackedTangents.data(), TangentSize, TangentOffset);
			CommandList->setPermanentBufferState(Buffers->vertexBuffer, nvrhi::ResourceStates::ShaderResource);
		}

		{
			nvrhi::BufferDesc Desc;
			Desc.byteSize = uint64_t(IndexCount) * sizeof(uint32_t);
			Desc.debugName = "HostSceneIndexBuffer";
			Desc.format = nvrhi::Format::R32_UINT;
			Desc.isIndexBuffer = true;
			Desc.initialState = nvrhi::ResourceStates::CopyDest;
			Buffers->indexBuffer = Device->createBuffer(Desc);

			CommandList->beginTrackingBufferState(Buffers->indexBuffer, nvrhi::ResourceStates::CopyDest);
			CommandList->writeBuffer(Buffers->indexBuffer, Source.Indices.data(), Desc.byteSize, 0);
			CommandList->setPermanentBufferState(Buffers->indexBuffer, nvrhi::ResourceStates::IndexBuffer);
		}

		const uint64_t InstanceBufferSize = uint64_t(InstanceCount) * sizeof(InstanceData);
		{
			nvrhi::BufferDesc Desc;
			Desc.byteSize = InstanceBufferSize;
			Desc.structStride = sizeof(InstanceData);
			Desc.debugName = "HostSceneInstanceBuffer";
			Desc.canHaveRawViews = true;
			Desc.isVertexBuffer = true;
			Desc.initialState = nvrhi::ResourceStates::CopyDest;
			Buffers->instanceBuffer = Device->createBuffer(Desc);
		}

		Buffers->getVertexBufferRange(donut::engine::VertexAttribute::Position) =
			nvrhi::BufferRange(PositionOffset, PositionSize);
		Buffers->getVertexBufferRange(donut::engine::VertexAttribute::TexCoord1) =
			nvrhi::BufferRange(TexcoordOffset, TexcoordSize);
		Buffers->getVertexBufferRange(donut::engine::VertexAttribute::Normal) =
			nvrhi::BufferRange(NormalOffset, NormalSize);
		Buffers->getVertexBufferRange(donut::engine::VertexAttribute::Tangent) =
			nvrhi::BufferRange(TangentOffset, TangentSize);
		Buffers->getVertexBufferRange(donut::engine::VertexAttribute::Transform) =
			nvrhi::BufferRange(0, InstanceBufferSize);

		// --- scene graph ---
		auto Graph = std::make_shared<donut::engine::SceneGraph>();
		auto Root = std::make_shared<donut::engine::SceneGraphNode>();
		Root->SetName("HostSceneRoot");
		Graph->SetRootNode(Root);

		std::vector<std::shared_ptr<donut::engine::MeshInfo>> Meshes(Parts.size());
		for (size_t I = 0; I < Parts.size(); ++I)
		{
			const FPartDescription& Part = Parts[I];

			auto Geometry = std::make_shared<donut::engine::MeshGeometry>();
			Geometry->material = Scene.Materials[Part.MaterialIndex];
			Geometry->indexOffsetInMesh = 0;
			Geometry->vertexOffsetInMesh = 0;
			Geometry->numIndices = Part.IndexCount;
			Geometry->numVertices = Part.VertexCount;
			Geometry->objectSpaceBounds = Part.Bounds;

			auto Mesh = std::make_shared<donut::engine::MeshInfo>();
			Mesh->name = Part.Name;
			Mesh->buffers = Buffers;
			Mesh->geometries.push_back(Geometry);
			Mesh->objectSpaceBounds = Part.Bounds;
			Mesh->indexOffset = Part.IndexOffset;
			Mesh->vertexOffset = Part.VertexOffset;
			Mesh->totalIndices = Part.IndexCount;
			Mesh->totalVertices = Part.VertexCount;

			Meshes[I] = Mesh;
		}

		std::vector<std::shared_ptr<donut::engine::MeshInstance>> Instances(Placements.size());
		std::vector<std::shared_ptr<donut::engine::SceneGraphNode>> InstanceNodes(Placements.size());

		for (size_t I = 0; I < Placements.size(); ++I)
		{
			const FPlacement& Placement = Placements[I];

			auto Instance = std::make_shared<donut::engine::MeshInstance>(Meshes[Placement.PartIndex]);
			auto Node = std::make_shared<donut::engine::SceneGraphNode>();
			Node->SetName(Parts[Placement.PartIndex].Name + "Node");
			Node->SetTransform(&Placement.Translation, &Placement.Rotation, &Placement.Scaling);
			Node->SetLeaf(Instance);
			Graph->Attach(Root, Node);

			Instances[I] = Instance;
			InstanceNodes[I] = Node;
		}

		// --- lights ---
		// Note: a Donut Leaf (including Light) must be attached to the scene graph before SetName /
		// SetDirection / SetPosition can be used; those go through the owning node to compute world
		// transforms and assert when the leaf is not attached yet.
		auto Sun = std::make_shared<donut::engine::DirectionalLight>();
		Graph->AttachLeafNode(Root, Sun);
		Sun->SetName("Sun");
		Sun->irradiance = Lighting.SunIrradiance;
		Sun->angularSize = 0.53f; // degrees, roughly the angular size of the real sun
		Sun->SetDirection(dm::double3(dm::normalize(Lighting.SunDirection)));

		auto FillLight = std::make_shared<donut::engine::PointLight>();
		Graph->AttachLeafNode(Root, FillLight);
		FillLight->SetName("FillLight");
		FillLight->SetPosition(dm::double3(-3.0, 2.5, 2.0));
		FillLight->intensity = 8.f;
		FillLight->radius = 0.15f;

		// Instance and geometry-instance indices are only assigned by Refresh, so instance data
		// must be written afterwards.
		Graph->Refresh(0);

		std::vector<InstanceData> InstanceDataArray(InstanceCount);
		for (uint32_t I = 0; I < InstanceCount; ++I)
		{
			const int Index = Instances[I]->GetInstanceIndex();
			if (Index < 0 || uint32_t(Index) >= InstanceCount)
			{
				donut::log::error("Prism: instance %u did not receive a valid instance index (%d).", I, Index);
				continue;
			}

			InstanceData& Data = InstanceDataArray[uint32_t(Index)];
			Data.flags = 0;
			Data.firstGeometryInstanceIndex = Instances[I]->GetGeometryInstanceIndex();
			Data.firstGeometryIndex = 0;
			Data.numGeometries = uint32_t(Meshes[Placements[I].PartIndex]->geometries.size());

			const dm::affine3 Transform = InstanceNodes[I]->GetLocalToWorldTransformFloat();
			Data.transform = dm::float3x4(dm::transpose(dm::affineToHomogeneous(Transform)));
			Data.prevTransform = Data.transform;
		}

		CommandList->beginTrackingBufferState(Buffers->instanceBuffer, nvrhi::ResourceStates::CopyDest);
		CommandList->writeBuffer(Buffers->instanceBuffer, InstanceDataArray.data(), InstanceBufferSize, 0);
		CommandList->setPermanentBufferState(Buffers->instanceBuffer, nvrhi::ResourceStates::ShaderResource);

		// --- backend geometry batch for algorithm-side passes (no Donut types in it) ---
		Gpu::FGeometryBuffers BatchBuffers;
		BatchBuffers.VertexBuffer = Buffers->vertexBuffer;
		BatchBuffers.IndexBuffer = Buffers->indexBuffer;
		BatchBuffers.PositionRange = nvrhi::BufferRange(PositionOffset, PositionSize);
		BatchBuffers.TexCoordRange = nvrhi::BufferRange(TexcoordOffset, TexcoordSize);
		BatchBuffers.NormalRange = nvrhi::BufferRange(NormalOffset, NormalSize);
		BatchBuffers.TangentRange = nvrhi::BufferRange(TangentOffset, TangentSize);
		BatchBuffers.IndexFormat = nvrhi::Format::R32_UINT;

		Scene.Geometry.BufferGroups.push_back(BatchBuffers);

		dm::box3 WorldBounds = dm::box3::empty();
		for (size_t I = 0; I < Placements.size(); ++I)
		{
			const FPartDescription& Part = Parts[Placements[I].PartIndex];

			Gpu::FDrawRecord Draw;
			Draw.DebugName = Part.Name;
			Draw.BufferGroupIndex = 0;
			Draw.MeshIndex = Placements[I].PartIndex;
			Draw.InstanceIndex = uint32_t(I);
			Draw.MaterialIndex = Part.MaterialIndex;
			Draw.FirstIndex = Part.IndexOffset;
			Draw.IndexCount = Part.IndexCount;
			Draw.BaseVertex = int32_t(Part.VertexOffset);
			Draw.ObjectToWorld = InstanceNodes[I]->GetLocalToWorldTransformFloat();
			Draw.PrevObjectToWorld = Draw.ObjectToWorld;
			Draw.WorldBounds = Gpu::TransformBounds(Part.Bounds, Draw.ObjectToWorld);

			WorldBounds = WorldBounds | Draw.WorldBounds;
			Scene.Geometry.Draws.push_back(std::move(Draw));
		}

		Scene.Geometry.WorldBounds = WorldBounds;

		// --- summary ---
		Scene.Graph = Graph;
		Scene.SharedBuffers = Buffers;
		Scene.Lights = CollectLights(*Graph);
		Scene.Description = "procedural test scene (no assets)";
		Scene.Stats.Meshes = uint32_t(Meshes.size());
		Scene.Stats.Instances = InstanceCount;
		Scene.Stats.Lights = uint32_t(Scene.Lights.size());
		Scene.Stats.Vertices = VertexCount;
		Scene.Stats.Triangles = IndexCount / 3;

		donut::log::info(
			"Prism: procedural scene ready -- %u meshes / %u instances / %u lights / %u vertices / %u triangles.",
			Scene.Stats.Meshes, Scene.Stats.Instances, Scene.Stats.Lights, Scene.Stats.Vertices, Scene.Stats.Triangles);

		return Scene;
	}
} // namespace Prism::Adapter
