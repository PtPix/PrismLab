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

namespace prism::experiments
{
    struct ContractVerificationReport
    {
        bool ran = false;
        bool passed = false;

        uint32_t sampledPixels = 0;

        float maxUvErrorPixels = 0.f;
        float maxPositionErrorMeters = 0.f;
        float maxLinearDepthErrorMeters = 0.f;
        float maxMatrixRoundTripError = 0.f;
        float maxDepthRoundTripError = 0.f;

        std::string summary = "not run";
    };

    class ContractExperiment final : public prism::host::Experiment
    {
    public:
        // 参数结构体保持普通 POD：描述符表（kContractParams）负责 UI、JSON 与 hash。
        struct Settings
        {
            int verifyFrame = 3;        // 在第 N 帧执行校验（读第 N-1 帧写下的结果）
            int sampleStride = 32;      // 采样步长（像素）
            float toleranceMeters = 0.01f;
            float tolerancePixels = 0.05f;
        };

        [[nodiscard]] const char* GetName() const override { return "ContractExperiment"; }
        [[nodiscard]] const char* GetDescription() const override;

        Status Initialize(host::ExperimentContext& context) override;
        void BeginFrame(host::ExperimentContext& context, const host::ExperimentFrame& frame) override;
        nvrhi::ITexture* Render(host::ExperimentContext& context, const host::ExperimentFrame& frame) override;
        void BuildUI(host::ExperimentContext& context) override;
        void OnResize(host::ExperimentContext& context, const Extent2D& renderSize, const Extent2D& outputSize) override;

        [[nodiscard]] bool PassedVerification() const override { return !m_Report.ran || m_Report.passed; }

    private:
        adapter::ForwardScene m_Scene;
        bool EnsureCheckPass(host::ExperimentContext& context, nvrhi::ITexture* depth);
        void RunVerification(host::ExperimentContext& context);
        void VerifyCpuMath(const prism::CameraData& camera);

        Settings m_Settings;
        host::ParamTable m_Params;
        ContractVerificationReport m_Report;

        gpu::TextureRequest m_ColorRequest;
        gpu::TextureRequest m_DepthRequest;
        gpu::TextureRequest m_PositionRequest;
        gpu::TextureRequest m_DepthCopyRequest;

        nvrhi::BufferHandle m_CheckConstantBuffer;
        nvrhi::BindingLayoutHandle m_CheckBindingLayout;
        nvrhi::BindingSetHandle m_CheckBindingSet;
        gpu::ComputePass m_CheckPass;
        bool m_CheckReady = false;

        nvrhi::ITexture* m_BoundDepth = nullptr;
        nvrhi::ITexture* m_BoundPositionTarget = nullptr;
        nvrhi::ITexture* m_BoundDepthTarget = nullptr;

        prism::CameraData m_VerifiedCamera;
        bool m_HasVerifiedCamera = false;
        bool m_VerificationRequested = false;
    };

    // 参数描述符：字段、JSON 键、UI 标签与范围写在一起，加参数不需要写额外代码。
    inline const prism::host::ParamDesc kContractParams[] = {
        PRISM_PARAM_INT(ContractExperiment::Settings, verifyFrame, "Verify frame", 1, 60, prism::host::ParamFlags::None),
        PRISM_PARAM_INT(ContractExperiment::Settings, sampleStride, "Sample stride", 2, 128, prism::host::ParamFlags::None),
        PRISM_PARAM_FLOAT(ContractExperiment::Settings, toleranceMeters, "Tolerance (m)", 0.0001f, 0.5f, prism::host::ParamFlags::None),
        PRISM_PARAM_FLOAT(ContractExperiment::Settings, tolerancePixels, "Tolerance (px)", 0.001f, 2.f, prism::host::ParamFlags::None),
    };
}
