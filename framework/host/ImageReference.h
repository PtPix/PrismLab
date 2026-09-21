#pragma once

// Host 层：浮点参考图与数值比较。
//
// 分工：
//   * PNG 截图（--capture）用于人眼看，不做像素级比较；
//   * 参考图（--write-reference / --reference）是 .f32 文件，用于数值回归（路线图 §10.1）。
// 格式：文本头 "RLFLOAT1\n<width> <height> <channels>\n" + 原始 float32 负载（RGBA 顺序）。

#include <framework/types/Status.h>
#include <framework/types/Types.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace renderlab::host
{
    struct FloatImage
    {
        Extent2D size;
        uint32_t channels = 4;              // 统一按 RGBA 存；缺失的通道为 0，alpha 为 1
        std::vector<float> pixels;

        [[nodiscard]] bool IsValid() const
        {
            return size.IsValid() && channels > 0 &&
                pixels.size() == size_t(size.width) * size_t(size.height) * channels;
        }

        [[nodiscard]] float At(uint32_t x, uint32_t y, uint32_t channel) const
        {
            if (x >= size.width || y >= size.height || channel >= channels)
                return 0.f;

            return pixels[(size_t(y) * size.width + x) * channels + channel];
        }
    };

    // 从 GPU 纹理读回为浮点图（支持 RGBA32_FLOAT / RGBA16_FLOAT / R32_FLOAT / R16_FLOAT）。
    Result<FloatImage> ReadTextureAsFloat(nvrhi::IDevice* device, nvrhi::ITexture* texture);

    bool SaveFloatImage(const std::filesystem::path& path, const FloatImage& image);
    Result<FloatImage> LoadFloatImage(const std::filesystem::path& path);

    struct ImageComparison
    {
        bool valid = false;
        uint32_t differingPixels = 0;

        // 任一图像出现 inf / NaN 的像素数：非有限值必须显式报告，不能靠 max 差异掩盖
        uint32_t nonFinitePixels = 0;

        float maxAbsolute = 0.f;
        float meanAbsolute = 0.f;
        std::string message;

        // 判定：没有非有限值，且最大差异在容差内
        [[nodiscard]] bool Passed(float tolerance) const
        {
            return valid && nonFinitePixels == 0 && maxAbsolute <= tolerance;
        }
    };

    // 逐像素逐通道比较；差异超过 tolerance 的像素计入 differingPixels。
    ImageComparison CompareImages(const FloatImage& reference, const FloatImage& current, float tolerance);
}
