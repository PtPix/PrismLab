#pragma once
#include <framework/render/resources/ResourceTable.h>
#include <framework/render/shaders/ShaderLibrary.h>
#include <framework/render/profiling/GpuProfiler.h>
#include <donut/engine/CommonRenderPasses.h>
namespace Prism::Gpu
{
	struct FRenderServices
	{
		nvrhi::IDevice* Device = nullptr;
		donut::engine::ShaderFactory* ShaderFactory = nullptr;
		donut::engine::CommonRenderPasses* CommonPasses = nullptr;
		FShaderLibrary* Shaders = nullptr;
		FTextureCache* Targets = nullptr;
		FBufferCache* Buffers = nullptr;
		FResourceTable* Resources = nullptr;
		FGpuProfiler* Profiler = nullptr;
	};
} // namespace Prism::Gpu
