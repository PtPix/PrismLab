#pragma once
#include <framework/scene/SceneStats.h>

// Donut facilities: the scene handed to every experiment.
//
// The scene stays in Donut form (graph, buffers, materials) because the shared scene pipeline draws it
// through Donut's passes; algorithm-side passes consume the shared data below: light records and the
// backend geometry batch, which contain no Donut scene types.

#include <framework/adapters/donut/GeometryBatch.h>

#include <framework/scene/LightData.h>

#include <donut/engine/SceneGraph.h>
#include <donut/engine/SceneTypes.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace prism::adapter
{
    struct SceneData
    {
        std::shared_ptr<donut::engine::SceneGraph> graph;
        std::shared_ptr<donut::engine::BufferGroup> sharedBuffers;

        // DrawRecord::materialIndex 指向这个列表；材质常量缓冲由 Donut 维护。
        std::vector<std::shared_ptr<donut::engine::Material>> materials;

        // 契约数据：算法只看到这些，不接触 Donut 场景类型。
        std::vector<prism::LightRecord> lights;

        // 自绘 Pass（阴影图、深度预pass、GBuffer）使用的批次视图。
        gpu::GeometryBatch geometry;

        SceneStats stats;
        std::string description = "(none)";

        [[nodiscard]] bool IsLoaded() const { return graph != nullptr; }
        [[nodiscard]] bool SupportsCustomPasses() const { return geometry.IsValid(); }
    };
}
