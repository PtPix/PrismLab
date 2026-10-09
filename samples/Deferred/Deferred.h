#pragma once

#include <algorithms/Surface/DepthRenderer.h>
#include <framework/adapters/donut/SceneHost.h>
#include <framework/app/Experiment.h>
#include <framework/render/shaders/ShaderReloadClient.h>
#include "DepthPreview.h"

namespace Prism::Samples
{
	class FDeferredExperiment final : public Host::IExperiment, public Gpu::IShaderReloadClient
	{
	  public:
		const char* GetName() const override
		{
			return "DeferredExperiment";
		}
		const char* GetDescription() const override
		{
			return "Reusable Surface depth pass with sample-owned scene and depth visualization.";
		}

		FStatus Initialize(Host::FExperimentContext& Context) override;
		nvrhi::ITexture* Render(Host::FExperimentContext& Context, const Host::FExperimentFrame& Frame) override;
		void BuildUI(Host::FExperimentContext& Context) override;

		void OnResize(Host::FExperimentContext& Context, const FExtent2D& RenderSize,
					  const FExtent2D& OutputSize) override;
		void Shutdown(Host::FExperimentContext& Context) override;

		FStatus PrepareShaders(Gpu::FShaderLibrary& Candidate) override;
		void CommitShaders() override;
		void DiscardShaders() override;

	  private:
		FStatus CreateDepthRenderer(Gpu::FShaderLibrary& Shaders,
									std::unique_ptr<Surface::FDepthRenderer>& OutRenderer);

		Adapter::FSceneHost Scene;
		std::unique_ptr<Surface::FDepthRenderer> DepthRenderer;
		std::unique_ptr<Surface::FDepthRenderer> CandidateDepthRenderer;
		nvrhi::IDevice* DepthDevice = nullptr;
		Gpu::FShaderLibrary* RegisteredShaders = nullptr;
		Surface::FDepthBatch DepthBatch;
		FDepthPreview Preview;
		Gpu::FTextureRequest DepthRequest;
		int DebugMode = 1;
	};
} // namespace Prism::Samples
