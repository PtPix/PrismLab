#pragma once

// NVRHI layer: format and usage translation.
//
// The framework types name formats without a graphics API (Prism::EPixelFormat); this is the only
// place that knows how they map onto NVRHI. Extend both enums together.

#include <framework/render/data/PixelFormat.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>

namespace Prism::Gpu
{
	enum class ETextureUsage : uint32_t
	{
		None = 0,
		ShaderResource = 1u << 0,
		RenderTarget = 1u << 1,
		UnorderedAccess = 1u << 2,
		DepthStencil = 1u << 3,
		CopySource = 1u << 4,
		CopyDest = 1u << 5,
	};

	constexpr ETextureUsage operator|(ETextureUsage A, ETextureUsage B)
	{
		return ETextureUsage(uint32_t(A) | uint32_t(B));
	}
	constexpr ETextureUsage operator&(ETextureUsage A, ETextureUsage B)
	{
		return ETextureUsage(uint32_t(A) & uint32_t(B));
	}
	constexpr ETextureUsage& operator|=(ETextureUsage& A, ETextureUsage B)
	{
		A = A | B;
		return A;
	}
	constexpr bool HasAny(ETextureUsage Value, ETextureUsage Test)
	{
		return (uint32_t(Value) & uint32_t(Test)) != 0;
	}
	constexpr bool HasAll(ETextureUsage Value, ETextureUsage Test)
	{
		return (uint32_t(Value) & uint32_t(Test)) == uint32_t(Test);
	}

	nvrhi::Format ToNvrhiFormat(EPixelFormat Format);
	EPixelFormat FromNvrhiFormat(nvrhi::Format Format);

	// Formats that NVRHI accepts as a UAV without a typeless view.
	bool IsUavCompatible(EPixelFormat Format);

	// Initial / permanent resource state for a requested usage set.
	nvrhi::ResourceStates GetInitialState(ETextureUsage Usage, EPixelFormat Format);

	const char* ToString(nvrhi::Format Format);
} // namespace Prism::Gpu
