#pragma once

// ForwardExperiment: the baseline experiment.
//
// It renders the shared scene with the shared forward pipeline and adds one pass of its own (a debug
// view of the depth buffer). There is no camera, scene, UI shell, timing or configuration code here:
// the host provides all of it, which is what "only write the algorithm" means in practice.

#include <framework/host/Experiment.h>

namespace prism::experiments
{
    class ForwardExperiment final : public prism::host::Experiment
    {
    public:
        [[nodiscard]] const char* GetName() const override { return "ForwardExperiment"; }
        [[nodiscard]] const char* GetDescription() const override;

        Status Initialize(host::ExperimentContext& context) override;
        nvrhi::ITexture* Render(host::ExperimentContext& context, const host::ExperimentFrame& frame) override;
        void BuildUI(host::ExperimentContext& context) override;
        void OnResize(host::ExperimentContext& context, const Extent2D& renderSize, const Extent2D& outputSize) override;

    private:
        struct Settings
        {
            int debugMode = 0;      // 0 = off
            float depthScale = 1.f;
        };

        bool EnsureDebugPass(host::ExperimentContext& context, nvrhi::ITexture* depth);

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
