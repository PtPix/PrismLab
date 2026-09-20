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
// template for the verification story of docs/architecture.md (section 10.1).

#include <host/Lab.h>

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
        [[nodiscard]] const char* GetName() const override { return "ContractLab"; }
        [[nodiscard]] const char* GetDescription() const override;

        Status Initialize(host::LabContext& context) override;
        void BeginFrame(host::LabContext& context, const host::LabFrame& frame) override;
        nvrhi::ITexture* Render(host::LabContext& context, const host::LabFrame& frame) override;
        void BuildUI(host::LabContext& context) override;
        void OnResize(host::LabContext& context, const Extent2D& renderSize, const Extent2D& outputSize) override;

        [[nodiscard]] bool PassedVerification() const override { return !m_Report.ran || m_Report.passed; }

    private:
        struct Settings
        {
            uint32_t verifyFrame = 3;       // 在第 N 帧执行校验（读第 N-1 帧写下的结果）
            uint32_t sampleStride = 32;     // 采样步长（像素）
            float toleranceMeters = 0.01f;
            float tolerancePixels = 0.05f;
        };

        bool EnsureCheckPass(host::LabContext& context, nvrhi::ITexture* depth);
        void RunVerification(host::LabContext& context);
        void VerifyCpuMath(const renderlab::CameraData& camera);

        Settings m_Settings;
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
}
