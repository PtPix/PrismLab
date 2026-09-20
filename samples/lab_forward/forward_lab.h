#pragma once

// ForwardLab: the baseline experiment.
//
// It renders the shared scene with the shared forward pipeline and adds one pass of its own (a debug
// view of the depth buffer). There is no camera, scene, UI shell, timing or configuration code here:
// the host provides all of it, which is what "only write the algorithm" means in practice.

#include <host/Lab.h>

namespace renderlab::labs
{
    class ForwardLab final : public renderlab::host::Lab
    {
    public:
        [[nodiscard]] const char* GetName() const override { return "ForwardLab"; }
        [[nodiscard]] const char* GetDescription() const override;

        Status Initialize(host::LabContext& context) override;
        nvrhi::ITexture* Render(host::LabContext& context, const host::LabFrame& frame) override;
        void BuildUI(host::LabContext& context) override;
        void OnResize(host::LabContext& context, const Extent2D& renderSize, const Extent2D& outputSize) override;

    private:
        struct Settings
        {
            int debugMode = 0;      // 0 = off
            float depthScale = 1.f;
        };

        bool EnsureDebugPass(host::LabContext& context, nvrhi::ITexture* depth);

        Settings m_Settings;

        gpu::TextureRequest m_ColorRequest;
        gpu::TextureRequest m_DepthRequest;
        gpu::TextureRequest m_DebugRequest;

        nvrhi::BufferHandle m_DebugConstantBuffer;
        nvrhi::BindingLayoutHandle m_DebugBindingLayout;
        nvrhi::BindingSetHandle m_DebugBindingSet;

        nvrhi::GraphicsPipelineHandle m_DebugPipeline;
        nvrhi::IFramebuffer* m_DebugFramebuffer = nullptr;
        nvrhi::ITexture* m_DebugFramebufferTarget = nullptr;
        nvrhi::ITexture* m_BoundDepthTexture = nullptr;
    };
}
