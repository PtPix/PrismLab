#pragma once

// Pipelines layer: the shared "scene -> texture" draw path.
//
// An experiment that needs a rendered scene (shadow receivers, AO, SSR, tonemapping experiments) does not
// wire up geometry passes: it declares a render target and calls RenderScene. The pass split inside
// this pipeline is Donut's forward shading, which is the baseline the roadmap compares against.

#include <framework/core/Types.h>

#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>
#include <donut/render/DrawStrategy.h>
#include <donut/render/ForwardShadingPass.h>

#include <memory>

namespace Prism::Pipeline
{
	class FSceneForwardPipeline
	{
	  public:
		bool Initialize(nvrhi::IDevice* Device, const std::shared_ptr<donut::engine::ShaderFactory>& ShaderFactory);

		// 清理目标并按共享前向路径绘制不透明场景。
		// ambientTop/Bottom 来自宿主配置的 lighting 段，已经乘上环境强度。
		void RenderScene(nvrhi::ICommandList* Commands, const donut::engine::SceneGraph& Graph,
						 const donut::engine::IView& View, const donut::engine::IView& PreviousView,
						 nvrhi::IFramebuffer* Framebuffer, const dm::float3& AmbientTop,
						 const dm::float3& AmbientBottom);

		// 需要自己插 Pass 的实验：先准备光源，再自行调用 RenderView。
		void PrepareLights(donut::render::ForwardShadingPass::Context& Context, nvrhi::ICommandList* Commands,
						   const donut::engine::SceneGraph& Graph, const dm::float3& AmbientTop,
						   const dm::float3& AmbientBottom);

		[[nodiscard]] donut::render::ForwardShadingPass& GetForwardPass()
		{
			return *ForwardPass;
		}
		[[nodiscard]] donut::render::InstancedOpaqueDrawStrategy& GetDrawStrategy()
		{
			return DrawStrategy;
		}

	  private:
		std::shared_ptr<donut::render::ForwardShadingPass> ForwardPass;
		donut::render::InstancedOpaqueDrawStrategy DrawStrategy;
	};
} // namespace Prism::Pipeline
