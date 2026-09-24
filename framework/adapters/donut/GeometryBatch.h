#pragma once

// NVRHI layer: backend view of drawable geometry, without Donut scene types.
//
// The Donut facilities (framework/adapters/donut) fill this from a scene graph; passes such as a shadow map, a depth
// prepass or a GBuffer fill consume it and bind their own shaders. The first version supports opaque,
// non-deforming triangles only: alpha test, skinning and displacement need explicit extensions, they
// must not be hidden behind this interface.

#include <framework/render/data/Conventions.h>
#include <framework/core/Types.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <string>
#include <vector>

namespace prism::gpu
{
    // 一组顶点/索引缓冲及其属性布局。glTF 场景可能包含多个缓冲组（每个模型一组），
    // 所以批次用下标引用它们，而不是假定全场只有一个缓冲。
    struct GeometryBuffers
    {
        nvrhi::IBuffer* vertexBuffer = nullptr;
        nvrhi::IBuffer* indexBuffer = nullptr;

        // 顶点属性在 vertexBuffer 中的字节范围；未使用的属性 byteSize 为 0。
        // 位置：float3，法线/切向：R8G8B8A8_SNORM 打包，UV：float2 —— 与 Donut 的 glTF 布局一致。
        nvrhi::BufferRange positionRange;
        nvrhi::BufferRange texCoordRange;
        nvrhi::BufferRange normalRange;
        nvrhi::BufferRange tangentRange;

        nvrhi::Format indexFormat = nvrhi::Format::R32_UINT;
        uint32_t vertexStride = 0;   // 0 表示按属性范围分别绑定（非交错布局）

        [[nodiscard]] bool IsValid() const { return vertexBuffer != nullptr && indexBuffer != nullptr; }
    };

    // One drawable triangle range with its object transform (CPU-side semantics: meters, right-handed).
    struct DrawRecord
    {
        std::string debugName;

        uint32_t bufferGroupIndex = 0;
        uint32_t meshIndex = 0;
        uint32_t instanceIndex = 0;
        uint32_t materialIndex = 0;

        // 元素索引，不是字节偏移
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        int32_t baseVertex = 0;

        dm::affine3 objectToWorld = dm::affine3::identity();
        dm::affine3 prevObjectToWorld = dm::affine3::identity();
        dm::box3 worldBounds;

        [[nodiscard]] uint32_t TriangleCount() const { return indexCount / 3; }
    };

    struct GeometryBatch
    {
        std::vector<GeometryBuffers> bufferGroups;
        std::vector<DrawRecord> draws;
        dm::box3 worldBounds;

        [[nodiscard]] bool IsValid() const { return !bufferGroups.empty() && !draws.empty(); }

        [[nodiscard]] uint64_t GetTriangleCount() const
        {
            uint64_t triangles = 0;
            for (const DrawRecord& draw : draws)
                triangles += draw.TriangleCount();

            return triangles;
        }
    };

    // 世界包围盒：dm::box3 已经提供仿射变换后的保守包围盒
    inline dm::box3 TransformBounds(const dm::box3& bounds, const dm::affine3& transform)
    {
        if (bounds.isempty())
            return bounds;

        return bounds * transform;
    }
}
