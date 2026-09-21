#include "DebugView.h"

#include "PipelineUtils.h"

#include <donut/core/log.h>
#include <donut/core/math/math.h>
#include <nvrhi/utils.h>

namespace dm = donut::math;
using namespace donut::math;

// 共享常量布局：需要 donut 数学类型在全局可见（与 Donut 自己的 shared header 用法一致）
#include "RenderLab/Debug/DebugView_cb.h"

static_assert(sizeof(DebugViewConstants) == 32, "DebugViewConstants layout changed; update DebugView.hlsl");

namespace renderlab::gpu
{
    bool DebugViewPass::Initialize(nvrhi::IDevice* device, ShaderLibrary& shaders)
    {
        if (!device)
            return false;

        m_Device = device;
        m_Shaders = &shaders;

        m_VertexShader = shaders.GetShader("renderlab/DebugView.hlsl", "main_vs", nvrhi::ShaderType::Vertex);
        m_PixelShader = shaders.GetShader("renderlab/DebugView.hlsl", "main_ps", nvrhi::ShaderType::Pixel);

        if (!m_VertexShader || !m_PixelShader)
            return false;

        m_ConstantBuffer = device->createBuffer(
            nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(DebugViewConstants), "RenderLab.DebugView", 8));

        m_PointClampSampler = device->createSampler(
            nvrhi::SamplerDesc()
                .setAllFilters(false)
                .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp));

        return m_ConstantBuffer != nullptr && m_PointClampSampler != nullptr;
    }

    bool DebugViewPass::EnsurePipeline(nvrhi::ITexture* source, nvrhi::IFramebuffer* target)
    {
        if (!m_Device || !source || !target)
            return false;

        if (!m_BindingSet || m_BoundSource != source)
        {
            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer),
                nvrhi::BindingSetItem::Texture_SRV(0, source),
                nvrhi::BindingSetItem::Sampler(0, m_PointClampSampler),
            };

            if (!nvrhi::utils::CreateBindingSetAndLayout(
                    m_Device, nvrhi::ShaderType::Pixel, 0, bindingSetDesc, m_BindingLayout, m_BindingSet))
            {
                return false;
            }

            m_BoundSource = source;
        }

        if (!m_Pipeline || m_BoundTarget != target)
        {
            m_BoundTarget = target;

            FullScreenPipelineDesc pipelineDesc;
            pipelineDesc.vertexShader = m_VertexShader;
            pipelineDesc.pixelShader = m_PixelShader;
            pipelineDesc.framebuffer = target;
            pipelineDesc.bindingLayout = m_BindingLayout;

            m_Pipeline = CreateFullScreenPipeline(m_Device, pipelineDesc);
            if (!m_Pipeline)
                return false;
        }

        return true;
    }

    bool DebugViewPass::Render(
        nvrhi::ICommandList* commands,
        nvrhi::ITexture* source,
        nvrhi::IFramebuffer* target,
        const DebugViewSettings& settings)
    {
        if (!commands)
            return false;

        if (!EnsurePipeline(source, target))
            return false;

        const nvrhi::TextureDesc& sourceDesc = source->getDesc();
        if (sourceDesc.width == 0 || sourceDesc.height == 0)
            return false;

        DebugViewConstants constants = {};
        constants.inverseSize = dm::float2(1.f / float(sourceDesc.width), 1.f / float(sourceDesc.height));
        constants.mode = int(settings.mode);
        constants.scale = settings.scale;
        constants.bias = settings.bias;

        commands->writeBuffer(m_ConstantBuffer, &constants, sizeof(constants));

        DrawFullScreenQuad(commands, m_Pipeline, target, m_BindingSet);
        return true;
    }
}
