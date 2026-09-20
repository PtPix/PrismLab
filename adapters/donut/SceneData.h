#pragma once

// Donut adapter: the scene handed to every lab.
//
// The scene stays in Donut form (graph, buffers, materials) because the shared scene pipeline draws it
// through Donut's passes; algorithm-side passes consume the contract data below: light records and the
// backend geometry batch, which contain no Donut scene types.

#include <backends/nvrhi/common/GeometryBatch.h>

#include <renderlab/contracts/LightData.h>

#include <donut/engine/SceneGraph.h>
#include <donut/engine/SceneTypes.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace renderlab::adapter
{
    struct SceneStats
    {
        uint32_t meshes = 0;
        uint32_t instances = 0;
        uint32_t lights = 0;
        uint32_t vertices = 0;
        uint32_t triangles = 0;
    };

    struct SceneData
    {
        std::shared_ptr<donut::engine::SceneGraph> graph;
        std::shared_ptr<donut::engine::BufferGroup> sharedBuffers;

        // DrawRecord::materialIndex 指向这个列表；材质常量缓冲由 Donut 维护。
        std::vector<std::shared_ptr<donut::engine::Material>> materials;

        // 契约数据：算法只看到这些，不接触 Donut 场景类型。
        std::vector<renderlab::LightRecord> lights;

        // 自绘 Pass（阴影图、深度预pass、GBuffer）使用的批次视图。
        gpu::GeometryBatch geometry;

        SceneStats stats;
        std::string description = "(none)";

        [[nodiscard]] bool IsLoaded() const { return graph != nullptr; }
        [[nodiscard]] bool SupportsCustomPasses() const { return geometry.IsValid(); }
    };
}
