#pragma once

// Framework types: texture formats without a graphics API.
//
// The NVRHI layer converts these to nvrhi::Format (see framework/render/resources/Formats.h), so that a
// resource request can be described without naming a graphics API type. Only the formats that the
// experiment actually uses for scene data, debug output and readback are listed here.

#include <cstdint>

namespace Prism
{
	enum class EPixelFormat : uint32_t
	{
		Unknown = 0,

		// Color and data targets
		RgbA32Float,
		RgbA16Float,
		RG16Float,
		R16Float,
		R32Float,
		RgbA8Unorm,
		RgbA8Snorm,
		RG16Unorm,
		R8Unorm,

		// Depth
		D32Float,
		D24UnormS8Uint,

		Count
	};

	// Bytes per pixel for the formats above; 0 when the format has no plain linear layout.
	constexpr uint32_t GetBytesPerPixel(EPixelFormat Format)
	{
		switch (Format)
		{
			case EPixelFormat::RgbA32Float:
				return 16;
			case EPixelFormat::RgbA16Float:
				return 8;
			case EPixelFormat::RG16Float:
				return 4;
			case EPixelFormat::R16Float:
				return 2;
			case EPixelFormat::R32Float:
				return 4;
			case EPixelFormat::RgbA8Unorm:
				return 4;
			case EPixelFormat::RgbA8Snorm:
				return 4;
			case EPixelFormat::RG16Unorm:
				return 4;
			case EPixelFormat::R8Unorm:
				return 1;
			case EPixelFormat::D32Float:
				return 4;
			case EPixelFormat::D24UnormS8Uint:
				return 4;
			default:
				return 0;
		}
	}

	// Number of color channels that carry meaningful data, for readback interpretation.
	constexpr uint32_t GetChannelCount(EPixelFormat Format)
	{
		switch (Format)
		{
			case EPixelFormat::RgbA32Float:
			case EPixelFormat::RgbA16Float:
			case EPixelFormat::RgbA8Unorm:
			case EPixelFormat::RgbA8Snorm:
				return 4;
			case EPixelFormat::RG16Float:
			case EPixelFormat::RG16Unorm:
				return 2;
			case EPixelFormat::R16Float:
			case EPixelFormat::R32Float:
			case EPixelFormat::R8Unorm:
			case EPixelFormat::D32Float:
				return 1;
			default:
				return 0;
		}
	}

	// True when the format can be read as 32-bit floats (directly or by promoting halfs).
	constexpr bool IsFloatFormat(EPixelFormat Format)
	{
		switch (Format)
		{
			case EPixelFormat::RgbA32Float:
			case EPixelFormat::RgbA16Float:
			case EPixelFormat::RG16Float:
			case EPixelFormat::R16Float:
			case EPixelFormat::R32Float:
			case EPixelFormat::D32Float:
				return true;
			default:
				return false;
		}
	}

	constexpr bool IsDepthFormat(EPixelFormat Format)
	{
		return Format == EPixelFormat::D32Float || Format == EPixelFormat::D24UnormS8Uint;
	}
} // namespace Prism
