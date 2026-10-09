#pragma once
#include <cstdint>

namespace Prism::Surface
{
    enum class EDepthCullMode : uint32_t
    {
        TwoSided = 0,
        FrontFacing,
        BackFacing,
        Count
    };

    struct FDepthSettings
    {
        bool bClearDepth = true;
        EDepthCullMode CullMode = EDepthCullMode::TwoSided;
    };
}