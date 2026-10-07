#pragma once

// Shared value types use Donut math, without scene, device or UI ownership.

#include <donut/core/math/math.h>

#include <algorithm>
#include <cstdint>

namespace dm = donut::math;

namespace Prism
{
	using FViewId = uint32_t;
	constexpr FViewId KPrimaryViewId = 0;
	constexpr uint32_t KMaxViews = 4;

	struct FExtent2D
	{
		uint32_t Width = 0;
		uint32_t Height = 0;

		[[nodiscard]] bool IsValid() const
		{
			return Width > 0 && Height > 0;
		}
		[[nodiscard]] float AspectRatio() const
		{
			return (Height > 0) ? float(Width) / float(Height) : 1.f;
		}
		[[nodiscard]] uint64_t PixelCount() const
		{
			return uint64_t(Width) * uint64_t(Height);
		}

		[[nodiscard]] FExtent2D Scaled(float Factor) const
		{
			return FExtent2D{std::max(1u, uint32_t(float(Width) * Factor + 0.5f)),
							 std::max(1u, uint32_t(float(Height) * Factor + 0.5f))};
		}

		[[nodiscard]] dm::uint2 ToUint2() const
		{
			return dm::uint2(Width, Height);
		}

		static FExtent2D From(dm::uint2 Value)
		{
			return FExtent2D{Value.x, Value.y};
		}

		bool operator==(const FExtent2D& Other) const
		{
			return Width == Other.Width && Height == Other.Height;
		}
		bool operator!=(const FExtent2D& Other) const
		{
			return !(*this == Other);
		}
	};

	// Aspect ratio without a zero-height division.
	inline float AspectRatio(const FExtent2D& Extent)
	{
		return Extent.AspectRatio();
	}
} // namespace Prism
