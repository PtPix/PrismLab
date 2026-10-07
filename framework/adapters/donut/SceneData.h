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

namespace Prism::Adapter
{
	struct FSceneData
	{
		std::shared_ptr<donut::engine::SceneGraph> Graph;
		std::shared_ptr<donut::engine::BufferGroup> SharedBuffers;

		// DrawRecord::materialIndex 指向这个列表；材质常量缓冲由 Donut 维护。
		std::vector<std::shared_ptr<donut::engine::Material>> Materials;

		// 契约数据：算法只看到这些，不接触 Donut 场景类型。
		std::vector<Prism::FLightRecord> Lights;

		// 自绘 Pass（阴影图、深度预pass、GBuffer）使用的批次视图。
		Gpu::FGeometryBatch Geometry;

		FSceneStats Stats;
		std::string Description = "(none)";

		[[nodiscard]] bool IsLoaded() const
		{
			return Graph != nullptr;
		}
		[[nodiscard]] bool SupportsCustomPasses() const
		{
			return Geometry.IsValid();
		}
	};
} // namespace Prism::Adapter
