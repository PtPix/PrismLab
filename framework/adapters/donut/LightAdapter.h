#pragma once

// Donut facilities: Donut lights -> shared light records.
//
// The adapter is the only place that knows how a Donut light stores its photometric values. Note the
// angle conventions: Donut's spot angles are full apex angles in degrees, LightRecord stores half
// angles in radians.

#include <framework/scene/LightData.h>

#include <donut/engine/SceneGraph.h>

#include <vector>

namespace Prism::Adapter
{
	std::vector<Prism::FLightRecord> CollectLights(const donut::engine::SceneGraph& Graph);

	// GPU 布局：把 LightRecord 转成固定对齐的结构，供常量/结构化缓冲上传。
	Prism::FGpuLight ToGpuLight(const Prism::FLightRecord& Light);
} // namespace Prism::Adapter
