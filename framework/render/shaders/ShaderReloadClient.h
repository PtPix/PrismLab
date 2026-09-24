#pragma once
#include <framework/core/Status.h>

namespace prism::gpu
{
    class ShaderLibrary;
    class ShaderReloadClient
    {
    public:
        virtual ~ShaderReloadClient() = default;
        virtual Status PrepareShaders(ShaderLibrary& candidate) = 0;
        virtual void CommitShaders() = 0;
        virtual void DiscardShaders() = 0;
    };
}
