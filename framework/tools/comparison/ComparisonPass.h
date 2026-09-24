#pragma once
#include <framework/render/passes/FullscreenPass.h>
#include <framework/render/data/ColorSpace.h>

namespace prism::gpu
{
    enum class ComparisonMode { Off, A, B, SideBySide, Wipe, Difference };
    struct ComparisonImage { nvrhi::ITexture* texture = nullptr; ColorSpace colorSpace = ColorSpace::SceneLinear; };
    struct ComparisonSettings { ComparisonMode mode = ComparisonMode::Off; float split = 0.5f; float gain = 1.f; };

    class ComparisonPass
    {
    public:
        Status Initialize(nvrhi::IDevice* device, ShaderLibrary& shaders, donut::engine::CommonRenderPasses& common);
        Status Record(nvrhi::ICommandList* commands, ComparisonImage a, ComparisonImage b, const ComparisonSettings& settings);
        Status Freeze(nvrhi::ICommandList* commands, ComparisonImage image);
        void ClearFrozen() { m_Frozen = nullptr; }
        ComparisonImage Frozen() const { return {m_Frozen, m_FrozenSpace}; }
        nvrhi::ITexture* Output() const { return m_Output; }
    private:
        nvrhi::IDevice* m_Device = nullptr;
        donut::engine::CommonRenderPasses* m_Common = nullptr;
        std::unique_ptr<donut::engine::BindingCache> m_BlitBindings;
        FullscreenPass m_Difference;
        PassConstants m_Constants;
        nvrhi::BindingLayoutHandle m_Layout;
        nvrhi::TextureHandle m_Output, m_Frozen;
        nvrhi::FramebufferHandle m_Framebuffer;
        ColorSpace m_FrozenSpace = ColorSpace::SceneLinear;
    };
}
