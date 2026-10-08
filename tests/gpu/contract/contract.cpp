#include "contract.h"

#include <framework/tools/capture/TextureReadback.h>

#include <donut/core/json.h>
#include <donut/core/log.h>
#include <nvrhi/utils.h>

#include <imgui.h>

#include <cmath>

namespace dm = donut::math;
using namespace donut::math;

// 共享常量布局：需要 donut 的数学类型在全局可见（与 Donut 自己的 shared header 用法一致）
#include "contract_check_cb.h"

static_assert(sizeof(FContractCheckConstants) == 96,
			  "ContractCheckConstants layout changed; update contract_check.hlsl");

namespace Prism::Experiments
{
	namespace
	{
		constexpr uint32_t KThreadGroupSize = 8;
	} // namespace

	const char* FContractExperiment::GetDescription() const
	{
		return "Verifies the data contracts end to end: matrix convention, depth convention and CPU/GPU "
			   "reconstruction agreement, with JPEG-free numeric readback of the reconstructed data.";
	}

	FStatus FContractExperiment::Initialize(Host::FExperimentContext& Context)
	{
		if (!Context.Gpu.Device || !Context.Gpu.Targets || !Context.Gpu.Shaders)
			return FStatus::Error(EErrorCode::NotInitialized, "the host context is incomplete");

		const Host::FHostConfig Defaults;
		const auto& Config = Context.Config ? *Context.Config : Defaults;
		const FStatus SceneStatus = Scene.Initialize(Context.Gpu, Config.Scene, Config.Lighting);
		if (!SceneStatus)
			return SceneStatus;
		Context.Scene.Stats = Scene.GetData().Stats;
		Context.Scene.Description = Scene.GetData().Description;

		// 参数表：JSON 读取 + UI + hash 都由描述符驱动，加参数不需要写这里的代码。
		Params = Host::FParamTable(KContractParams);
		Params.Bind(&Settings);

		if (Context.Config)
		{
			Json::Value JsonSettings;
			if (Host::LoadExperimentSettings(*Context.Config, GetName(), JsonSettings))
				Params.LoadJson(JsonSettings);
		}

		Params.ClearEdited();

		ColorRequest.Name = "SceneColor";
		ColorRequest.Format = EPixelFormat::RgbA16Float;
		ColorRequest.Usage = Gpu::ETextureUsage::ShaderResource | Gpu::ETextureUsage::RenderTarget;
		ColorRequest.ClearColor = dm::float4(0.04f, 0.05f, 0.07f, 1.f);

		DepthRequest.Name = "SceneDepth";
		DepthRequest.Format = EPixelFormat::D32Float;
		DepthRequest.Usage = Gpu::ETextureUsage::ShaderResource | Gpu::ETextureUsage::DepthStencil;
		DepthRequest.ClearDepth = GetDepthClearValue(Config.Camera.DepthConvention);

		PositionRequest.Name = "ContractPosition";
		PositionRequest.Format = EPixelFormat::RgbA32Float;
		PositionRequest.Usage = Gpu::ETextureUsage::ShaderResource | Gpu::ETextureUsage::UnorderedAccess;
		PositionRequest.bHasClearValue = false;

		DepthCopyRequest.Name = "ContractDeviceDepth";
		DepthCopyRequest.Format = EPixelFormat::R32Float;
		DepthCopyRequest.Usage = Gpu::ETextureUsage::ShaderResource | Gpu::ETextureUsage::UnorderedAccess;
		DepthCopyRequest.bHasClearValue = false;

		if (!Context.Gpu.Targets->GetOrCreate(ColorRequest) || !Context.Gpu.Targets->GetOrCreate(DepthRequest))
			return FStatus::Error(EErrorCode::DeviceError, "failed to create the scene render targets");

		CheckConstantBuffer = Context.Gpu.Device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(
			sizeof(FContractCheckConstants), "ContractExperimentCheck", 4));

		if (!CheckConstantBuffer)
			return FStatus::Error(EErrorCode::DeviceError, "failed to create the contract check constant buffer");

		donut::log::info("ContractExperiment: ready (verification at frame %d, sample stride %d).",
						 Settings.VerifyFrame, Settings.SampleStride);

		return FStatus::Ok();
	}

	bool FContractExperiment::EnsureCheckPass(Host::FExperimentContext& Context, nvrhi::ITexture* Depth)
	{
		nvrhi::ITexture* PositionTarget = Context.Gpu.Targets->GetOrCreate(PositionRequest);
		nvrhi::ITexture* DepthTarget = Context.Gpu.Targets->GetOrCreate(DepthCopyRequest);

		if (!PositionTarget || !DepthTarget)
			return false;

		if (!CheckBindingSet || BoundDepth != Depth || BoundPositionTarget != PositionTarget ||
			BoundDepthTarget != DepthTarget)
		{
			nvrhi::BindingSetDesc BindingSetDesc;
			BindingSetDesc.bindings = {
				nvrhi::BindingSetItem::ConstantBuffer(0, CheckConstantBuffer),
				nvrhi::BindingSetItem::Texture_SRV(0, Depth),
				nvrhi::BindingSetItem::Texture_UAV(0, PositionTarget),
				nvrhi::BindingSetItem::Texture_UAV(1, DepthTarget),
			};

			if (!CheckBindingLayout &&
				!nvrhi::utils::CreateBindingSetAndLayout(Context.Gpu.Device, nvrhi::ShaderType::Compute, 0,
														 BindingSetDesc, CheckBindingLayout, CheckBindingSet))
			{
				return false;
			}

			CheckBindingSet = Context.Gpu.Device->createBindingSet(BindingSetDesc, CheckBindingLayout);
			BoundDepth = Depth;
			BoundPositionTarget = PositionTarget;
			BoundDepthTarget = DepthTarget;
		}

		if (!bCheckReady)
		{
			const FStatus Status = CheckPass.Initialize(
				Context.Gpu.Device, *Context.Gpu.Shaders,
				{"prism/PrismContract/contract_check.hlsl", "main_cs", nvrhi::ShaderType::Compute, {}},
				{CheckBindingLayout});
			if (!Status)
				return false;
			bCheckReady = true;
		}

		return true;
	}

	void FContractExperiment::BeginFrame(Host::FExperimentContext& Context, const Host::FExperimentFrame& Frame)
	{
		if (!bHasVerifiedCamera)
			return;

		const bool bScheduled =
			!Report.bRan && Frame.Frame.FrameIndex >= uint64_t(std::max(Settings.VerifyFrame, 1)) + 1;

		if (!bScheduled && !bVerificationRequested)
			return;

		bVerificationRequested = false;
		RunVerification(Context);
	}

	nvrhi::ITexture* FContractExperiment::Render(Host::FExperimentContext& Context, const Host::FExperimentFrame& Frame)
	{
		Gpu::FTextureCache& Targets = *Context.Gpu.Targets;
		if (DepthRequest.ClearDepth != GetDepthClearValue(Frame.Camera.DepthConvention))
		{
			DepthRequest.ClearDepth = GetDepthClearValue(Frame.Camera.DepthConvention);
			CheckBindingSet = nullptr;
			BoundDepth = nullptr;
			Report = FContractVerificationReport{};
		}

		nvrhi::ITexture* Color = Targets.GetOrCreate(ColorRequest);
		nvrhi::ITexture* Depth = Targets.GetOrCreate(DepthRequest);

		if (!Color || !Depth)
			return nullptr;

		nvrhi::ICommandList* Commands = Frame.Commands;
		const nvrhi::TextureSubresourceSet Subresources(0, 1, 0, 1);

		Commands->clearTextureFloat(Color, Subresources,
									nvrhi::Color(ColorRequest.ClearColor.x, ColorRequest.ClearColor.y,
												 ColorRequest.ClearColor.z, ColorRequest.ClearColor.w));
		Commands->clearDepthStencilTexture(Depth, Subresources, true, DepthRequest.ClearDepth, false, 0);

		if (Scene.GetData().Graph)
		{
			Gpu::FScopedGpuScope Scope(*Context.Gpu.Profiler, Commands, "Forward scene");

			Scene.Record(Commands, Frame.Frame.SubmissionIndex, *Frame.View, *Frame.PreviousView,
						 Targets.GetFramebuffer(Color, Depth));
		}

		if (!EnsureCheckPass(Context, Depth))
			return Color;

		// 中间结果发布给宿主面板：任何一张都可以被公共调试视图显示。
		if (Context.Tools.DebugViews)
		{
			Context.Tools.DebugViews->Publish(GetName(), "Scene color", Color);
			const bool bReverseZ = Frame.Camera.DepthConvention == EDepthConvention::ReversedZ0To1;
			Context.Tools.DebugViews->Publish(GetName(), "Scene depth (near bright)", Depth,
											  {bReverseZ ? Gpu::EDebugViewMode::R : Gpu::EDebugViewMode::OneMinusR,
											   20.f, 0.f});
			Context.Tools.DebugViews->Publish(GetName(), "Reconstructed world position",
											  Context.Gpu.Targets->Find(PositionRequest.Id));
			Context.Tools.DebugViews->Publish(GetName(), "Linear depth (meters)",
											  Context.Gpu.Targets->Find(PositionRequest.Id),
											  {Gpu::EDebugViewMode::A, 0.1f, 0.f});
			Context.Tools.DebugViews->Publish(GetName(), "Device depth copy",
											  Context.Gpu.Targets->Find(DepthCopyRequest.Id));
		}

		FContractCheckConstants Constants = {};
		Constants.ClipToWorld = Frame.Camera.Raster.ClipToWorld;
		Constants.InverseSize = dm::float2(1.f / float(Frame.RenderSize.Width), 1.f / float(Frame.RenderSize.Height));
		Constants.Size = dm::uint2(Frame.RenderSize.Width, Frame.RenderSize.Height);
		Constants.ZNear = Frame.Camera.ZNearMeters;
		Constants.ZFar = Frame.Camera.ZFarMeters;
		Constants.DepthConvention = int(Frame.Camera.DepthConvention);

		Commands->writeBuffer(CheckConstantBuffer, &Constants, sizeof(Constants));

		FStatus CheckStatus;
		{
			Gpu::FScopedGpuScope Scope(*Context.Gpu.Profiler, Commands, "Contract check");

			const dm::uint3 Groups((Frame.RenderSize.Width + KThreadGroupSize - 1) / KThreadGroupSize,
								   (Frame.RenderSize.Height + KThreadGroupSize - 1) / KThreadGroupSize, 1);

			CheckStatus = CheckPass.Dispatch(Commands, {CheckBindingSet}, Groups);
		}

		// 派发失败时不要标记"可校验"：否则下一帧会读到上一帧或未初始化的内容并给出无意义的结论。
		if (!CheckStatus)
		{
			donut::log::error("ContractExperiment: the check dispatch failed: %s",
							  CheckStatus.ToStringWithCode().c_str());
			Report = FContractVerificationReport{};
			Report.bRan = true;
			Report.Summary = "contract dispatch failed";
			return Color;
		}

		// 下一帧的 BeginFrame 会用这些数据做校验。
		VerifiedCamera = Frame.Camera;
		bHasVerifiedCamera = true;

		return Color;
	}

	void FContractExperiment::VerifyCpuMath(const Prism::FCameraData& Camera)
	{
		// 矩阵往返：world -> clip -> world
		const dm::float3 Probes[] = {
			dm::float3(0.f, 0.f, 0.f),
			dm::float3(1.f, 2.f, -3.f),
			Camera.Position + Camera.Forward * 5.f,
			Camera.Position + Camera.Right * 2.f + Camera.Up * 1.f + Camera.Forward * 8.f,
		};

		for (const dm::float3& Probe : Probes)
		{
			const dm::float4 Clip = dm::float4(Probe, 1.f) * Camera.Current.WorldToClip;
			const dm::float4 Back = Clip * Camera.Current.ClipToWorld;
			if (std::fabs(Back.w) < 1e-8f)
				continue;

			const dm::float3 Reconstructed = dm::float3(Back.x, Back.y, Back.z) / Back.w;
			Report.MaxMatrixRoundTripError =
				std::max(Report.MaxMatrixRoundTripError, dm::length(Reconstructed - Probe));
		}

		// 深度往返：device depth -> linear depth -> device depth
		for (int Step = 0; Step <= 9; ++Step)
		{
			const float DeviceDepth = float(Step) / 10.f;
			const float LinearDepth =
				Prism::LinearizeDepth(DeviceDepth, Camera.ZNearMeters, Camera.ZFarMeters, Camera.DepthConvention);
			const float RoundTrip = Prism::DeviceDepthFromLinear(LinearDepth, Camera.ZNearMeters, Camera.ZFarMeters,
																 Camera.DepthConvention);

			Report.MaxDepthRoundTripError = std::max(Report.MaxDepthRoundTripError, std::fabs(RoundTrip - DeviceDepth));
		}
	}

	void FContractExperiment::RunVerification(Host::FExperimentContext& Context)
	{
		const Prism::FCameraData& Camera = VerifiedCamera;

		Report = FContractVerificationReport{};
		VerifyCpuMath(Camera);

		nvrhi::ITexture* PositionTarget = Context.Gpu.Targets->Find(PositionRequest.Id);
		nvrhi::ITexture* DepthTarget = Context.Gpu.Targets->Find(DepthCopyRequest.Id);

		if (!PositionTarget || !DepthTarget)
		{
			Report.bRan = true;
			Report.Summary = "the contract targets are missing";
			donut::log::error("ContractExperiment: %s", Report.Summary.c_str());
			return;
		}

		const TResult<Gpu::FTextureData> Positions =
			Gpu::ReadTexture(Context.Gpu.Device, PositionTarget, EPixelFormat::RgbA32Float);
		const TResult<Gpu::FTextureData> Depths =
			Gpu::ReadTexture(Context.Gpu.Device, DepthTarget, EPixelFormat::R32Float);

		if (!Positions.IsOk() || !Depths.IsOk())
		{
			Report.bRan = true;
			Report.Summary = "texture readback failed";
			donut::log::error("ContractExperiment: %s", Report.Summary.c_str());
			return;
		}

		const Gpu::FTextureData& PositionData = Positions.GetValue();
		const Gpu::FTextureData& DepthData = Depths.GetValue();

		const FExtent2D Size = PositionData.Size;
		const uint32_t Stride = uint32_t(std::max(Settings.SampleStride, 1));

		for (uint32_t Y = 0; Y < Size.Height; Y += Stride)
		{
			for (uint32_t X = 0; X < Size.Width; X += Stride)
			{
				const float DeviceDepth = DepthData.FloatAt(X, Y);

				// 背景像素（清空值）不参与比较
				if (IsBackgroundDepth(DeviceDepth, Camera.DepthConvention))
					continue;

				const dm::float4 GpuSample = PositionData.ColorAt(X, Y);
				const dm::float3 GpuWorld = dm::float3(GpuSample.x, GpuSample.y, GpuSample.z);
				const float GpuLinearDepth = GpuSample.w;

				const dm::float2 Uv =
					dm::float2((float(X) + 0.5f) / float(Size.Width), (float(Y) + 0.5f) / float(Size.Height));

				// 1) 用实际光栅矩阵回投 GPU 世界位置，应当落回同一个像素
				const dm::float2 ProjectedUv = Camera.WorldToRasterUv(GpuWorld);
				const dm::float2 UvError =
					dm::float2((ProjectedUv.x - Uv.x) * float(Size.Width), (ProjectedUv.y - Uv.y) * float(Size.Height));
				Report.MaxUvErrorPixels = std::max(Report.MaxUvErrorPixels, dm::length(UvError));

				// 2) 深度约定：GPU 的线性深度应当等于 CPU 用同一个设备深度算出的线性深度
				const float CpuLinearDepth = Camera.LinearizeDepth(DeviceDepth);
				Report.MaxLinearDepthErrorMeters =
					std::max(Report.MaxLinearDepthErrorMeters, std::fabs(CpuLinearDepth - GpuLinearDepth));

				// 3) 重建一致性：CPU 与 GPU 从同一个设备深度重建的世界位置应当一致
				const dm::float3 CpuWorld = Camera.ReconstructRasterWorldPosition(Uv, DeviceDepth);
				Report.MaxPositionErrorMeters =
					std::max(Report.MaxPositionErrorMeters, dm::length(CpuWorld - GpuWorld));

				++Report.SampledPixels;
			}
		}

		Report.bRan = true;
		Report.bPassed = Report.SampledPixels > 0 && Report.MaxUvErrorPixels <= Settings.TolerancePixels &&
						 Report.MaxPositionErrorMeters <= Settings.ToleranceMeters &&
						 Report.MaxLinearDepthErrorMeters <= Settings.ToleranceMeters &&
						 Report.MaxMatrixRoundTripError <= 1e-3f && Report.MaxDepthRoundTripError <= 1e-5f;

		Report.Summary = Report.bPassed ? "passed" : "failed";

		donut::log::info("ContractExperiment: verification %s -- samples %u, uv error %.4f px, position error %.5f m, "
						 "linear depth error %.6f m, matrix round trip %.6f, depth round trip %.3e",
						 Report.Summary.c_str(), Report.SampledPixels, Report.MaxUvErrorPixels,
						 Report.MaxPositionErrorMeters, Report.MaxLinearDepthErrorMeters,
						 Report.MaxMatrixRoundTripError, Report.MaxDepthRoundTripError);

		if (!Report.bPassed && Report.SampledPixels > 0)
		{
			// 不静默通过：约定不一致必须让实验失败。
			donut::log::error("ContractExperiment: the data contracts are not consistent.");
		}
	}

	void FContractExperiment::BuildUI(Host::FExperimentContext& Context)
	{
		ImGui::TextWrapped("A GPU pass reconstructs world position and linear depth from the depth buffer; "
						   "the CPU verifies matrix and depth conventions against its own math.");

		if (ImGui::Button("Verify now"))
			bVerificationRequested = true;

		ImGui::SameLine();
		ImGui::Text("Samples: %u", Report.SampledPixels);

		if (Report.bRan)
		{
			const ImVec4 Color = Report.bPassed ? ImVec4(0.4f, 0.9f, 0.4f, 1.f) : ImVec4(0.95f, 0.4f, 0.35f, 1.f);
			ImGui::TextColored(Color, "Result: %s", Report.bPassed ? "PASSED" : "FAILED");
			ImGui::Text("Max uv error: %.4f px (limit %.3f)", Report.MaxUvErrorPixels, Settings.TolerancePixels);
			ImGui::Text("Max position error: %.5f m (limit %.4f)", Report.MaxPositionErrorMeters,
						Settings.ToleranceMeters);
			ImGui::Text("Max linear depth error: %.6f m", Report.MaxLinearDepthErrorMeters);
			ImGui::Text("Matrix round trip: %.6f", Report.MaxMatrixRoundTripError);
			ImGui::Text("Depth round trip: %.3e", Report.MaxDepthRoundTripError);
		}
		else
		{
			ImGui::TextUnformatted("Result: not run");
		}

		ImGui::SeparatorText("Parameters");
		Params.BuildUI();

		if (ImGui::Button("Save reconstructed data (PNG)"))
		{
			if (nvrhi::ITexture* Target = Context.Gpu.Targets->Find(PositionRequest.Id))
			{
				const std::filesystem::path Path =
					std::filesystem::path(Context.AssetsDirectory).empty()
						? std::filesystem::path("contract_position.png")
						: Context.AssetsDirectory.parent_path() / "contract_position.png";

				Context.Callbacks.SaveTexture(Target, Path, nvrhi::ResourceStates::UnorderedAccess);
			}
		}

		ImGui::SameLine();
		ImGui::TextDisabled("(needs GPU idle, off the frame path)");
	}

	void FContractExperiment::OnResize(Host::FExperimentContext& Context, const FExtent2D& RenderSize,
									   const FExtent2D& OutputSize)
	{
		(void)Context;
		(void)RenderSize;
		(void)OutputSize;

		CheckBindingSet = nullptr;
		CheckPass.ClearBindings();
		BoundDepth = nullptr;
		BoundPositionTarget = nullptr;
		BoundDepthTarget = nullptr;
	}
} // namespace Prism::Experiments

std::unique_ptr<Prism::Host::IExperiment> Prism::Host::CreateExperiment()
{
	return std::make_unique<Prism::Experiments::FContractExperiment>();
}
