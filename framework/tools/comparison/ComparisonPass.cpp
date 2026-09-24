#include "ComparisonPass.h"
#include <algorithm>
#include <cmath>

namespace prism::gpu
{
    namespace
    {
        bool Valid(nvrhi::ITexture* texture)
        {
            if (!texture) return false;
            const auto& d = texture->getDesc();
            return d.dimension == nvrhi::TextureDimension::Texture2D && d.sampleCount == 1 && d.isShaderResource;
        }
    }
    Status ComparisonPass::Initialize(nvrhi::IDevice* device, ShaderLibrary& shaders, donut::engine::CommonRenderPasses& common)
    {
        m_Device = device; m_Common = &common;
        m_BlitBindings = std::make_unique<donut::engine::BindingCache>(device);
        if (!m_Constants.Initialize(device, 16, "Comparison.Constants"))
            return Status::Error(ErrorCode::DeviceError, "comparison constants failed");
        nvrhi::BindingLayoutDesc layout; layout.visibility = nvrhi::ShaderType::Pixel;
        layout.bindings = {nvrhi::BindingLayoutItem::VolatileConstantBuffer(0), nvrhi::BindingLayoutItem::Texture_SRV(0), nvrhi::BindingLayoutItem::Texture_SRV(1)};
        m_Layout = device->createBindingLayout(layout);
        if (!m_Layout) return Status::Error(ErrorCode::DeviceError, "comparison layout failed");
        return m_Difference.Initialize(device, shaders, common, {"prism/PrismTools/Difference.hlsl", "main_ps", nvrhi::ShaderType::Pixel, {}}, {m_Layout});
    }
    Status ComparisonPass::Freeze(nvrhi::ICommandList* commands, ComparisonImage image)
    {
        if (!commands || !Valid(image.texture)) return Status::Error(ErrorCode::InvalidArgument, "freeze requires a 2D single-sample SRV");
        auto desc = image.texture->getDesc(); desc.debugName = "Comparison.Frozen";
        desc.mipLevels = 1; desc.isVirtual = false; desc.sharedResourceFlags = nvrhi::SharedResourceFlags::None;
        desc.initialState = nvrhi::ResourceStates::ShaderResource; desc.keepInitialState = true;
        auto frozen = m_Device->createTexture(desc);
        if (!frozen) return Status::Error(ErrorCode::DeviceError, "freeze allocation failed");
        commands->copyTexture(frozen, nvrhi::TextureSlice(), image.texture, nvrhi::TextureSlice());
        m_Frozen = frozen; m_FrozenSpace = image.colorSpace;
        m_BlitBindings->Clear(); m_Difference.ClearBindings();
        return Status::Ok();
    }
    Status ComparisonPass::Record(nvrhi::ICommandList* commands, ComparisonImage a, ComparisonImage b, const ComparisonSettings& settings)
    {
        if (!commands || !Valid(a.texture) || !Valid(b.texture) || !std::isfinite(settings.split) || !std::isfinite(settings.gain))
            return Status::Error(ErrorCode::InvalidArgument, "comparison requires two 2D single-sample SRVs and finite settings");
        const auto& da = a.texture->getDesc(); const auto& db = b.texture->getDesc();
        if (da.width != db.width || da.height != db.height)
            return Status::Error(ErrorCode::ExtentMismatch, "comparison inputs must have equal extents");
        if (a.colorSpace != b.colorSpace)
            return Status::Error(ErrorCode::FormatMismatch, "comparison inputs must use the same color space");
        if (a.texture == m_Output || b.texture == m_Output)
            return Status::Error(ErrorCode::InvalidArgument, "comparison output cannot be an input");
        if (!m_Output || m_Output->getDesc().width != da.width || m_Output->getDesc().height != da.height)
        {
            nvrhi::TextureDesc desc; desc.width = da.width; desc.height = da.height; desc.format = nvrhi::Format::RGBA16_FLOAT;
            desc.isRenderTarget = true; desc.initialState = nvrhi::ResourceStates::RenderTarget; desc.keepInitialState = true; desc.debugName = "Comparison.Output";
            m_Output = m_Device->createTexture(desc);
            if (!m_Output) return Status::Error(ErrorCode::DeviceError, "comparison output allocation failed");
            m_Framebuffer = m_Device->createFramebuffer(nvrhi::FramebufferDesc().addColorAttachment(m_Output));
            m_BlitBindings->Clear(); m_Difference.ClearBindings();
        }
        if (!m_Framebuffer) return Status::Error(ErrorCode::DeviceError, "comparison framebuffer failed");
        if (settings.mode == ComparisonMode::Difference)
        {
            struct Constants { float gain; float pad[3]; } constants{settings.gain, {0, 0, 0}};
            m_Constants.Write(commands, constants);
            nvrhi::BindingSetDesc bindings;
            bindings.bindings = {nvrhi::BindingSetItem::ConstantBuffer(0, m_Constants.Get()),
                nvrhi::BindingSetItem::Texture_SRV(0, a.texture), nvrhi::BindingSetItem::Texture_SRV(1, b.texture)};
            return m_Difference.Record(commands, m_Framebuffer, {m_Difference.Bindings(bindings, m_Layout)});
        }
        auto blit = [&](nvrhi::ITexture* texture, float left, float right, bool crop)
        {
            if (right <= left) return;
            donut::engine::BlitParameters p; p.sourceTexture = texture; p.targetFramebuffer = m_Framebuffer;
            p.targetBox = dm::box2(dm::float2(left, 0), dm::float2(right, 1));
            if (crop) p.sourceBox = p.targetBox;
            m_Common->BlitTexture(commands, p, m_BlitBindings.get());
        };
        const float split = std::clamp(settings.split, 0.f, 1.f);
        if (settings.mode == ComparisonMode::A || settings.mode == ComparisonMode::Off) blit(a.texture, 0, 1, false);
        else if (settings.mode == ComparisonMode::B) blit(b.texture, 0, 1, false);
        else { blit(a.texture, 0, split, settings.mode == ComparisonMode::Wipe); blit(b.texture, split, 1, settings.mode == ComparisonMode::Wipe); }
        return Status::Ok();
    }
}
