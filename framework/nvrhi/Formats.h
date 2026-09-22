#pragma once

// NVRHI layer: format and usage translation.
//
// The framework types name formats without a graphics API (prism::PixelFormat); this is the only
// place that knows how they map onto NVRHI. Extend both enums together.

#include <framework/types/PixelFormat.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>

namespace prism::gpu
{
    enum class TextureUsage : uint32_t
    {
        None = 0,
        ShaderResource = 1u << 0,
        RenderTarget = 1u << 1,
        UnorderedAccess = 1u << 2,
        DepthStencil = 1u << 3,
        CopySource = 1u << 4,
        CopyDest = 1u << 5,
  };

    constexpr TextureUsage operator|(TextureUsage a, TextureUsage b) { return TextureUsage(uint32_t(a) | uint32_t(b)); }
    constexpr TextureUsage operator&(TextureUsage a, TextureUsage b) { return TextureUsage(uint32_t(a) & uint32_t(b)); }
    constexpr TextureUsage& operator|=(TextureUsage& a, TextureUsage b) { a = a | b; return a; }
    constexpr bool HasAny(TextureUsage value, TextureUsage test) { return (uint32_t(value) & uint32_t(test)) != 0; }
    constexpr bool HasAll(TextureUsage value, TextureUsage test) { return (uint32_t(value) & uint32_t(test)) == uint32_t(test); }

    nvrhi::Format ToNvrhiFormat(PixelFormat format);
    PixelFormat FromNvrhiFormat(nvrhi::Format format);

    // Formats that NVRHI accepts as a UAV without a typeless view.
    bool IsUavCompatible(PixelFormat format);

    // Initial / permanent resource state for a requested usage set.
    nvrhi::ResourceStates GetInitialState(TextureUsage usage, PixelFormat format);

    const char* ToString(nvrhi::Format format);
}
