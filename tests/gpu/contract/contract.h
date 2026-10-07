#pragma once
#include <framework/adapters/donut/ForwardScene.h>
#include <framework/app/Params.h>
#include <framework/tools/inspection/DebugViewRegistry.h>
#include <framework/tools/metrics/Metrics.h>

// ContractExperiment: the M1 contract check.
//
// A GPU pass decodes the depth buffer and writes the reconstructed world position, the linear depth
// and the raw device depth. The CPU then verifies, on a sparse set of pixels:
//   * matrix convention: projecting the GPU's world position returns the same pixel
//   * depth convention: the GPU's linear depth matches the CPU's LinearizeDepth of the same depth
//   * reconstruction: the CPU's world position matches the GPU's
// plus pure CPU round trips for the matrices and the depth conversion.
//
// This is what a new data convention has to pass before an algorithm may rely on it, and it is the
// template for the project's verification story.
//
// It also demonstrates the parameter table: the settings below drive the ImGui panel, the JSON
// section (experiments.ContractExperiment) and the parameter hash without any per-parameter code.

#include <framework/app/Experiment.h>
#include <framework/render/passes/ComputePass.h>

#include <string>

namespace Prism::Experiments
{
	struct FContractVerificationReport
	{
		bool bRan = false;
		bool bPassed = false;

		uint32_t SampledPixels = 0;

		float MaxUvErrorPixels = 0.f;
		float MaxPositionErrorMeters = 0.f;
		float MaxLinearDepthErrorMeters = 0.f;
		float MaxMatrixRoundTripError = 0.f;
		float MaxDepthRoundTripError = 0.f;

		std::string Summary = "not run";
	};

	class FContractExperiment final : public Prism::Host::IExperiment
	{
	  public:
		// 参数结构体保持普通 POD：描述符表（kContractParams）负责 UI、JSON 与 hash。
		struct FSettings
		{
			int VerifyFrame = 3;   // 在第 N 帧执行校验（读第 N-1 帧写下的结果）
			int SampleStride = 32; // 采样步长（像素）
			float ToleranceMeters = 0.01f;
			float TolerancePixels = 0.05f;
		};

		[[nodiscard]] const char* GetName() const override
		{
			return "ContractExperiment";
		}
		[[nodiscard]] const char* GetDescription() const override;

		FStatus Initialize(Host::FExperimentContext& Context) override;
		void BeginFrame(Host::FExperimentContext& Context, const Host::FExperimentFrame& Frame) override;
		nvrhi::ITexture* Render(Host::FExperimentContext& Context, const Host::FExperimentFrame& Frame) override;
		void BuildUI(Host::FExperimentContext& Context) override;
		void OnResize(Host::FExperimentContext& Context, const FExtent2D& RenderSize,
					  const FExtent2D& OutputSize) override;

		[[nodiscard]] bool PassedVerification() const override
		{
			return !Report.bRan || Report.bPassed;
		}

	  private:
		Adapter::FForwardScene Scene;
		bool EnsureCheckPass(Host::FExperimentContext& Context, nvrhi::ITexture* Depth);
		void RunVerification(Host::FExperimentContext& Context);
		void VerifyCpuMath(const Prism::FCameraData& Camera);

		FSettings Settings;
		Host::FParamTable Params;
		FContractVerificationReport Report;

		Gpu::FTextureRequest ColorRequest;
		Gpu::FTextureRequest DepthRequest;
		Gpu::FTextureRequest PositionRequest;
		Gpu::FTextureRequest DepthCopyRequest;

		nvrhi::BufferHandle CheckConstantBuffer;
		nvrhi::BindingLayoutHandle CheckBindingLayout;
		nvrhi::BindingSetHandle CheckBindingSet;
		Gpu::FComputePass CheckPass;
		bool bCheckReady = false;

		nvrhi::ITexture* BoundDepth = nullptr;
		nvrhi::ITexture* BoundPositionTarget = nullptr;
		nvrhi::ITexture* BoundDepthTarget = nullptr;

		Prism::FCameraData VerifiedCamera;
		bool bHasVerifiedCamera = false;
		bool bVerificationRequested = false;
	};

	// 参数描述符：字段、JSON 键、UI 标签与范围写在一起，加参数不需要写额外代码。
	inline const Prism::Host::FParamDesc KContractParams[] = {
		PRISM_PARAM_INT(FContractExperiment::FSettings, VerifyFrame, "Verify frame", 1, 60,
						Prism::Host::EParamFlags::None),
		PRISM_PARAM_INT(FContractExperiment::FSettings, SampleStride, "Sample stride", 2, 128,
						Prism::Host::EParamFlags::None),
		PRISM_PARAM_FLOAT(FContractExperiment::FSettings, ToleranceMeters, "Tolerance (m)", 0.0001f, 0.5f,
						  Prism::Host::EParamFlags::None),
		PRISM_PARAM_FLOAT(FContractExperiment::FSettings, TolerancePixels, "Tolerance (px)", 0.001f, 2.f,
						  Prism::Host::EParamFlags::None),
	};
} // namespace Prism::Experiments
