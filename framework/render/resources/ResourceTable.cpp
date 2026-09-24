#include "ResourceTable.h"

#include <cstdio>

namespace prism::gpu
{
    namespace
    {
        std::string DescribePixelFormat(PixelFormat format)
        {
            switch (format)
            {
            case PixelFormat::RGBA32_FLOAT:      return "RGBA32_FLOAT";
            case PixelFormat::RGBA16_FLOAT:      return "RGBA16_FLOAT";
            case PixelFormat::RG16_FLOAT:        return "RG16_FLOAT";
            case PixelFormat::R16_FLOAT:         return "R16_FLOAT";
            case PixelFormat::R32_FLOAT:         return "R32_FLOAT";
            case PixelFormat::RGBA8_UNORM:       return "RGBA8_UNORM";
            case PixelFormat::RGBA8_SNORM:       return "RGBA8_SNORM";
            case PixelFormat::RG16_UNORM:        return "RG16_UNORM";
            case PixelFormat::R8_UNORM:          return "R8_UNORM";
            case PixelFormat::D32_FLOAT:         return "D32_FLOAT";
            case PixelFormat::D24_UNORM_S8_UINT: return "D24_UNORM_S8_UINT";
            default:                             return "unknown";
            }
        }
    }

    std::vector<ResourceTable::EntryInfo> ResourceTable::GetEntries() const
    {
        std::vector<EntryInfo> infos;

        char detail[128] = {};

        for (const TextureCache::EntryInfo& texture : m_Textures.GetEntries())
        {
            snprintf(detail, sizeof(detail), "%ux%u %s", texture.size.width, texture.size.height,
                DescribePixelFormat(texture.format).c_str());

            infos.push_back(EntryInfo{ texture.name, "texture", detail });
        }

        for (const BufferCache::EntryInfo& buffer : m_Buffers.GetEntries())
        {
            snprintf(detail, sizeof(detail), "%llu bytes, stride %u",
                (unsigned long long)buffer.byteSize, buffer.structStride);

            infos.push_back(EntryInfo{ buffer.name, "buffer", detail });
        }

        return infos;
    }
}
