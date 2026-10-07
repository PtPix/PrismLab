#pragma once
#include <framework/render/shaders/ShaderLibrary.h>

namespace Prism::Gpu
{
	// Empty means reflection is unavailable; such shaders cannot be hot-reloaded safely.
	std::string ReflectShaderInterface(donut::engine::ShaderFactory& Factory, const char* Path, const char* Entry,
									   const FShaderMacroList& Defines);
} // namespace Prism::Gpu
