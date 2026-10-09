#include "Deferred.h"
#include "ProceduralScene.h"

#include <framework/tools/comparison/ComparisonController.h>
#include <framework/tools/inspection/DebugViewRegistry.h>

#include <donut/core/json.h>
#include <donut/core/log.h>
#include <donut/core/vfs/VFS.h>
#include <imgui.h>

#include <cstring>

namespace dm = donut::math;

namespace Prism::Samples
{
	FStatus FDeferredExperiment::CreateDepthRenderer(Gpu::FShaderLibrary& Shaders,
													 std::unique_ptr<Surface::FDepthRenderer>& OutRenderer)
	{
		auto Shader = Shaders.GetShader("prism/PrismSurface/SurfaceDepth.hlsl", "main_vs", nvrhi::ShaderType::Vertex);
		if (!Shader)
		{
			return FStatus::Error(EErrorCode::ShaderCompileFailed, Shaders.GetLastError());
		}
		auto Renderer = std::make_unique<Surface::FDepthRenderer>();
		const auto Result = Renderer->Initialize(DepthDevice, Shader);
		if (!Result)
		{
			return FStatus::Error(EErrorCode::PipelineCreationFailed, Result.Message);
		}
		OutRenderer = std::move(Renderer);
		return FStatus::Ok();
	}

	FStatus FDeferredExperiment::PrepareShaders(Gpu::FShaderLibrary& Candidate)
	{
		return CreateDepthRenderer(Candidate, CandidateDepthRenderer);
	}

	void FDeferredExperiment::CommitShaders()
	{
		DepthRenderer.swap(CandidateDepthRenderer);
		CandidateDepthRenderer.reset();
	}

	void FDeferredExperiment::DiscardShaders()
	{
		CandidateDepthRenderer.reset();
	}

	void FDeferredExperiment::Shutdown(Host::FExperimentContext&)
	{
		if (RegisteredShaders)
		{
			RegisteredShaders->Unregister(this);
			RegisteredShaders = nullptr;
		}
		CandidateDepthRenderer.reset();
		DepthRenderer.reset();
		Scene.Reset();
	}

	FStatus FDeferredExperiment::Initialize(Host::FExperimentContext& Context)
	{
		if (!Context.Gpu.Device || !Context.Gpu.Shaders || !Context.Gpu.Targets || !Context.Gpu.CommonPasses)
		{
			return FStatus::Error(EErrorCode::NotInitialized, "deferred sample needs GPU services");
		}

		// Get scene settings from host config, or use defaults if not provided.
		const Host::FHostConfig Defaults;
		const auto& Config = Context.Config ? *Context.Config : Defaults;
		if (Config.Scene.Source != "procedural" || !Config.Scene.Asset.empty())
		{
			return FStatus::Error(EErrorCode::Unsupported,
								  "the depth sample currently supports procedural geometry only");
		}

		// Load the scene
		const auto SceneStatus = Scene.Load(Context.Gpu.Device, Context.Gpu.Shaders->GetFactory(),
											std::make_shared<donut::vfs::NativeFileSystem>(), Config.Scene,
											Config.Lighting, &CreateProceduralScene);
		if (!SceneStatus)
		{
			return SceneStatus;
		}
		if (!Scene.GetData().Geometry.IsValid())
		{
			return FStatus::Error(EErrorCode::ResourceMissing, "the scene contains no drawable geometry");
		}

		Context.Scene.Stats = Scene.GetData().Stats;
		Context.Scene.Description = Scene.GetData().Description;

		if (Context.Config)
		{
			Json::Value Settings;
			if (Host::LoadExperimentSettings(*Context.Config, GetName(), Settings) && Settings["debugMode"].isInt())
			{
				DebugMode = Settings["debugMode"].asInt() == 1 ? 1 : 0;
			}
		}

		// Set depth texture request parameters
		DepthRequest.Name = "Deferred.Depth";
		DepthRequest.Format = EPixelFormat::D32Float;
		DepthRequest.Usage = Gpu::ETextureUsage::DepthStencil | Gpu::ETextureUsage::ShaderResource;
		DepthRequest.ClearDepth = GetDepthClearValue(Config.Camera.DepthConvention);

		// Set output color space to display-encoded for depth preview
		Context.Output.ColorSpace = EColorSpace::DisplayEncoded;

		DepthDevice = Context.Gpu.Device;
		const auto Status = CreateDepthRenderer(*Context.Gpu.Shaders, DepthRenderer);
		if (!Status)
		{
			return Status;
		}
		const auto PreviewStatus = Preview.Initialize(Context.Gpu);
		if (!PreviewStatus)
		{
			return PreviewStatus;
		}
		RegisteredShaders = Context.Gpu.Shaders;
		RegisteredShaders->Register(this);
		return FStatus::Ok();
	}

	nvrhi::ITexture* FDeferredExperiment::Render(Host::FExperimentContext& Context, const Host::FExperimentFrame& Frame)
	{
		// update depth clear value.
		const float ClearDepth = GetDepthClearValue(Frame.Camera.DepthConvention);
		if (DepthRequest.ClearDepth != ClearDepth)
		{
			DepthRequest.ClearDepth = ClearDepth;
			Preview.OnResize();
		}

		// Get or create the depth texture and framebuffer for this frame.
		auto& Targets = *Context.Gpu.Targets;
		nvrhi::ITexture* Depth = Targets.GetOrCreate(DepthRequest);
		nvrhi::IFramebuffer* DepthTarget = Depth ? Targets.GetFramebuffer(nullptr, Depth) : nullptr;
		if (!DepthTarget)
		{
			return nullptr;
		}

		// Update the scene.
		Scene.Update(Frame.Commands, uint32_t(Frame.Frame.SubmissionIndex));

		// Record the depth pass
		const auto& Geometry = Scene.GetData().Geometry;
		DepthBatch.BufferGroups.clear();
		DepthBatch.Draws.clear();
		DepthBatch.BufferGroups.reserve(Geometry.BufferGroups.size());
		DepthBatch.Draws.reserve(Geometry.Draws.size());
		for (const auto& Group : Geometry.BufferGroups)
		{
			DepthBatch.BufferGroups.push_back(
				{Group.VertexBuffer, Group.PositionRange, Group.IndexBuffer, Group.IndexFormat});
		}
		for (const auto& Draw : Geometry.Draws)
		{
			if (Draw.BaseVertex < 0)
			{
				donut::log::error("Deferred: negative base vertex is not supported by the depth batch");
				return nullptr;
			}
			Surface::FDepthDraw DepthDraw;
			DepthDraw.BufferGroupIndex = Draw.BufferGroupIndex;
			DepthDraw.FirstIndex = Draw.FirstIndex;
			DepthDraw.IndexCount = Draw.IndexCount;
			DepthDraw.BaseVertex = uint32_t(Draw.BaseVertex);
			const dm::float4x4 ObjectToClip =
				dm::affineToHomogeneous(Draw.ObjectToWorld) * Frame.Camera.Raster.WorldToClip;
			static_assert(sizeof(ObjectToClip) == sizeof(DepthDraw.ObjectToClip));
			std::memcpy(DepthDraw.ObjectToClip.data(), &ObjectToClip, sizeof(ObjectToClip));
			DepthBatch.Draws.push_back(DepthDraw);
		}

		const Surface::FDepthInputs DepthInputs{DepthBatch, DepthTarget};
		Surface::FDepthSettings DepthSettings;
		DepthSettings.bReverseZ = Frame.Camera.DepthConvention == EDepthConvention::ReversedZ0To1;
		Surface::FDepthOutputs DepthOutputs;
		const auto Status = DepthRenderer->Record(Frame.Commands, DepthInputs, DepthSettings, DepthOutputs);

		if (!Status)
		{
			donut::log::error("Deferred: depth pass failed: %s", Status.Message);
			return nullptr;
		}

		if (Context.Tools.DebugViews)
		{
			Context.Tools.DebugViews->Publish(GetName(), "Device depth", DepthOutputs.Depth);
		}
		nvrhi::ITexture* Output =
			Preview.Record(Context.Gpu, Frame.Commands, DepthOutputs.Depth, Frame.Camera, Frame.RenderSize, DebugMode);
		if (Output && Context.Tools.Comparison)
		{
			Context.Tools.Comparison->Publish("Depth preview", {Output, EColorSpace::DisplayEncoded});
		}
		return Output;
	}

	void FDeferredExperiment::BuildUI(Host::FExperimentContext&)
	{
		const char* Modes[] = {"Device depth", "Linear depth"};
		ImGui::Combo("Depth preview", &DebugMode, Modes, 2);
	}

	void FDeferredExperiment::OnResize(Host::FExperimentContext&, const FExtent2D&, const FExtent2D&)
	{
		Preview.OnResize();
	}
} // namespace Prism::Samples

namespace Prism::Host
{
	std::unique_ptr<IExperiment> CreateExperiment()
	{
		return std::make_unique<Samples::FDeferredExperiment>();
	}
} // namespace Prism::Host
