#pragma once
#include <framework/render/shaders/ShaderLibrary.h>

namespace prism::gpu
{
    // Empty means reflection is unavailable; such shaders cannot be hot-reloaded safely.
    std::string ReflectShaderInterface(donut::engine::ShaderFactory& factory, const char* path,
        const char* entry, const ShaderMacroList& defines);
}
