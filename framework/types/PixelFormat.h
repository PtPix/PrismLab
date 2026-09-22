#pragma once

// Framework types: texture formats without a graphics API.
//
// The NVRHI layer converts these to nvrhi::Format (see framework/nvrhi/Formats.h), so that a
// resource request can be described without naming a graphics API type. Only the formats that the
// experiment actually uses for scene data, debug output and readback are listed here.

#include <cstdint>

namespace prism
{
    enum class PixelFormat : uint32_t
    {
        Unknown = 0,

        // Color and data targets
        RGBA32_FLOAT,
        RGBA16_FLOAT,
        RG16_FLOAT,
        R16_FLOAT,
        R32_FLOAT,
        RGBA8_UNORM,
        RGBA8_SNORM,
        RG16_UNORM,
        R8_UNORM,

        // Depth
        D32_FLOAT,
        D24_UNORM_S8_UINT,

        Count
    };

    // Bytes per pixel for the formats above; 0 when the format has no plain linear layout.
    constexpr uint32_t GetBytesPerPixel(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::RGBA32_FLOAT:      return 16;
        case PixelFormat::RGBA16_FLOAT:      return 8;
        case PixelFormat::RG16_FLOAT:        return 4;
        case PixelFormat::R16_FLOAT:         return 2;
        case PixelFormat::R32_FLOAT:         return 4;
        case PixelFormat::RGBA8_UNORM:       return 4;
        case PixelFormat::RGBA8_SNORM:       return 4;
        case PixelFormat::RG16_UNORM:        return 4;
        case PixelFormat::R8_UNORM:          return 1;
        case PixelFormat::D32_FLOAT:         return 4;
        case PixelFormat::D24_UNORM_S8_UINT: return 4;
        default:                             return 0;
        }
    }

    // Number of color channels that carry meaningful data, for readback interpretation.
    constexpr uint32_t GetChannelCount(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::RGBA32_FLOAT:
        case PixelFormat::RGBA16_FLOAT:
        case PixelFormat::RGBA8_UNORM:
        case PixelFormat::RGBA8_SNORM:       return 4;
        case PixelFormat::RG16_FLOAT:
        case PixelFormat::RG16_UNORM:        return 2;
        case PixelFormat::R16_FLOAT:
        case PixelFormat::R32_FLOAT:
        case PixelFormat::R8_UNORM:
        case PixelFormat::D32_FLOAT:         return 1;
        default:                             return 0;
        }
    }

    // True when the format can be read as 32-bit floats (directly or by promoting halfs).
    constexpr bool IsFloatFormat(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::RGBA32_FLOAT:
        case PixelFormat::RGBA16_FLOAT:
        case PixelFormat::RG16_FLOAT:
        case PixelFormat::R16_FLOAT:
        case PixelFormat::R32_FLOAT:
        case PixelFormat::D32_FLOAT:         return true;
        default:                             return false;
        }
    }

    constexpr bool IsDepthFormat(PixelFormat format)
    {
        return format == PixelFormat::D32_FLOAT || format == PixelFormat::D24_UNORM_S8_UINT;
    }
}
