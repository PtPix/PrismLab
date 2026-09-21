#pragma once

// NVRHI layer: getting pixels back to the CPU.
//
// Two uses, both belonging to the verification story:
//   * SaveTextureToImage  -> 人工查看与文档里的参考图（PNG 等）
//   * ReadTexture         -> 数值回归（矩阵往返、深度重建、Pass 的边界像素）
//
// ReadTexture submits its own command list and waits for the GPU: it must not be called every frame,
// only from截图/验证路径.

#include "Formats.h"

#include <framework/types/PixelFormat.h>
#include <framework/types/Status.h>
#include <framework/types/Types.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace donut::engine
{
    class CommonRenderPasses;
}

namespace renderlab::gpu
{
    // 图像格式由扩展名决定（PNG/BMP/JPG/TGA）。
    bool SaveTextureToImage(
        nvrhi::IDevice* device,
        donut::engine::CommonRenderPasses* commonPasses,
        nvrhi::ITexture* texture,
        nvrhi::ResourceStates textureState,
        const std::filesystem::path& path,
        bool saveAlphaChannel = true);

    struct TextureData
    {
        Extent2D size;
        PixelFormat format = PixelFormat::Unknown;
        uint32_t channels = 0;

        // 紧凑排列的原始像素，rowPitch 已去掉填充
        std::vector<uint8_t> bytes;
        uint32_t rowPitch = 0;

        // 仅对 32 位浮点格式有效；索引越界返回 0
        [[nodiscard]] float FloatAt(uint32_t x, uint32_t y, uint32_t channel = 0) const;

        // 归一化到 [0,1] 的颜色访问（8 位与 16 位浮点格式会自动转换）
        [[nodiscard]] dm::float4 ColorAt(uint32_t x, uint32_t y) const;
    };

    // 读回 slice 0 / mip 0。内部提交命令列表并等待 GPU 空闲。
    Result<TextureData> ReadTexture(nvrhi::IDevice* device, nvrhi::ITexture* texture, PixelFormat format);

    // 两张同尺寸浮点图的统计差异，用于数值回归。
    struct ImageDifference
    {
        float maxAbsolute = 0.f;
        float meanAbsolute = 0.f;
        uint32_t differingPixels = 0;
    };

    ImageDifference CompareFloatImages(
        const TextureData& a,
        const TextureData& b,
        uint32_t channelCount,
        float tolerance);
}
