#pragma once

// Shared value types use Donut math, without scene, device or UI ownership.

#include <donut/core/math/math.h>

#include <algorithm>
#include <cstdint>

namespace dm = donut::math;

namespace prism
{
    using ViewId = uint32_t;
    constexpr ViewId kPrimaryViewId = 0;
    constexpr uint32_t kMaxViews = 4;

    struct Extent2D
    {
        uint32_t width = 0;
        uint32_t height = 0;

        [[nodiscard]] bool IsValid() const { return width > 0 && height > 0; }
        [[nodiscard]] float AspectRatio() const { return (height > 0) ? float(width) / float(height) : 1.f; }
        [[nodiscard]] uint64_t PixelCount() const { return uint64_t(width) * uint64_t(height); }

        [[nodiscard]] Extent2D Scaled(float factor) const
        {
            return Extent2D{
                std::max(1u, uint32_t(float(width) * factor + 0.5f)),
                std::max(1u, uint32_t(float(height) * factor + 0.5f)) };
        }

        [[nodiscard]] dm::uint2 ToUint2() const { return dm::uint2(width, height); }

        static Extent2D From(dm::uint2 value)
        {
            return Extent2D{ value.x, value.y };
        }

        bool operator==(const Extent2D& other) const { return width == other.width && height == other.height; }
        bool operator!=(const Extent2D& other) const { return !(*this == other); }
    };

    // Aspect ratio without a zero-height division.
    inline float AspectRatio(const Extent2D& extent)
    {
        return extent.AspectRatio();
    }
}
