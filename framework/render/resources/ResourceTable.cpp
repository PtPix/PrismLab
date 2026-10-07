#include "ResourceTable.h"

#include <cstdio>

namespace Prism::Gpu
{
	namespace
	{
		std::string DescribePixelFormat(EPixelFormat Format)
		{
			switch (Format)
			{
				case EPixelFormat::RgbA32Float:
					return "RGBA32_FLOAT";
				case EPixelFormat::RgbA16Float:
					return "RGBA16_FLOAT";
				case EPixelFormat::RG16Float:
					return "RG16_FLOAT";
				case EPixelFormat::R16Float:
					return "R16_FLOAT";
				case EPixelFormat::R32Float:
					return "R32_FLOAT";
				case EPixelFormat::RgbA8Unorm:
					return "RGBA8_UNORM";
				case EPixelFormat::RgbA8Snorm:
					return "RGBA8_SNORM";
				case EPixelFormat::RG16Unorm:
					return "RG16_UNORM";
				case EPixelFormat::R8Unorm:
					return "R8_UNORM";
				case EPixelFormat::D32Float:
					return "D32_FLOAT";
				case EPixelFormat::D24UnormS8Uint:
					return "D24_UNORM_S8_UINT";
				default:
					return "unknown";
			}
		}
	} // namespace

	std::vector<FResourceTable::FEntryInfo> FResourceTable::GetEntries() const
	{
		std::vector<FEntryInfo> Infos;

		char Detail[128] = {};

		for (const FTextureCache::FEntryInfo& Texture : TextureCache.GetEntries())
		{
			snprintf(Detail, sizeof(Detail), "%ux%u %s", Texture.Size.Width, Texture.Size.Height,
					 DescribePixelFormat(Texture.Format).c_str());

			Infos.push_back(FEntryInfo{Texture.Name, "texture", Detail});
		}

		for (const FBufferCache::FEntryInfo& Buffer : BufferCache.GetEntries())
		{
			snprintf(Detail, sizeof(Detail), "%llu bytes, stride %u", (unsigned long long)Buffer.ByteSize,
					 Buffer.StructStride);

			Infos.push_back(FEntryInfo{Buffer.Name, "buffer", Detail});
		}

		return Infos;
	}
} // namespace Prism::Gpu
