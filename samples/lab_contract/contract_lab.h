#pragma once

// ContractLab: the M1 contract check.
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
// section (labs.ContractLab) and the parameter hash without any per-parameter code.

#include <framework/host/Lab.h>

#include <string>

namespace renderlab::labs
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

    class ContractLab final : public renderlab::host::Lab
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

        [[nodiscard]] const char* GetName() const override { return "ContractLab"; }
        [[nodiscard]] const char* GetDescription() const override;

        Status Initialize(host::LabContext& context) override;
        void BeginFrame(host::LabContext& context, const host::LabFrame& frame) override;
        nvrhi::ITexture* Render(host::LabContext& context, const host::LabFrame& frame) override;
        void BuildUI(host::LabContext& context) override;
        void OnResize(host::LabContext& context, const Extent2D& renderSize, const Extent2D& outputSize) override;

        [[nodiscard]] bool PassedVerification() const override { return !m_Report.ran || m_Report.passed; }

    private:
        bool EnsureCheckPass(host::LabContext& context, nvrhi::ITexture* depth);
        void RunVerification(host::LabContext& context);
        void VerifyCpuMath(const renderlab::CameraData& camera);

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
        nvrhi::ComputePipelineHandle m_CheckPipeline;

        nvrhi::ITexture* m_BoundDepth = nullptr;
        nvrhi::ITexture* m_BoundPositionTarget = nullptr;
        nvrhi::ITexture* m_BoundDepthTarget = nullptr;

        renderlab::CameraData m_VerifiedCamera;
        bool m_HasVerifiedCamera = false;
        bool m_VerificationRequested = false;
    };

    // 参数描述符：字段、JSON 键、UI 标签与范围写在一起，加参数不需要写额外代码。
    inline const renderlab::host::ParamDesc kContractParams[] = {
        RL_PARAM_INT(ContractLab::Settings, verifyFrame, "Verify frame", 1, 60, renderlab::host::ParamFlags::None),
        RL_PARAM_INT(ContractLab::Settings, sampleStride, "Sample stride", 2, 128, renderlab::host::ParamFlags::None),
        RL_PARAM_FLOAT(ContractLab::Settings, toleranceMeters, "Tolerance (m)", 0.0001f, 0.5f, renderlab::host::ParamFlags::None),
        RL_PARAM_FLOAT(ContractLab::Settings, tolerancePixels, "Tolerance (px)", 0.001f, 2.f, renderlab::host::ParamFlags::None),
    };
}
