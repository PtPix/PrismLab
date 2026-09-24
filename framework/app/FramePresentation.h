#pragma once
#include "framework/render/presentation/DisplayChain.h"
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/BindingCache.h>

namespace prism::host
{
    struct ExperimentContext;
    // Capturable final pixels. The display transform remains application supplied.
    class FramePresentation
    {
    public:
        Status Record(ExperimentContext& context, nvrhi::ICommandList* commands, nvrhi::IFramebuffer* swapchain,
            nvrhi::ITexture* scene, ColorSpace space, float delta, uint64_t frame);
        nvrhi::ITexture* Output() const { return m_Texture; }
        void Reset() { m_Framebuffer = nullptr; m_Texture = nullptr; if (m_Bindings) m_Bindings->Clear(); }
    private:
        nvrhi::TextureHandle m_Texture;
        nvrhi::FramebufferHandle m_Framebuffer;
        std::unique_ptr<donut::engine::BindingCache> m_Bindings;
    };
}
