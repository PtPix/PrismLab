#pragma once

// Framework types: 颜色当前所处的处理阶段。
//
// 路线图要求每个 Pass 标注它的输入位于哪个阶段：把已经做过显示变换的颜色当作线性辐射信号
// 是后处理与光照类实验最常见的错误来源。显示链只接受 SceneLinear 输入。

#include <cstdint>

namespace renderlab
{
    enum class ColorSpace : uint32_t
    {
        SceneLinear = 0,    // 场景线性 HDR 辐射；不做 gamma，不预曝光（默认）
        PreExposed,         // 已乘过曝光，仍是线性
        DisplayEncoded,     // 已做显示变换与编码，仅可显示或做屏幕后处理
        Count
    };

    inline const char* ToString(ColorSpace colorSpace)
    {
        switch (colorSpace)
        {
        case ColorSpace::SceneLinear:    return "scene linear (HDR)";
        case ColorSpace::PreExposed:     return "pre-exposed linear";
        case ColorSpace::DisplayEncoded: return "display encoded";
        default:                         return "unknown";
        }
    }
}
