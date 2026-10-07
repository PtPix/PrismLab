#pragma once
#include "SceneHost.h"
#include "SceneForwardPipeline.h"
#include <framework/render/RenderServices.h>
namespace Prism::Adapter
{
	// Optional legacy reference path. Samples own and select this adapter explicitly.
	class FForwardScene
	{
	  public:
		FStatus Initialize(Gpu::FRenderServices& Gpu, const FScenePreset& ScenePreset,
						   const FLightingPreset& LightingPreset);
		void Record(nvrhi::ICommandList* Commands, uint64_t Submission, const donut::engine::IView& View,
					const donut::engine::IView& Previous, nvrhi::IFramebuffer* Target);
		const FSceneData& GetData() const
		{
			return Scene.GetData();
		}

	  private:
		FSceneHost Scene;
		Pipeline::FSceneForwardPipeline Pipeline;
		dm::float3 AmbientTop{}, AmbientBottom{};
	};
} // namespace Prism::Adapter
