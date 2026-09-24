#pragma once

// Donut facilities: the procedural test scene.
//
// No external art assets are required, which matters for algorithm research: a scene must be ready
// immediately after a clean build. The vertex/index/instance layout matches what the glTF loader
// produces, so passes written against this scene keep working with loaded assets.

#include <framework/scene/ScenePreset.h>
#include "SceneData.h"

#include <nvrhi/nvrhi.h>

namespace prism::adapter
{
    // Must be called with an already open() command list: the first uploads of geometry,
    // material constants and instance data are all recorded into it.
    SceneData CreateProceduralScene(
        nvrhi::IDevice* device,
        nvrhi::ICommandList* commandList,
        const LightingPreset& lighting);
}
