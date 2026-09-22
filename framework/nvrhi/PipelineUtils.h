#pragma once

// NVRHI layer: small helpers that remove repetitive pipeline / binding / draw plumbing.

#include <framework/types/Types.h>

#include <nvrhi/nvrhi.h>

#include <vector>

namespace prism::gpu
{
    nvrhi::BindingSetHandle CreateBindingSet(
        nvrhi::IDevice* device,
        nvrhi::IBindingLayout* layout,
        const nvrhi::BindingSetDesc& desc);

    // 同时创建 layout 与 set，供单 shader 阶段的实验 pass 使用。
    nvrhi::BindingSetHandle CreateBindingSetAndLayout(
        nvrhi::IDevice* device,
        nvrhi::ShaderType visibility,
        uint32_t registerSpace,
        const nvrhi::BindingSetDesc& desc,
        nvrhi::BindingLayoutHandle& layout);

    nvrhi::ComputePipelineHandle CreateComputePipeline(
        nvrhi::IDevice* device,
        nvrhi::IShader* computeShader,
        nvrhi::IBindingLayout* layout = nullptr);

    struct FullScreenPipelineDesc
    {
        nvrhi::IShader* vertexShader = nullptr;     // Donut 的 m_FullscreenVS：由 SV_VertexID 生成的四边形
        nvrhi::IShader* pixelShader = nullptr;
        nvrhi::IFramebuffer* framebuffer = nullptr;
        nvrhi::IBindingLayout* bindingLayout = nullptr;

        nvrhi::DepthStencilState depthStencil;
        nvrhi::BlendState blend;

        // Donut 的全屏顶点着色器输出 4 个顶点（SV_VertexID 0..3），所以是 TriangleStrip。
        // 用 TriangleList 只画 3 个顶点会得到一个半屏三角形——契约自检的图像验证抓得到这个错误。
        nvrhi::PrimitiveType primitiveType = nvrhi::PrimitiveType::TriangleStrip;

        // 全屏 pass 通常不需要写入深度；需要时由调用方设置。
        bool depthTestEnabled = false;
        bool depthWriteEnabled = false;
    };

    nvrhi::GraphicsPipelineHandle CreateFullScreenPipeline(nvrhi::IDevice* device, const FullScreenPipelineDesc& desc);

    // 绑定 pipeline / viewport / scissor 并绘制覆盖全屏的四边形（4 个顶点，顶点由 VS 生成）。
    void DrawFullScreenQuad(
        nvrhi::ICommandList* commands,
        nvrhi::IGraphicsPipeline* pipeline,
        nvrhi::IFramebuffer* framebuffer,
        nvrhi::IBindingSet* bindingSet = nullptr);

    void Dispatch(
        nvrhi::ICommandList* commands,
        nvrhi::IComputePipeline* pipeline,
        nvrhi::IBindingSet* bindingSet,
        dm::uint3 groups);

    // 从 framebuffer 派生全屏 viewport；自定义绘制几何的 Pass 用它填 GraphicsState::viewport。
    nvrhi::ViewportState MakeFullViewportState(nvrhi::IFramebuffer* framebuffer);
}
