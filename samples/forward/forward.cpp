#include "forward.h"

#include <framework/tools/replay/ReplayController.h>
#include <framework/tools/comparison/ComparisonController.h>

#include <donut/core/json.h>
#include <donut/core/log.h>
#include <nvrhi/utils.h>

#include <imgui.h>

#include <array>

namespace dm = donut::math;
using donut::math::float2;
using donut::math::float4x4;

// 共享常量布局：需要 donut 的数学类型在全局可见（与 Donut 自己的 shared header 用法一致）
#include "debug_view_cb.h"

static_assert(sizeof(FDebugViewConstants) == 96, "DebugViewConstants layout changed; update debug_view.hlsl");

namespace Prism::Experiments
{
	namespace
	{
		const char* const KDebugModeNames[] = {
			"Off", "Device depth", "Linear depth", "World position", "Normal from depth",
		};
	} // namespace

	const char* FForwardExperiment::GetDescription() const
	{
		return "Shared forward scene path plus a debug view pass written by the experiment (depth "
			   "decode, world position reconstruction and depth-derived normals).";
	}

	FStatus FForwardExperiment::Initialize(Host::FExperimentContext& Context)
	{
		if (!Context.Gpu.Device || !Context.Gpu.Targets || !Context.Gpu.Shaders)
		{
			return FStatus::Error(EErrorCode::NotInitialized, "the host context is incomplete");
		}

		const Host::FHostConfig Defaults;
		const auto& Config = Context.Config ? *Context.Config : Defaults;
		const FStatus SceneStatus = Scene.Initialize(Context.Gpu, Config.Scene, Config.Lighting);
		if (!SceneStatus)
		{
			return SceneStatus;
		}
		Context.Scene.Stats = Scene.GetData().Stats;
		Context.Scene.Description = Scene.GetData().Description;

		// Read this experiment's settings from presets/default.json (experiments.ForwardExperiment).
		if (Context.Config)
		{
			Json::Value JsonSettings;
			if (Host::LoadExperimentSettings(*Context.Config, GetName(), JsonSettings))
			{
				JsonSettings["debugMode"] >> Settings.DebugMode;
				JsonSettings["depthScale"] >> Settings.DepthScale;
			}
		}

		ColorRequest.Name = "SceneColor";
		ColorRequest.Format = EPixelFormat::RgbA16Float;
		ColorRequest.Usage = Gpu::ETextureUsage::ShaderResource | Gpu::ETextureUsage::RenderTarget;
		ColorRequest.ClearColor = dm::float4(0.04f, 0.05f, 0.07f, 1.f);

		DepthRequest.Name = "SceneDepth";
		DepthRequest.Format = EPixelFormat::D32Float;
		DepthRequest.Usage = Gpu::ETextureUsage::ShaderResource | Gpu::ETextureUsage::DepthStencil;
		DepthRequest.ClearDepth = GetDepthClearValue(Config.Camera.DepthConvention);

		DebugRequest.Name = "DebugColor";
		DebugRequest.Format = EPixelFormat::RgbA16Float;
		DebugRequest.Usage = Gpu::ETextureUsage::ShaderResource | Gpu::ETextureUsage::RenderTarget;
		DebugRequest.ClearColor = dm::float4(0.02f, 0.02f, 0.03f, 1.f);

		if (!Context.Gpu.Targets->GetOrCreate(ColorRequest) || !Context.Gpu.Targets->GetOrCreate(DepthRequest))
		{
			return FStatus::Error(EErrorCode::DeviceError, "failed to create the scene render targets");
		}

		DebugConstantBuffer = Context.Gpu.Device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(
			sizeof(FDebugViewConstants), "ForwardExperimentDebugView", 4));

		if (!DebugConstantBuffer)
		{
			return FStatus::Error(EErrorCode::DeviceError, "failed to create the debug view constant buffer");
		}

		Context.Tools.Replay->CaptureParameters = [this]()
		{
			Json::Value P;
			P["debugMode"] = Settings.DebugMode;
			P["depthScale"] = Settings.DepthScale;
			return P;
		};
		Context.Tools.Replay->RestoreParameters = [this](const Json::Value& P)
		{
			if (P["debugMode"].isInt())
			{
				Settings.DebugMode = P["debugMode"].asInt();
			}
			if (P["depthScale"].isNumeric())
			{
				Settings.DepthScale = P["depthScale"].asFloat();
			}
		};

		donut::log::info("ForwardExperiment: ready (debug mode %d).", Settings.DebugMode);
		return FStatus::Ok();
	}

	bool FForwardExperiment::EnsureDebugPass(Host::FExperimentContext& Context, nvrhi::ITexture* Depth)
	{
		if (!bDebugReady)
		{
			nvrhi::BindingLayoutDesc LayoutDescription;
			LayoutDescription.visibility = nvrhi::ShaderType::Pixel;
			LayoutDescription.bindings = {nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
										  nvrhi::BindingLayoutItem::Texture_SRV(0),
										  nvrhi::BindingLayoutItem::Sampler(0)};
			DebugBindingLayout = Context.Gpu.Device->createBindingLayout(LayoutDescription);
			if (!DebugBindingLayout)
			{
				return false;
			}
			const FStatus Status = DebugPass.Initialize(
				Context.Gpu.Device, *Context.Gpu.Shaders, *Context.Gpu.CommonPasses,
				{"prism/PrismForward/debug_view.hlsl", "main_ps", nvrhi::ShaderType::Pixel, {}}, {DebugBindingLayout});
			if (!Status)
			{
				return false;
			}
			bDebugReady = true;
		}
		nvrhi::BindingSetDesc Bindings;
		Bindings.bindings = {nvrhi::BindingSetItem::ConstantBuffer(0, DebugConstantBuffer),
							 nvrhi::BindingSetItem::Texture_SRV(0, Depth),
							 nvrhi::BindingSetItem::Sampler(0, Context.Gpu.CommonPasses->m_PointClampSampler)};
		DebugBindingSet = DebugPass.GetOrCreateBindingSet(Bindings, DebugBindingLayout);
		return true;
	}

	nvrhi::ITexture* FForwardExperiment::Render(Host::FExperimentContext& Context, const Host::FExperimentFrame& Frame)
	{
		Gpu::FTextureCache& Targets = *Context.Gpu.Targets;
		if (DepthRequest.ClearDepth != GetDepthClearValue(Frame.Camera.DepthConvention))
		{
			DepthRequest.ClearDepth = GetDepthClearValue(Frame.Camera.DepthConvention);
			DebugPass.ClearBindings();
			DebugBindingSet = nullptr;
			DebugFramebuffer = nullptr;
		}

		nvrhi::ITexture* Color = Targets.GetOrCreate(ColorRequest);
		nvrhi::ITexture* Depth = Targets.GetOrCreate(DepthRequest);

		if (!Color || !Depth)
		{
			return nullptr;
		}

		nvrhi::ICommandList* Commands = Frame.Commands;
		const nvrhi::TextureSubresourceSet Subresources(0, 1, 0, 1);

		Commands->clearTextureFloat(Color, Subresources,
									nvrhi::Color(ColorRequest.ClearColor.x, ColorRequest.ClearColor.y,
												 ColorRequest.ClearColor.z, ColorRequest.ClearColor.w));
		Commands->clearDepthStencilTexture(Depth, Subresources, true, DepthRequest.ClearDepth, false, 0);

		if (!Scene.GetData().Graph)
		{
			return Color;
		}

		{
			Gpu::FScopedGpuScope Scope(*Context.Gpu.Profiler, Commands, "Forward scene");

			Scene.Record(Commands, Frame.Frame.SubmissionIndex, *Frame.View, *Frame.PreviousView,
						 Targets.GetFramebuffer(Color, Depth));
		}

		// 中间结果发布给宿主面板（顺序每帧固定）
		if (Context.Tools.DebugViews)
		{
			Context.Tools.DebugViews->Publish(GetName(), "Scene color", Color);
			const bool bReverseZ = Frame.Camera.DepthConvention == EDepthConvention::ReversedZ0To1;
			Context.Tools.DebugViews->Publish(
				GetName(), "Scene depth (near bright)", Depth,
				{bReverseZ ? Gpu::EDebugViewMode::R : Gpu::EDebugViewMode::OneMinusR, 20.f, 0.f});
		}

		if (Context.Tools.Comparison)
		{
			Context.Tools.Comparison->Publish("Scene color", {Color, EColorSpace::SceneLinear});
		}
		if (Settings.DebugMode <= 0)
		{
			return Color;
		}

		nvrhi::ITexture* DebugTarget = Targets.GetOrCreate(DebugRequest);
		if (!DebugTarget)
		{
			return Color;
		}

		DebugFramebuffer = Targets.GetFramebuffer(DebugTarget, nullptr);
		if (!DebugFramebuffer)
		{
			return Color;
		}

		if (!EnsureDebugPass(Context, Depth))
		{
			return Color;
		}

		FDebugViewConstants Constants = {};
		Constants.ClipToWorld = Frame.Camera.Raster.ClipToWorld;
		Constants.InverseSize = dm::float2(1.f / float(Frame.RenderSize.Width), 1.f / float(Frame.RenderSize.Height));
		Constants.ZNear = Frame.Camera.ZNearMeters;
		Constants.ZFar = Frame.Camera.ZFarMeters;
		Constants.Mode = Settings.DebugMode - 1;
		Constants.DepthConvention = int(Frame.Camera.DepthConvention);
		Constants.DepthScale = Settings.DepthScale;

		Commands->writeBuffer(DebugConstantBuffer, &Constants, sizeof(Constants));

		FStatus DebugStatus;
		{
			Gpu::FScopedGpuScope Scope(*Context.Gpu.Profiler, Commands, "Debug view");

			Commands->clearTextureFloat(DebugTarget, Subresources,
										nvrhi::Color(DebugRequest.ClearColor.x, DebugRequest.ClearColor.y,
													 DebugRequest.ClearColor.z, DebugRequest.ClearColor.w));

			DebugStatus = DebugPass.Record(Commands, DebugFramebuffer, {DebugBindingSet});
		}

		// 调试 Pass 失败时回退到场景颜色，而不是显示一张只被清空过的目标。
		if (!DebugStatus)
		{
			donut::log::error("ForwardExperiment: the debug view pass failed: %s",
							  DebugStatus.ToStringWithCode().c_str());
			return Color;
		}

		return DebugTarget;
	}

	void FForwardExperiment::BuildUI(Host::FExperimentContext& Context)
	{
		(void)Context;

		ImGui::Text("Passes: forward scene (shared) + debug view (this experiment)");

		int DebugMode = Settings.DebugMode;
		if (ImGui::Combo("Debug view", &DebugMode, KDebugModeNames, int(std::size(KDebugModeNames))))
		{
			Settings.DebugMode = DebugMode;
		}

		if (Settings.DebugMode > 0)
		{
			ImGui::SliderFloat("Depth scale", &Settings.DepthScale, 0.01f, 4.f);
		}
	}

	void FForwardExperiment::OnResize(Host::FExperimentContext& Context, const FExtent2D& RenderSize,
									  const FExtent2D& OutputSize)
	{
		(void)Context;
		(void)RenderSize;
		(void)OutputSize;

		// 池里的纹理会被重建：这些缓存引用必须失效。
		DebugPass.ClearBindings();
		DebugFramebuffer = nullptr;
		DebugBindingSet = nullptr;
	}
} // namespace Prism::Experiments

std::unique_ptr<Prism::Host::IExperiment> Prism::Host::CreateExperiment()
{
	return std::make_unique<Prism::Experiments::FForwardExperiment>();
}
