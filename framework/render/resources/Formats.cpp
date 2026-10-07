#include "Formats.h"

namespace Prism::Gpu
{
	nvrhi::Format ToNvrhiFormat(EPixelFormat Format)
	{
		switch (Format)
		{
			case EPixelFormat::RgbA32Float:
				return nvrhi::Format::RGBA32_FLOAT;
			case EPixelFormat::RgbA16Float:
				return nvrhi::Format::RGBA16_FLOAT;
			case EPixelFormat::RG16Float:
				return nvrhi::Format::RG16_FLOAT;
			case EPixelFormat::R16Float:
				return nvrhi::Format::R16_FLOAT;
			case EPixelFormat::R32Float:
				return nvrhi::Format::R32_FLOAT;
			case EPixelFormat::RgbA8Unorm:
				return nvrhi::Format::RGBA8_UNORM;
			case EPixelFormat::RgbA8Snorm:
				return nvrhi::Format::RGBA8_SNORM;
			case EPixelFormat::RG16Unorm:
				return nvrhi::Format::RG16_UNORM;
			case EPixelFormat::R8Unorm:
				return nvrhi::Format::R8_UNORM;
			case EPixelFormat::D32Float:
				return nvrhi::Format::D32;
			case EPixelFormat::D24UnormS8Uint:
				return nvrhi::Format::D24S8;
			default:
				return nvrhi::Format::UNKNOWN;
		}
	}

	EPixelFormat FromNvrhiFormat(nvrhi::Format Format)
	{
		switch (Format)
		{
			case nvrhi::Format::RGBA32_FLOAT:
				return EPixelFormat::RgbA32Float;
			case nvrhi::Format::RGBA16_FLOAT:
				return EPixelFormat::RgbA16Float;
			case nvrhi::Format::RG16_FLOAT:
				return EPixelFormat::RG16Float;
			case nvrhi::Format::R16_FLOAT:
				return EPixelFormat::R16Float;
			case nvrhi::Format::R32_FLOAT:
				return EPixelFormat::R32Float;
			case nvrhi::Format::RGBA8_UNORM:
				return EPixelFormat::RgbA8Unorm;
			case nvrhi::Format::RGBA8_SNORM:
				return EPixelFormat::RgbA8Snorm;
			case nvrhi::Format::RG16_UNORM:
				return EPixelFormat::RG16Unorm;
			case nvrhi::Format::R8_UNORM:
				return EPixelFormat::R8Unorm;
			case nvrhi::Format::D32:
				return EPixelFormat::D32Float;
			case nvrhi::Format::D24S8:
				return EPixelFormat::D24UnormS8Uint;
			default:
				return EPixelFormat::Unknown;
		}
	}

	bool IsUavCompatible(EPixelFormat Format)
	{
		switch (Format)
		{
			case EPixelFormat::RgbA32Float:
			case EPixelFormat::RgbA16Float:
			case EPixelFormat::RG16Float:
			case EPixelFormat::R16Float:
			case EPixelFormat::R32Float:
			case EPixelFormat::RgbA8Unorm:
			case EPixelFormat::R8Unorm:
				return true;
			default:
				return false;
		}
	}

	nvrhi::ResourceStates GetInitialState(ETextureUsage Usage, EPixelFormat Format)
	{
		if (HasAny(Usage, ETextureUsage::DepthStencil) || IsDepthFormat(Format))
			return nvrhi::ResourceStates::DepthWrite;

		if (HasAny(Usage, ETextureUsage::RenderTarget))
			return nvrhi::ResourceStates::RenderTarget;

		if (HasAny(Usage, ETextureUsage::UnorderedAccess))
			return nvrhi::ResourceStates::UnorderedAccess;

		return nvrhi::ResourceStates::Common;
	}

	const char* ToString(nvrhi::Format Format)
	{
		switch (Format)
		{
			case nvrhi::Format::RGBA32_FLOAT:
				return "RGBA32_FLOAT";
			case nvrhi::Format::RGBA16_FLOAT:
				return "RGBA16_FLOAT";
			case nvrhi::Format::RG16_FLOAT:
				return "RG16_FLOAT";
			case nvrhi::Format::R16_FLOAT:
				return "R16_FLOAT";
			case nvrhi::Format::R32_FLOAT:
				return "R32_FLOAT";
			case nvrhi::Format::RGBA8_UNORM:
				return "RGBA8_UNORM";
			case nvrhi::Format::RGBA8_SNORM:
				return "RGBA8_SNORM";
			case nvrhi::Format::RG16_UNORM:
				return "RG16_UNORM";
			case nvrhi::Format::R8_UNORM:
				return "R8_UNORM";
			case nvrhi::Format::D32:
				return "D32_FLOAT";
			case nvrhi::Format::UNKNOWN:
				return "unknown";
			default:
				return "other";
		}
	}
} // namespace Prism::Gpu
