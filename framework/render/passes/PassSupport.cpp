#include "PassSupport.h"

namespace prism::gpu
{
    ShaderPass::~ShaderPass() { if (m_Shaders) m_Shaders->Unregister(this); }
    void ShaderPass::Attach(nvrhi::IDevice* device, ShaderLibrary& shaders)
    {
        if (m_Shaders) m_Shaders->Unregister(this);
        m_Device = device;
        m_Shaders = &shaders;
        m_Bindings = std::make_unique<donut::engine::BindingCache>(device);
        shaders.Register(this);
    }
    nvrhi::BindingSetHandle ShaderPass::Bindings(const nvrhi::BindingSetDesc& desc, nvrhi::IBindingLayout* layout)
    { return m_Bindings->GetOrCreateBindingSet(desc, layout); }
    void ShaderPass::ClearBindings() { if (m_Bindings) m_Bindings->Clear(); }
}
