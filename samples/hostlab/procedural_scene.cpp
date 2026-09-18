#include "procedural_scene.h"

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

namespace renderlab
{
namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    struct MeshSource
    {
        std::vector<dm::float3> positions;
        std::vector<dm::float2> texcoords;
        std::vector<dm::float3> normals;
        std::vector<dm::float3> tangents;
        std::vector<uint32_t> indices;
    };

    dm::float3 Perpendicular(const dm::float3& value)
    {
        // Unrelated to Donut's forward shading conventions: just builds a stable basis for the face.
        const dm::float3 reference = (std::fabs(value.y) < 0.9f)
            ? dm::float3(0.f, 1.f, 0.f)
            : dm::float3(1.f, 0.f, 0.f);

        return dm::normalize(dm::cross(reference, value));
    }

    // Winding convention: cross(v1 - v0, v2 - v0) == outward normal, i.e. front faces under
    // the default D3D culling mode.
    void AddBox(MeshSource& destination, const dm::float3& center, const dm::float3& halfSize)
    {
        static const dm::float3 faceNormals[6] = {
            dm::float3( 1.f,  0.f,  0.f),
            dm::float3(-1.f,  0.f,  0.f),
            dm::float3( 0.f,  1.f,  0.f),
            dm::float3( 0.f, -1.f,  0.f),
            dm::float3( 0.f,  0.f,  1.f),
            dm::float3( 0.f,  0.f, -1.f),
        };

        static const dm::float2 cornerUVs[4] = {
            dm::float2(0.f, 0.f),
            dm::float2(1.f, 0.f),
            dm::float2(1.f, 1.f),
            dm::float2(0.f, 1.f),
        };

        for (const dm::float3& normal : faceNormals)
        {
            const dm::float3 tangent = Perpendicular(normal);
            const dm::float3 bitangent = dm::cross(normal, tangent);

            const float extentN = dm::dot(halfSize, dm::abs(normal));
            const float extentT = dm::dot(halfSize, dm::abs(tangent));
            const float extentB = dm::dot(halfSize, dm::abs(bitangent));

            const dm::float3 faceCenter = center + normal * extentN;
            const dm::float3 corners[4] = {
                faceCenter - tangent * extentT - bitangent * extentB,
                faceCenter + tangent * extentT - bitangent * extentB,
                faceCenter + tangent * extentT + bitangent * extentB,
                faceCenter - tangent * extentT + bitangent * extentB,
            };

            const uint32_t base = uint32_t(destination.positions.size());

            for (int corner = 0; corner < 4; ++corner)
            {
                destination.positions.push_back(corners[corner]);
                destination.texcoords.push_back(cornerUVs[corner]);
                destination.normals.push_back(normal);
                destination.tangents.push_back(tangent);
            }

            destination.indices.push_back(base + 0);
            destination.indices.push_back(base + 1);
            destination.indices.push_back(base + 2);
            destination.indices.push_back(base + 0);
            destination.indices.push_back(base + 2);
            destination.indices.push_back(base + 3);
        }
    }

    void AddSphere(MeshSource& destination, const dm::float3& center, float radius, uint32_t segments, uint32_t rings)
    {
        const uint32_t base = uint32_t(destination.positions.size());

        for (uint32_t ring = 0; ring <= rings; ++ring)
        {
            const float theta = kPi * float(ring) / float(rings);
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            for (uint32_t segment = 0; segment <= segments; ++segment)
            {
                const float phi = 2.f * kPi * float(segment) / float(segments);
                const dm::float3 direction = dm::float3(sinTheta * std::cos(phi), cosTheta, sinTheta * std::sin(phi));

                destination.positions.push_back(center + direction * radius);
                destination.texcoords.push_back(dm::float2(float(segment) / float(segments), float(ring) / float(rings)));
                destination.normals.push_back(direction);
                destination.tangents.push_back(dm::normalize(dm::float3(-std::sin(phi), 0.f, std::cos(phi))));
            }
        }

        for (uint32_t ring = 0; ring < rings; ++ring)
        {
            for (uint32_t segment = 0; segment < segments; ++segment)
            {
                const uint32_t v00 = base + ring * (segments + 1) + segment;
                const uint32_t v01 = v00 + 1;                 // +phi (east)
                const uint32_t v10 = v00 + segments + 1;      // +theta (south)
                const uint32_t v11 = v10 + 1;

                destination.indices.push_back(v00);
                destination.indices.push_back(v01);
                destination.indices.push_back(v11);

                destination.indices.push_back(v00);
                destination.indices.push_back(v11);
                destination.indices.push_back(v10);
            }
        }
    }

    struct PartDescription
    {
        std::string name;
        std::shared_ptr<donut::engine::Material> material;
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        uint32_t vertexOffset = 0;
        uint32_t vertexCount = 0;
        dm::box3 bounds;
    };

    struct Placement
    {
        uint32_t partIndex = 0;
        dm::double3 translation = dm::double3(0.0);
        dm::dquat rotation = dm::dquat::identity();
        dm::double3 scaling = dm::double3(1.0);
    };

    std::shared_ptr<donut::engine::Material> CreateMaterial(
        nvrhi::IDevice* device,
        nvrhi::ICommandList* commandList,
        const std::string& name,
        const dm::float3& baseColor,
        float roughness,
        float metalness,
        const dm::float3& emissiveColor = dm::float3(0.f),
        float emissiveIntensity = 1.f)
    {
        auto material = std::make_shared<donut::engine::Material>();

        material->name = name;
        material->domain = donut::engine::MaterialDomain::Opaque;
        material->baseOrDiffuseColor = baseColor;
        material->roughness = roughness;
        material->metalness = metalness;
        material->emissiveColor = emissiveColor;
        material->emissiveIntensity = emissiveIntensity;

        // Everything comes from constants, no textures are referenced.
        material->enableBaseOrDiffuseTexture = false;
        material->enableMetalRoughOrSpecularTexture = false;
        material->enableNormalTexture = false;
        material->enableEmissiveTexture = false;
        material->enableOcclusionTexture = false;
        material->enableTransmissionTexture = false;
        material->enableOpacityTexture = false;

        // Material constants are written once at init, but a material may be edited by later
        // milestones, so this uses a volatile constant buffer (allows multiple versions per frame).
        const nvrhi::BufferDesc constantBufferDesc =
            nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(MaterialConstants), name.c_str(), 16);
        material->materialConstants = device->createBuffer(constantBufferDesc);

        MaterialConstants constants = {};
        material->FillConstantBuffer(constants);
        commandList->writeBuffer(material->materialConstants, &constants, sizeof(constants));

        return material;
    }
}

ProceduralScene CreateProceduralScene(
    nvrhi::IDevice* device,
    nvrhi::ICommandList* commandList,
    const HostLightingPreset& lighting)
{
    MeshSource source;
    std::vector<PartDescription> parts;
    std::vector<Placement> placements;

    auto beginPart = [&](const char* name, std::shared_ptr<donut::engine::Material> material) -> uint32_t
    {
        PartDescription part;
        part.name = name;
        part.material = std::move(material);
        part.indexOffset = uint32_t(source.indices.size());
        part.vertexOffset = uint32_t(source.positions.size());
        parts.push_back(std::move(part));
        return uint32_t(parts.size()) - 1;
    };

    auto endPart = [&](uint32_t partIndex)
    {
        PartDescription& part = parts[partIndex];
        part.indexCount = uint32_t(source.indices.size()) - part.indexOffset;
        part.vertexCount = uint32_t(source.positions.size()) - part.vertexOffset;

        dm::float3 minimum(std::numeric_limits<float>::max());
        dm::float3 maximum(-std::numeric_limits<float>::max());
        for (size_t i = part.vertexOffset; i < source.positions.size(); ++i)
        {
            minimum = dm::min(minimum, source.positions[i]);
            maximum = dm::max(maximum, source.positions[i]);
        }
        part.bounds = dm::box3(minimum, maximum);
    };

    // --- geometry and materials: each part is a separate mesh and may have several instances ---
    {
        const uint32_t part = beginPart("Ground",
            CreateMaterial(device, commandList, "GroundMaterial", dm::float3(0.30f, 0.31f, 0.33f), 0.95f, 0.f));
        AddBox(source, dm::float3(0.f, -0.05f, 0.f), dm::float3(10.f, 0.05f, 10.f));
        endPart(part);
        placements.push_back({ part, dm::double3(0.0, 0.0, 0.0) });
    }

    {
        const uint32_t part = beginPart("MetalSphere",
            CreateMaterial(device, commandList, "MetalSphereMaterial", dm::float3(0.95f, 0.86f, 0.70f), 0.20f, 1.f));
        AddSphere(source, dm::float3(0.f), 0.8f, 48, 32);
        endPart(part);
        placements.push_back({ part, dm::double3(0.0, 0.8, 0.0) });
    }

    // Four instances of the same mesh, to exercise instance indices and per-instance transforms.
    {
        const uint32_t part = beginPart("Cube",
            CreateMaterial(device, commandList, "CubeMaterial", dm::float3(0.72f, 0.25f, 0.20f), 0.35f, 0.f));
        AddBox(source, dm::float3(0.f), dm::float3(0.35f));
        endPart(part);

        placements.push_back({ part, dm::double3(-2.2, 0.35,  1.4) });
        placements.push_back({ part, dm::double3( 2.2, 0.35,  1.4) });
        placements.push_back({ part, dm::double3(-2.2, 0.35, -1.4) });
        placements.push_back({ part, dm::double3( 2.2, 0.35, -1.4) });
    }

    {
        const uint32_t part = beginPart("BackWall",
            CreateMaterial(device, commandList, "BackWallMaterial", dm::float3(0.45f, 0.48f, 0.52f), 0.80f, 0.f));
        AddBox(source, dm::float3(0.f), dm::float3(6.f, 1.6f, 0.1f));
        endPart(part);
        placements.push_back({ part, dm::double3(0.0, 1.6, 4.0) });
    }

    {
        const uint32_t part = beginPart("EmissiveCube",
            CreateMaterial(device, commandList, "EmissiveCubeMaterial", dm::float3(0.f), 0.5f, 0.f,
                dm::float3(1.f, 0.55f, 0.15f), 6.f));
        AddBox(source, dm::float3(0.f), dm::float3(0.25f));
        endPart(part);
        placements.push_back({ part, dm::double3(0.0, 0.25, -3.0) });
    }

    // --- vertex / index / instance buffers ---
    const uint32_t vertexCount = uint32_t(source.positions.size());
    const uint32_t indexCount = uint32_t(source.indices.size());
    const uint32_t instanceCount = uint32_t(placements.size());

    // Donut's forward vertex shader reads packed normals / tangents (4-byte RGBA8_SNORM)
    // straight out of the raw buffers.
    std::vector<uint32_t> packedNormals(vertexCount);
    std::vector<uint32_t> packedTangents(vertexCount);
    for (uint32_t i = 0; i < vertexCount; ++i)
    {
        packedNormals[i] = dm::vectorToSnorm8(dm::float4(source.normals[i], 0.f));
        packedTangents[i] = dm::vectorToSnorm8(dm::float4(source.tangents[i], 1.f));
    }

    const uint64_t positionSize = uint64_t(vertexCount) * sizeof(dm::float3);
    const uint64_t texcoordSize = uint64_t(vertexCount) * sizeof(dm::float2);
    const uint64_t normalSize = uint64_t(vertexCount) * sizeof(uint32_t);
    const uint64_t tangentSize = uint64_t(vertexCount) * sizeof(uint32_t);
    const uint64_t vertexBufferSize = positionSize + texcoordSize + normalSize + tangentSize;

    const uint32_t positionOffset = 0;
    const uint32_t texcoordOffset = uint32_t(positionSize);
    const uint32_t normalOffset = uint32_t(positionSize + texcoordSize);
    const uint32_t tangentOffset = uint32_t(positionSize + texcoordSize + normalSize);

    auto buffers = std::make_shared<donut::engine::BufferGroup>();

    {
        nvrhi::BufferDesc desc;
        desc.byteSize = vertexBufferSize;
        desc.debugName = "HostSceneVertexBuffer";
        desc.canHaveRawViews = true;
        desc.isVertexBuffer = true;
        desc.initialState = nvrhi::ResourceStates::CopyDest;
        buffers->vertexBuffer = device->createBuffer(desc);

        commandList->beginTrackingBufferState(buffers->vertexBuffer, nvrhi::ResourceStates::CopyDest);
        commandList->writeBuffer(buffers->vertexBuffer, source.positions.data(), positionSize, positionOffset);
        commandList->writeBuffer(buffers->vertexBuffer, source.texcoords.data(), texcoordSize, texcoordOffset);
        commandList->writeBuffer(buffers->vertexBuffer, packedNormals.data(), normalSize, normalOffset);
        commandList->writeBuffer(buffers->vertexBuffer, packedTangents.data(), tangentSize, tangentOffset);
        commandList->setPermanentBufferState(buffers->vertexBuffer, nvrhi::ResourceStates::ShaderResource);
    }

    {
        nvrhi::BufferDesc desc;
        desc.byteSize = uint64_t(indexCount) * sizeof(uint32_t);
        desc.debugName = "HostSceneIndexBuffer";
        desc.format = nvrhi::Format::R32_UINT;
        desc.isIndexBuffer = true;
        desc.initialState = nvrhi::ResourceStates::CopyDest;
        buffers->indexBuffer = device->createBuffer(desc);

        commandList->beginTrackingBufferState(buffers->indexBuffer, nvrhi::ResourceStates::CopyDest);
        commandList->writeBuffer(buffers->indexBuffer, source.indices.data(), desc.byteSize, 0);
        commandList->setPermanentBufferState(buffers->indexBuffer, nvrhi::ResourceStates::IndexBuffer);
    }

    const uint64_t instanceBufferSize = uint64_t(instanceCount) * sizeof(InstanceData);
    {
        nvrhi::BufferDesc desc;
        desc.byteSize = instanceBufferSize;
        desc.structStride = sizeof(InstanceData);
        desc.debugName = "HostSceneInstanceBuffer";
        desc.canHaveRawViews = true;
        desc.isVertexBuffer = true;
        desc.initialState = nvrhi::ResourceStates::CopyDest;
        buffers->instanceBuffer = device->createBuffer(desc);
    }

    buffers->getVertexBufferRange(donut::engine::VertexAttribute::Position) = nvrhi::BufferRange(positionOffset, positionSize);
    buffers->getVertexBufferRange(donut::engine::VertexAttribute::TexCoord1) = nvrhi::BufferRange(texcoordOffset, texcoordSize);
    buffers->getVertexBufferRange(donut::engine::VertexAttribute::Normal) = nvrhi::BufferRange(normalOffset, normalSize);
    buffers->getVertexBufferRange(donut::engine::VertexAttribute::Tangent) = nvrhi::BufferRange(tangentOffset, tangentSize);
    buffers->getVertexBufferRange(donut::engine::VertexAttribute::Transform) = nvrhi::BufferRange(0, instanceBufferSize);

    // --- scene graph ---
    auto graph = std::make_shared<donut::engine::SceneGraph>();
    auto root = std::make_shared<donut::engine::SceneGraphNode>();
    root->SetName("HostSceneRoot");
    graph->SetRootNode(root);

    std::vector<std::shared_ptr<donut::engine::MeshInfo>> meshes(parts.size());
    for (size_t i = 0; i < parts.size(); ++i)
    {
        const PartDescription& part = parts[i];

        auto geometry = std::make_shared<donut::engine::MeshGeometry>();
        geometry->material = part.material;
        geometry->indexOffsetInMesh = 0;
        geometry->vertexOffsetInMesh = 0;
        geometry->numIndices = part.indexCount;
        geometry->numVertices = part.vertexCount;
        geometry->objectSpaceBounds = part.bounds;

        auto mesh = std::make_shared<donut::engine::MeshInfo>();
        mesh->name = part.name;
        mesh->buffers = buffers;
        mesh->geometries.push_back(geometry);
        mesh->objectSpaceBounds = part.bounds;
        mesh->indexOffset = part.indexOffset;
        mesh->vertexOffset = part.vertexOffset;
        mesh->totalIndices = part.indexCount;
        mesh->totalVertices = part.vertexCount;

        meshes[i] = mesh;
    }

    std::vector<std::shared_ptr<donut::engine::MeshInstance>> instances(placements.size());
    std::vector<std::shared_ptr<donut::engine::SceneGraphNode>> instanceNodes(placements.size());

    for (size_t i = 0; i < placements.size(); ++i)
    {
        const Placement& placement = placements[i];

        auto instance = std::make_shared<donut::engine::MeshInstance>(meshes[placement.partIndex]);
        auto node = std::make_shared<donut::engine::SceneGraphNode>();
        node->SetName(parts[placement.partIndex].name + "Node");
        node->SetTransform(&placement.translation, &placement.rotation, &placement.scaling);
        node->SetLeaf(instance);
        graph->Attach(root, node);

        instances[i] = instance;
        instanceNodes[i] = node;
    }

    // --- lights ---
    // Note: a Donut Leaf (including Light) must be attached to the scene graph before SetName /
    // SetDirection / SetPosition can be used; those go through the owning node to compute world
    // transforms and assert when the leaf is not attached yet.
    auto sun = std::make_shared<donut::engine::DirectionalLight>();
    graph->AttachLeafNode(root, sun);
    sun->SetName("Sun");
    sun->irradiance = lighting.sunIrradiance;
    sun->angularSize = 0.53f; // degrees, roughly the angular size of the real sun
    sun->SetDirection(dm::double3(dm::normalize(lighting.sunDirection)));

    auto fillLight = std::make_shared<donut::engine::PointLight>();
    graph->AttachLeafNode(root, fillLight);
    fillLight->SetName("FillLight");
    fillLight->SetPosition(dm::double3(-3.0, 2.5, 2.0));
    fillLight->intensity = 8.f;
    fillLight->radius = 0.15f;

    // Instance and geometry-instance indices are only assigned by Refresh, so instance data
    // must be written afterwards.
    graph->Refresh(0);

    std::vector<InstanceData> instanceData(instanceCount);
    for (uint32_t i = 0; i < instanceCount; ++i)
    {
        const int index = instances[i]->GetInstanceIndex();
        if (index < 0 || uint32_t(index) >= instanceCount)
        {
            donut::log::error("HostLab: instance %u did not receive a valid instance index (%d).", i, index);
            continue;
        }

        InstanceData& data = instanceData[uint32_t(index)];
        data.flags = 0;
        data.firstGeometryInstanceIndex = instances[i]->GetGeometryInstanceIndex();
        data.firstGeometryIndex = 0;
        data.numGeometries = uint32_t(meshes[placements[i].partIndex]->geometries.size());

        const dm::affine3 transform = instanceNodes[i]->GetLocalToWorldTransformFloat();
        data.transform = dm::float3x4(dm::transpose(dm::affineToHomogeneous(transform)));
        data.prevTransform = data.transform;
    }

    commandList->beginTrackingBufferState(buffers->instanceBuffer, nvrhi::ResourceStates::CopyDest);
    commandList->writeBuffer(buffers->instanceBuffer, instanceData.data(), instanceBufferSize, 0);
    commandList->setPermanentBufferState(buffers->instanceBuffer, nvrhi::ResourceStates::ShaderResource);

    ProceduralScene scene;
    scene.graph = graph;
    scene.buffers = buffers;
    scene.meshCount = uint32_t(meshes.size());
    scene.instanceCount = instanceCount;
    scene.lightCount = uint32_t(graph->GetLights().size());
    scene.vertexCount = vertexCount;
    scene.triangleCount = indexCount / 3;

    donut::log::info("HostLab: procedural scene ready -- %u meshes / %u instances / %u lights / %u vertices / %u triangles.",
        scene.meshCount, scene.instanceCount, scene.lightCount, scene.vertexCount, scene.triangleCount);

    return scene;
}
}
