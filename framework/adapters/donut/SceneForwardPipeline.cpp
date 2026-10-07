#include "SceneForwardPipeline.h"

#include <donut/core/log.h>
#include <donut/render/GeometryPasses.h>

namespace Prism::Pipeline
{
	bool FSceneForwardPipeline::Initialize(nvrhi::IDevice* Device,
										   const std::shared_ptr<donut::engine::ShaderFactory>& ShaderFactory)
	{
		if (!Device || !ShaderFactory)
		{
			donut::log::error("SceneForwardPipeline: device and shader factory are required.");
			return false;
		}

		auto CommonPasses = std::make_shared<donut::engine::CommonRenderPasses>(Device, ShaderFactory);

		ForwardPass = std::make_shared<donut::render::ForwardShadingPass>(Device, CommonPasses);
		ForwardPass->Init(*ShaderFactory, donut::render::ForwardShadingPass::CreateParameters());

		return true;
	}

	void FSceneForwardPipeline::PrepareLights(donut::render::ForwardShadingPass::Context& Context,
											  nvrhi::ICommandList* Commands, const donut::engine::SceneGraph& Graph,
											  const dm::float3& AmbientTop, const dm::float3& AmbientBottom)
	{
		if (!ForwardPass)
			return;

		ForwardPass->PrepareLights(Context, Commands, Graph.GetLights(), AmbientTop, AmbientBottom, {});
	}

	void FSceneForwardPipeline::RenderScene(nvrhi::ICommandList* Commands, const donut::engine::SceneGraph& Graph,
											const donut::engine::IView& View, const donut::engine::IView& PreviousView,
											nvrhi::IFramebuffer* Framebuffer, const dm::float3& AmbientTop,
											const dm::float3& AmbientBottom)
	{
		if (!ForwardPass || !Framebuffer)
			return;

		donut::render::ForwardShadingPass::Context Context;
		PrepareLights(Context, Commands, Graph, AmbientTop, AmbientBottom);

		DrawStrategy.PrepareForView(Graph.GetRootNode(), View);

		donut::render::RenderView(Commands, &View, &PreviousView, Framebuffer, DrawStrategy, *ForwardPass, Context);
	}
} // namespace Prism::Pipeline
