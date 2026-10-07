#pragma once

// Host 层：浮点参考图与数值比较。
//
// 分工：
//   * PNG 截图（--capture）用于人眼看，不做像素级比较；
//   * 参考图（--write-reference / --reference）是 .f32 文件，用于数值回归（路线图 §10.1）。
// 格式：文本头 "PRISM1\n<width> <height> <channels>\n" + 原始 float32 负载（RGBA 顺序）。

#include <framework/core/Status.h>
#include <framework/core/Types.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Prism::Host
{
	struct FFloatImage
	{
		FExtent2D Size;
		uint32_t Channels = 4; // 统一按 RGBA 存；缺失的通道为 0，alpha 为 1
		std::vector<float> Pixels;

		[[nodiscard]] bool IsValid() const
		{
			return Size.IsValid() && Channels > 0 &&
				   Pixels.size() == size_t(Size.Width) * size_t(Size.Height) * Channels;
		}

		[[nodiscard]] float At(uint32_t X, uint32_t Y, uint32_t Channel) const
		{
			if (X >= Size.Width || Y >= Size.Height || Channel >= Channels)
				return 0.f;

			return Pixels[(size_t(Y) * Size.Width + X) * Channels + Channel];
		}
	};

	// 从 GPU 纹理读回为浮点图（支持 RGBA32_FLOAT / RGBA16_FLOAT / R32_FLOAT / R16_FLOAT）。
	TResult<FFloatImage> ReadTextureAsFloat(nvrhi::IDevice* Device, nvrhi::ITexture* Texture);

	bool SaveFloatImage(const std::filesystem::path& Path, const FFloatImage& Image);
	TResult<FFloatImage> LoadFloatImage(const std::filesystem::path& Path);

	struct FImageComparison
	{
		bool bValid = false;
		uint32_t DifferingPixels = 0;

		// 任一图像出现 inf / NaN 的像素数：非有限值必须显式报告，不能靠 max 差异掩盖
		uint32_t NonFinitePixels = 0;

		float MaxAbsolute = 0.f;
		float MeanAbsolute = 0.f;
		std::string Message;

		// 判定：没有非有限值，且最大差异在容差内
		[[nodiscard]] bool Passed(float Tolerance) const
		{
			return bValid && NonFinitePixels == 0 && MaxAbsolute <= Tolerance;
		}
	};

	// 逐像素逐通道比较；差异超过 tolerance 的像素计入 differingPixels。
	FImageComparison CompareImages(const FFloatImage& Reference, const FFloatImage& Current, float Tolerance);
} // namespace Prism::Host
