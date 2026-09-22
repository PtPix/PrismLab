#include "PipelineUtils.h"

#include <donut/core/log.h>

#include <nvrhi/utils.h>

namespace prism::gpu
{
    nvrhi::BindingSetHandle CreateBindingSet(
        nvrhi::IDevice* device,
        nvrhi::IBindingLayout* layout,
        const nvrhi::BindingSetDesc& desc)
    {
        if (!device || !layout)
        {
            donut::log::error("CreateBindingSet: device and layout are required.");
            return nullptr;
        }

        nvrhi::BindingSetHandle bindingSet = device->createBindingSet(desc, layout);
        if (!bindingSet)
            donut::log::error("CreateBindingSet: failed to create a binding set.");

        return bindingSet;
    }

    nvrhi::BindingSetHandle CreateBindingSetAndLayout(
        nvrhi::IDevice* device,
        nvrhi::ShaderType visibility,
        uint32_t registerSpace,
        const nvrhi::BindingSetDesc& desc,
        nvrhi::BindingLayoutHandle& layout)
    {
        nvrhi::BindingLayoutHandle bindingLayout;
        nvrhi::BindingSetHandle bindingSet;

        if (!nvrhi::utils::CreateBindingSetAndLayout(device, visibility, registerSpace, desc, bindingLayout, bindingSet))
        {
            donut::log::error("CreateBindingSetAndLayout: failed to create a binding layout or set.");
            return nullptr;
        }

        layout = bindingLayout;
        return bindingSet;
    }

    nvrhi::ComputePipelineHandle CreateComputePipeline(
        nvrhi::IDevice* device,
        nvrhi::IShader* computeShader,
        nvrhi::IBindingLayout* layout)
    {
        if (!device || !computeShader)
        {
            donut::log::error("CreateComputePipeline: device and compute shader are required.");
            return nullptr;
        }

        nvrhi::ComputePipelineDesc desc;
        desc.setComputeShader(computeShader);

        if (layout)
            desc.addBindingLayout(layout);

        nvrhi::ComputePipelineHandle pipeline = device->createComputePipeline(desc);
        if (!pipeline)
            donut::log::error("CreateComputePipeline: failed to create a pipeline object.");

        return pipeline;
    }

    nvrhi::GraphicsPipelineHandle CreateFullScreenPipeline(nvrhi::IDevice* device, const FullScreenPipelineDesc& desc)
    {
        if (!device || !desc.vertexShader || !desc.pixelShader || !desc.framebuffer)
        {
            donut::log::error("CreateFullScreenPipeline: framebuffer and both shaders are required.");
            return nullptr;
        }

        nvrhi::DepthStencilState depthStencil = desc.depthStencil;
        depthStencil.setDepthTestEnable(desc.depthTestEnabled);
        depthStencil.setDepthWriteEnable(desc.depthWriteEnabled);
        depthStencil.setDepthFunc(nvrhi::ComparisonFunc::Less);

        nvrhi::RenderState renderState;
        renderState.setBlendState(desc.blend);
        renderState.setDepthStencilState(depthStencil);

        nvrhi::GraphicsPipelineDesc pipelineDesc;
        pipelineDesc.setPrimType(desc.primitiveType);
        pipelineDesc.setVertexShader(desc.vertexShader);
        pipelineDesc.setPixelShader(desc.pixelShader);
        pipelineDesc.setRenderState(renderState);

        if (desc.bindingLayout)
            pipelineDesc.addBindingLayout(desc.bindingLayout);

        const nvrhi::FramebufferInfo framebufferInfo(desc.framebuffer->getFramebufferInfo());

        nvrhi::GraphicsPipelineHandle pipeline = device->createGraphicsPipeline(pipelineDesc, framebufferInfo);
        if (!pipeline)
            donut::log::error("CreateFullScreenPipeline: failed to create a pipeline object.");

        return pipeline;
    }

    void DrawFullScreenQuad(
        nvrhi::ICommandList* commands,
        nvrhi::IGraphicsPipeline* pipeline,
        nvrhi::IFramebuffer* framebuffer,
        nvrhi::IBindingSet* bindingSet)
    {
        if (!commands || !pipeline || !framebuffer)
            return;

        nvrhi::GraphicsState state;
        state.pipeline = pipeline;
        state.framebuffer = framebuffer;
        state.viewport.addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());

        if (bindingSet)
            state.bindings.push_back(bindingSet);

        commands->setGraphicsState(state);
        commands->draw(nvrhi::DrawArguments().setVertexCount(4));
    }

    void Dispatch(
        nvrhi::ICommandList* commands,
        nvrhi::IComputePipeline* pipeline,
        nvrhi::IBindingSet* bindingSet,
        dm::uint3 groups)
    {
        if (!commands || !pipeline)
            return;

        nvrhi::ComputeState state;
        state.pipeline = pipeline;

        if (bindingSet)
            state.bindings.push_back(bindingSet);

        commands->setComputeState(state);
        commands->dispatch(groups.x, groups.y, groups.z);
    }

    nvrhi::ViewportState MakeFullViewportState(nvrhi::IFramebuffer* framebuffer)
    {
        nvrhi::ViewportState state;

        if (framebuffer)
            state.addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());

        return state;
    }
}
