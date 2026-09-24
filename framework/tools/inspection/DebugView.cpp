#include "DebugView.h"
using namespace donut::math;
#include "shaders/DebugView_cb.h"
static_assert(sizeof(DebugViewConstants) == 32);
namespace prism::gpu
{
    bool DebugViewPass::Initialize(nvrhi::IDevice* device, ShaderLibrary& shaders)
    {
        if (!device || !m_Constants.Initialize(device, sizeof(DebugViewConstants), "DebugView.Constants")) return false;
        m_PointClampSampler = device->createSampler(nvrhi::SamplerDesc().setAllFilters(false).setAllAddressModes(nvrhi::SamplerAddressMode::Clamp));
        nvrhi::BindingLayoutDesc layout; layout.visibility = nvrhi::ShaderType::Pixel;
        layout.bindings = {nvrhi::BindingLayoutItem::VolatileConstantBuffer(0), nvrhi::BindingLayoutItem::Texture_SRV(0), nvrhi::BindingLayoutItem::Sampler(0)};
        m_Layout = device->createBindingLayout(layout);
        nvrhi::GraphicsPipelineDesc desc; desc.bindingLayouts = {m_Layout};
        desc.primType = nvrhi::PrimitiveType::TriangleStrip;
        desc.renderState.depthStencilState.setDepthTestEnable(false).setDepthWriteEnable(false);
        auto status = m_Pass.Initialize(device, shaders, desc, {
            {"prism/PrismTools/DebugView.hlsl", "main_vs", nvrhi::ShaderType::Vertex, {}},
            {"prism/PrismTools/DebugView.hlsl", "main_ps", nvrhi::ShaderType::Pixel, {}}});
        return m_Ready = m_PointClampSampler && m_Layout && bool(status);
    }
    bool DebugViewPass::Render(nvrhi::ICommandList* commands, nvrhi::ITexture* source,
        nvrhi::IFramebuffer* target, const DebugViewSettings& settings)
    {
        if (!m_Ready || !commands || !source || !target) return false;
        const auto& d = source->getDesc();
        DebugViewConstants constants{};
        constants.inverseSize = dm::float2(1.f / d.width, 1.f / d.height);
        constants.mode = int(settings.mode); constants.scale = settings.scale; constants.bias = settings.bias;
        m_Constants.Write(commands, constants);
        nvrhi::BindingSetDesc bindings;
        bindings.bindings = {nvrhi::BindingSetItem::ConstantBuffer(0, m_Constants.Get()),
            nvrhi::BindingSetItem::Texture_SRV(0, source), nvrhi::BindingSetItem::Sampler(0, m_PointClampSampler)};
        nvrhi::GraphicsState state; state.framebuffer = target;
        state.bindings = {m_Pass.Bindings(bindings, m_Layout)};
        return bool(m_Pass.Draw(commands, state, nvrhi::DrawArguments().setVertexCount(4)));
    }
}
