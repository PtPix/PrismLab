#include "Formats.h"

namespace renderlab::gpu
{
    nvrhi::Format ToNvrhiFormat(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::RGBA32_FLOAT:      return nvrhi::Format::RGBA32_FLOAT;
        case PixelFormat::RGBA16_FLOAT:      return nvrhi::Format::RGBA16_FLOAT;
        case PixelFormat::RG16_FLOAT:        return nvrhi::Format::RG16_FLOAT;
        case PixelFormat::R16_FLOAT:         return nvrhi::Format::R16_FLOAT;
        case PixelFormat::R32_FLOAT:         return nvrhi::Format::R32_FLOAT;
        case PixelFormat::RGBA8_UNORM:       return nvrhi::Format::RGBA8_UNORM;
        case PixelFormat::RGBA8_SNORM:       return nvrhi::Format::RGBA8_SNORM;
        case PixelFormat::RG16_UNORM:        return nvrhi::Format::RG16_UNORM;
        case PixelFormat::R8_UNORM:          return nvrhi::Format::R8_UNORM;
        case PixelFormat::D32_FLOAT:         return nvrhi::Format::D32;
        case PixelFormat::D24_UNORM_S8_UINT: return nvrhi::Format::D24S8;
        default:                             return nvrhi::Format::UNKNOWN;
        }
    }

    PixelFormat FromNvrhiFormat(nvrhi::Format format)
    {
        switch (format)
        {
        case nvrhi::Format::RGBA32_FLOAT:      return PixelFormat::RGBA32_FLOAT;
        case nvrhi::Format::RGBA16_FLOAT:      return PixelFormat::RGBA16_FLOAT;
        case nvrhi::Format::RG16_FLOAT:        return PixelFormat::RG16_FLOAT;
        case nvrhi::Format::R16_FLOAT:         return PixelFormat::R16_FLOAT;
        case nvrhi::Format::R32_FLOAT:         return PixelFormat::R32_FLOAT;
        case nvrhi::Format::RGBA8_UNORM:       return PixelFormat::RGBA8_UNORM;
        case nvrhi::Format::RGBA8_SNORM:       return PixelFormat::RGBA8_SNORM;
        case nvrhi::Format::RG16_UNORM:        return PixelFormat::RG16_UNORM;
        case nvrhi::Format::R8_UNORM:          return PixelFormat::R8_UNORM;
        case nvrhi::Format::D32:               return PixelFormat::D32_FLOAT;
        case nvrhi::Format::D24S8:             return PixelFormat::D24_UNORM_S8_UINT;
        default:                               return PixelFormat::Unknown;
        }
    }

    bool IsUavCompatible(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::RGBA32_FLOAT:
        case PixelFormat::RGBA16_FLOAT:
        case PixelFormat::RG16_FLOAT:
        case PixelFormat::R16_FLOAT:
        case PixelFormat::R32_FLOAT:
        case PixelFormat::RGBA8_UNORM:
        case PixelFormat::R8_UNORM:
            return true;
        default:
            return false;
        }
    }

    nvrhi::ResourceStates GetInitialState(TextureUsage usage, PixelFormat format)
    {
        if (HasAny(usage, TextureUsage::DepthStencil) || IsDepthFormat(format))
            return nvrhi::ResourceStates::DepthWrite;

        if (HasAny(usage, TextureUsage::RenderTarget))
            return nvrhi::ResourceStates::RenderTarget;

        if (HasAny(usage, TextureUsage::UnorderedAccess))
            return nvrhi::ResourceStates::UnorderedAccess;

        return nvrhi::ResourceStates::Common;
    }

    const char* ToString(nvrhi::Format format)
    {
        switch (format)
        {
        case nvrhi::Format::RGBA32_FLOAT: return "RGBA32_FLOAT";
        case nvrhi::Format::RGBA16_FLOAT: return "RGBA16_FLOAT";
        case nvrhi::Format::RG16_FLOAT:   return "RG16_FLOAT";
        case nvrhi::Format::R16_FLOAT:    return "R16_FLOAT";
        case nvrhi::Format::R32_FLOAT:    return "R32_FLOAT";
        case nvrhi::Format::RGBA8_UNORM:  return "RGBA8_UNORM";
        case nvrhi::Format::RGBA8_SNORM:  return "RGBA8_SNORM";
        case nvrhi::Format::RG16_UNORM:   return "RG16_UNORM";
        case nvrhi::Format::R8_UNORM:     return "R8_UNORM";
        case nvrhi::Format::D32:          return "D32_FLOAT";
        case nvrhi::Format::UNKNOWN:      return "unknown";
        default:                          return "other";
        }
    }
}
