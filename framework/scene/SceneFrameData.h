#pragma once

#include "framework/render/data/FrameInfo.h"
#include <cstddef>

namespace prism
{
    using SceneId = uint64_t;

    struct InstanceFrameData
    {
        SceneId id = 0;
        dm::affine3 current = dm::affine3::identity();
        dm::affine3 previous = dm::affine3::identity();
        dm::box3 worldBounds;
        uint32_t materialIndex = 0;
        bool hasPrevious = false;
        bool changed = false;
    };

    // Borrowed data. The producer keeps it valid until frame recording ends.
    struct SceneFrameData
    {
        uint64_t revision = 0;
        const InstanceFrameData* instances = nullptr;
        size_t instanceCount = 0;
        bool transformsChanged = false;
        bool materialsChanged = false;
        bool lightsChanged = false;
        bool topologyChanged = false;
    };
}
