#pragma once
#include <framework/render/resources/ResourceTable.h>
#include <framework/render/shaders/ShaderLibrary.h>
#include <framework/render/profiling/GpuProfiler.h>
#include <donut/engine/CommonRenderPasses.h>
namespace prism::gpu
{
    struct RenderServices
    {
        nvrhi::IDevice* device = nullptr;
        donut::engine::ShaderFactory* shaderFactory = nullptr;
        donut::engine::CommonRenderPasses* commonPasses = nullptr;
        ShaderLibrary* shaders = nullptr;
        TextureCache* targets = nullptr;
        BufferCache* buffers = nullptr;
        ResourceTable* resources = nullptr;
        GpuProfiler* profiler = nullptr;
    };
}
