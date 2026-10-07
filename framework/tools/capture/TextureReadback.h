#pragma once

// NVRHI layer: getting pixels back to the CPU.
//
// Two uses, both belonging to the verification story:
//   * SaveTextureToImage  -> 人工查看与文档里的参考图（PNG 等）
//   * ReadTexture         -> 数值回归（矩阵往返、深度重建、Pass 的边界像素）
//
// ReadTexture submits its own command list and waits for the GPU: it must not be called every frame,
// only from截图/验证路径.

#include "framework/render/resources/Formats.h"

#include <framework/render/data/PixelFormat.h>
#include <framework/core/Status.h>
#include <framework/core/Types.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace donut::engine
{
	class CommonRenderPasses;
}

namespace Prism::Gpu
{
	// 图像格式由扩展名决定（PNG/BMP/JPG/TGA）。
	bool SaveTextureToImage(nvrhi::IDevice* Device, donut::engine::CommonRenderPasses* CommonPasses,
							nvrhi::ITexture* Texture, nvrhi::ResourceStates TextureState,
							const std::filesystem::path& Path, bool bSaveAlphaChannel = true);

	struct FTextureData
	{
		FExtent2D Size;
		EPixelFormat Format = EPixelFormat::Unknown;
		uint32_t Channels = 0;

		// 紧凑排列的原始像素，rowPitch 已去掉填充
		std::vector<uint8_t> Bytes;
		uint32_t RowPitch = 0;

		// 仅对 32 位浮点格式有效；索引越界返回 0
		[[nodiscard]] float FloatAt(uint32_t X, uint32_t Y, uint32_t Channel = 0) const;

		// 归一化到 [0,1] 的颜色访问（8 位与 16 位浮点格式会自动转换）
		[[nodiscard]] dm::float4 ColorAt(uint32_t X, uint32_t Y) const;
	};

	// 读回 slice 0 / mip 0。内部提交命令列表并等待 GPU 空闲。
	TResult<FTextureData> ReadTexture(nvrhi::IDevice* Device, nvrhi::ITexture* Texture, EPixelFormat Format);

	// 两张同尺寸浮点图的统计差异，用于数值回归。
	struct FImageDifference
	{
		float MaxAbsolute = 0.f;
		float MeanAbsolute = 0.f;
		uint32_t DifferingPixels = 0;
	};

	FImageDifference CompareFloatImages(const FTextureData& A, const FTextureData& B, uint32_t ChannelCount,
										float Tolerance);
} // namespace Prism::Gpu
