#pragma once
#include <framework/adapters/donut/ForwardScene.h>
#include <framework/app/Params.h>
#include <framework/tools/inspection/DebugViewRegistry.h>
#include <framework/tools/metrics/Metrics.h>

// ForwardExperiment: the baseline experiment.
//
// It renders the shared scene with the shared forward pipeline and adds one pass of its own (a debug
// view of the depth buffer). There is no camera, scene, UI shell, timing or configuration code here:
// the host provides all of it, which is what "only write the algorithm" means in practice.

#include <framework/app/Experiment.h>
#include <framework/render/passes/FullscreenPass.h>

namespace Prism::Experiments
{
	class FForwardExperiment final : public Prism::Host::IExperiment
	{
	  public:
		[[nodiscard]] const char* GetName() const override
		{
			return "ForwardExperiment";
		}
		[[nodiscard]] const char* GetDescription() const override;

		FStatus Initialize(Host::FExperimentContext& Context) override;
		nvrhi::ITexture* Render(Host::FExperimentContext& Context, const Host::FExperimentFrame& Frame) override;
		void BuildUI(Host::FExperimentContext& Context) override;
		void OnResize(Host::FExperimentContext& Context, const FExtent2D& RenderSize,
					  const FExtent2D& OutputSize) override;

	  private:
		Adapter::FForwardScene Scene;
		struct FSettings
		{
			int DebugMode = 0; // 0 = off
			float DepthScale = 1.f;
		};

		bool EnsureDebugPass(Host::FExperimentContext& Context, nvrhi::ITexture* Depth);

		FSettings Settings;

		Gpu::FTextureRequest ColorRequest;
		Gpu::FTextureRequest DepthRequest;
		Gpu::FTextureRequest DebugRequest;

		nvrhi::BufferHandle DebugConstantBuffer;
		nvrhi::BindingLayoutHandle DebugBindingLayout;
		nvrhi::BindingSetHandle DebugBindingSet;

		Gpu::FFullscreenPass DebugPass;
		bool bDebugReady = false;
		nvrhi::IFramebuffer* DebugFramebuffer = nullptr;
	};
} // namespace Prism::Experiments
