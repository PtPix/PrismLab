#include "TextureReadback.h"

#include <donut/core/log.h>
#include <donut/engine/TextureCache.h>

#include <cmath>
#include <cstring>

namespace Prism::Gpu
{
	bool SaveTextureToImage(nvrhi::IDevice* Device, donut::engine::CommonRenderPasses* CommonPasses,
							nvrhi::ITexture* Texture, nvrhi::ResourceStates TextureState,
							const std::filesystem::path& Path, bool bSaveAlphaChannel)
	{
		if (!Device || !CommonPasses || !Texture)
		{
			donut::log::error("SaveTextureToImage: device, common passes and texture are required.");
			return false;
		}

		const std::string FileName = Path.string();
		const bool bSaved = donut::engine::SaveTextureToFile(Device, CommonPasses, Texture, TextureState,
															 FileName.c_str(), bSaveAlphaChannel);

		if (!bSaved)
		{
			donut::log::error("SaveTextureToImage: failed to write %s.", FileName.c_str());
			return false;
		}

		donut::log::info("Prism: wrote %s", FileName.c_str());
		return true;
	}

	namespace
	{
		// 把一行像素从半精度浮点转换为 32 位浮点。
		float HalfToFloat(uint16_t Value)
		{
			const uint32_t Sign = uint32_t(Value & 0x8000u) << 16;
			const uint32_t Exponent = (Value >> 10) & 0x1fu;
			const uint32_t Mantissa = Value & 0x3ffu;

			uint32_t Bits = Sign;
			if (Exponent == 0)
			{
				if (Mantissa == 0)
				{
					Bits = Sign;
				}
				else
				{
					// 次正规数
					uint32_t E = 127 - 15 + 1;
					uint32_t M = Mantissa;
					while ((M & 0x400u) == 0)
					{
						M <<= 1;
						--E;
					}

					Bits |= (E << 23) | ((M & 0x3ffu) << 13);
				}
			}
			else if (Exponent == 0x1fu)
			{
				Bits |= 0x7f800000u | (Mantissa << 13);
			}
			else
			{
				Bits |= ((Exponent + 127 - 15) << 23) | (Mantissa << 13);
			}

			float Result = 0.f;
			std::memcpy(&Result, &Bits, sizeof(Result));
			return Result;
		}
	} // namespace

	float FTextureData::FloatAt(uint32_t X, uint32_t Y, uint32_t Channel) const
	{
		if (X >= Size.Width || Y >= Size.Height || Channel >= Channels)
			return 0.f;

		const uint8_t* Pixel = Bytes.data() + size_t(Y) * RowPitch + size_t(X) * GetBytesPerPixel(Format);

		switch (Format)
		{
			case EPixelFormat::RgbA32Float:
				return reinterpret_cast<const float*>(Pixel)[Channel];
			case EPixelFormat::R32Float:
				return reinterpret_cast<const float*>(Pixel)[0];
			case EPixelFormat::RG16Float:
			case EPixelFormat::R16Float:
			case EPixelFormat::RgbA16Float:
				return HalfToFloat(reinterpret_cast<const uint16_t*>(Pixel)[Channel]);
			default:
				return 0.f;
		}
	}

	dm::float4 FTextureData::ColorAt(uint32_t X, uint32_t Y) const
	{
		if (X >= Size.Width || Y >= Size.Height)
			return dm::float4(0.f);

		const uint8_t* Pixel = Bytes.data() + size_t(Y) * RowPitch + size_t(X) * GetBytesPerPixel(Format);

		switch (Format)
		{
			case EPixelFormat::RgbA32Float:
			{
				const float* Values = reinterpret_cast<const float*>(Pixel);
				return dm::float4(Values[0], Values[1], Values[2], Values[3]);
			}
			case EPixelFormat::RgbA16Float:
				return dm::float4(HalfToFloat(reinterpret_cast<const uint16_t*>(Pixel)[0]),
								  HalfToFloat(reinterpret_cast<const uint16_t*>(Pixel)[1]),
								  HalfToFloat(reinterpret_cast<const uint16_t*>(Pixel)[2]),
								  HalfToFloat(reinterpret_cast<const uint16_t*>(Pixel)[3]));
			case EPixelFormat::R32Float:
			{
				const float Value = *reinterpret_cast<const float*>(Pixel);
				return dm::float4(Value, 0.f, 0.f, 1.f);
			}
			case EPixelFormat::R16Float:
			{
				const float Value = HalfToFloat(*reinterpret_cast<const uint16_t*>(Pixel));
				return dm::float4(Value, 0.f, 0.f, 1.f);
			}
			case EPixelFormat::RG16Float:
				return dm::float4(HalfToFloat(reinterpret_cast<const uint16_t*>(Pixel)[0]),
								  HalfToFloat(reinterpret_cast<const uint16_t*>(Pixel)[1]), 0.f, 1.f);
			case EPixelFormat::RgbA8Unorm:
				return dm::float4(Pixel[0] / 255.f, Pixel[1] / 255.f, Pixel[2] / 255.f, Pixel[3] / 255.f);
			case EPixelFormat::R8Unorm:
				return dm::float4(Pixel[0] / 255.f, 0.f, 0.f, 1.f);
			default:
				return dm::float4(0.f);
		}
	}

	TResult<FTextureData> ReadTexture(nvrhi::IDevice* Device, nvrhi::ITexture* Texture, EPixelFormat Format)
	{
		if (!Device || !Texture)
			return FStatus::Error(EErrorCode::InvalidArgument, "device and texture are required");

		if (Format == EPixelFormat::Unknown)
			return FStatus::Error(EErrorCode::InvalidArgument, "readback format must be specified explicitly");

		const nvrhi::TextureDesc& SourceDesc = Texture->getDesc();

		// 只改 staging 需要的东西：拷贝尺寸由 mip/array/format 决定，其余标志保持与源纹理一致。
		// 清掉 isShaderResource 之类的标志会让 D3D12 后端在创建 staging 时崩溃（实测），
		// 所以这里刻意不碰它们，只做最小的规范化。
		nvrhi::TextureDesc StagingDesc = SourceDesc;
		StagingDesc.dimension = nvrhi::TextureDimension::Texture2D;
		StagingDesc.mipLevels = 1;
		StagingDesc.arraySize = 1;
		StagingDesc.isVirtual = false;
		StagingDesc.isTiled = false;
		StagingDesc.debugName = SourceDesc.debugName + "_Readback";

		nvrhi::StagingTextureHandle Staging = Device->createStagingTexture(StagingDesc, nvrhi::CpuAccessMode::Read);
		if (!Staging)
			return FStatus::Error(EErrorCode::DeviceError, "failed to create a staging texture");

		// 读回走独立命令列表并等待 GPU 完成：只用于截图与验证，不进入每帧路径。
		nvrhi::CommandListHandle Commands = Device->createCommandList();
		Commands->open();
		Commands->copyTexture(Staging, nvrhi::TextureSlice().setMipLevel(0).setArraySlice(0), Texture,
							  nvrhi::TextureSlice().setMipLevel(0).setArraySlice(0));
		Commands->close();
		Device->executeCommandList(Commands);
		Device->waitForIdle();

		size_t RowPitch = 0;
		const void* Mapped =
			Device->mapStagingTexture(Staging, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &RowPitch);
		if (!Mapped)
			return FStatus::Error(EErrorCode::DeviceError, "failed to map the staging texture");

		FTextureData Data;
		Data.Size = FExtent2D{SourceDesc.width, SourceDesc.height};
		Data.Format = Format;
		Data.Channels = GetChannelCount(Format);
		Data.RowPitch = uint32_t(RowPitch);

		const uint32_t BytesPerPixel = GetBytesPerPixel(Format);
		if (BytesPerPixel == 0)
		{
			Device->unmapStagingTexture(Staging);
			return FStatus::Error(EErrorCode::Unsupported, "the requested readback format has no linear layout");
		}

		Data.Bytes.resize(size_t(Data.Size.Width) * Data.Size.Height * BytesPerPixel);
		for (uint32_t Y = 0; Y < Data.Size.Height; ++Y)
		{
			const uint8_t* Source = static_cast<const uint8_t*>(Mapped) + size_t(Y) * Data.RowPitch;
			std::memcpy(Data.Bytes.data() + size_t(Y) * Data.Size.Width * BytesPerPixel, Source,
						size_t(Data.Size.Width) * BytesPerPixel);
		}

		Device->unmapStagingTexture(Staging);

		// 读回后紧凑排列，方便调用方按下标访问
		Data.RowPitch = Data.Size.Width * BytesPerPixel;

		return Data;
	}

	FImageDifference CompareFloatImages(const FTextureData& A, const FTextureData& B, uint32_t ChannelCount,
										float Tolerance)
	{
		FImageDifference Difference;

		if (A.Size != B.Size || A.Channels == 0 || B.Channels == 0)
			return Difference;

		const uint32_t Channels = std::min(ChannelCount, std::min(A.Channels, B.Channels));
		double Sum = 0.0;
		uint64_t Samples = 0;

		for (uint32_t Y = 0; Y < A.Size.Height; ++Y)
		{
			for (uint32_t X = 0; X < A.Size.Width; ++X)
			{
				bool bPixelDiffers = false;

				for (uint32_t Channel = 0; Channel < Channels; ++Channel)
				{
					const float DifferenceValue = std::fabs(A.FloatAt(X, Y, Channel) - B.FloatAt(X, Y, Channel));
					Sum += DifferenceValue;
					++Samples;

					if (DifferenceValue > Tolerance)
						bPixelDiffers = true;

					Difference.MaxAbsolute = std::max(Difference.MaxAbsolute, DifferenceValue);
				}

				if (bPixelDiffers)
					++Difference.DifferingPixels;
			}
		}

		if (Samples > 0)
			Difference.MeanAbsolute = float(Sum / double(Samples));

		return Difference;
	}
} // namespace Prism::Gpu
