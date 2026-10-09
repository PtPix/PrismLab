#pragma once

// Sample-owned procedural scene, managed by the Donut scene host.
//
// No external art assets are required, which matters for algorithm research: a scene must be ready
// immediately after a clean build. The vertex/index/instance layout matches what the glTF loader
// produces, so passes written against this scene keep working with loaded assets.

#include <framework/scene/ScenePreset.h>
#include <framework/adapters/donut/SceneData.h>

#include <nvrhi/nvrhi.h>

namespace Prism::Samples
{
	// Must be called with an already open() command list: the first uploads of geometry,
	// material constants and instance data are all recorded into it.
	Adapter::FSceneData CreateProceduralScene(nvrhi::IDevice* Device, nvrhi::ICommandList* CommandList,
											  const FLightingPreset& Lighting);
} // namespace Prism::Samples
