#pragma once

#include <donut/engine/SceneGraph.h>
#include <donut/engine/SceneTypes.h>
#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <memory>

#include "host_config.h"

namespace renderlab
{
    // Procedural test scene: no external art assets required, ready to render once built.
    // It uses a single shared BufferGroup (vertex / index / instance), matching the layout
    // produced by glTF loading, so the render side will not change when real assets arrive.
    struct ProceduralScene
    {
        std::shared_ptr<donut::engine::SceneGraph> graph;
        std::shared_ptr<donut::engine::BufferGroup> buffers;

        uint32_t meshCount = 0;
        uint32_t instanceCount = 0;
        uint32_t lightCount = 0;
        uint32_t vertexCount = 0;
        uint32_t triangleCount = 0;
    };

    // Must be called with an already open() command list: the first uploads of geometry,
    // material constants and instance data are all recorded into it.
    ProceduralScene CreateProceduralScene(
        nvrhi::IDevice* device,
        nvrhi::ICommandList* commandList,
        const HostLightingPreset& lighting);
}
