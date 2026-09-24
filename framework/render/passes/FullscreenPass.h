#pragma once
#include "RasterPass.h"
#include <donut/engine/CommonRenderPasses.h>

namespace prism::gpu
{
    class FullscreenPass
    {
    public:
        Status Initialize(nvrhi::IDevice* device, ShaderLibrary& shaders, donut::engine::CommonRenderPasses& common,
            ShaderEntry pixelShader, const nvrhi::BindingLayoutVector& layouts = {}, nvrhi::RenderState state = {});
        Status Record(nvrhi::ICommandList* commands, nvrhi::IFramebuffer* target,
            const nvrhi::BindingSetVector& bindings = {}, nvrhi::ViewportState viewport = {});
        nvrhi::BindingSetHandle Bindings(const nvrhi::BindingSetDesc& desc, nvrhi::IBindingLayout* layout)
        { return m_Raster.Bindings(desc, layout); }
        void ClearBindings() { m_Raster.ClearBindings(); }
    private:
        RasterPass m_Raster;
    };
}
