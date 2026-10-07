#include "ImageReference.h"

#include <framework/tools/capture/TextureReadback.h>

#include <donut/core/log.h>
#include <donut/core/math/math.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Prism::Host
{
	namespace
	{
		EPixelFormat PickReadbackFormat(nvrhi::Format Format)
		{
			switch (Format)
			{
				case nvrhi::Format::RGBA32_FLOAT:
					return EPixelFormat::RgbA32Float;
				case nvrhi::Format::RGBA16_FLOAT:
					return EPixelFormat::RgbA16Float;
				case nvrhi::Format::R32_FLOAT:
					return EPixelFormat::R32Float;
				case nvrhi::Format::R16_FLOAT:
					return EPixelFormat::R16Float;
				default:
					return EPixelFormat::Unknown;
			}
		}
	} // namespace

	TResult<FFloatImage> ReadTextureAsFloat(nvrhi::IDevice* Device, nvrhi::ITexture* Texture)
	{
		if (!Device || !Texture)
			return FStatus::Error(EErrorCode::InvalidArgument, "device and texture are required");

		const EPixelFormat Format = PickReadbackFormat(Texture->getDesc().format);
		if (Format == EPixelFormat::Unknown)
		{
			return FStatus::Error(
				EErrorCode::Unsupported,
				"the reference image path supports RGBA32_FLOAT / RGBA16_FLOAT / R32_FLOAT / R16_FLOAT outputs");
		}

		const TResult<Gpu::FTextureData> Data = Gpu::ReadTexture(Device, Texture, Format);
		if (!Data.IsOk())
			return Data.GetStatus();

		const Gpu::FTextureData& TextureData = Data.GetValue();

		FFloatImage Image;
		Image.Size = TextureData.Size;
		Image.Channels = 4;
		Image.Pixels.resize(size_t(Image.Size.Width) * Image.Size.Height * Image.Channels);

		for (uint32_t Y = 0; Y < Image.Size.Height; ++Y)
		{
			for (uint32_t X = 0; X < Image.Size.Width; ++X)
			{
				const dm::float4 Color = TextureData.ColorAt(X, Y);
				float* Pixel = Image.Pixels.data() + (size_t(Y) * Image.Size.Width + X) * Image.Channels;
				Pixel[0] = Color.x;
				Pixel[1] = Color.y;
				Pixel[2] = Color.z;
				Pixel[3] = Color.w;
			}
		}

		return Image;
	}

	bool SaveFloatImage(const std::filesystem::path& Path, const FFloatImage& Image)
	{
		if (!Image.IsValid())
		{
			donut::log::error("Prism: refusing to save an invalid reference image.");
			return false;
		}

		FILE* File = nullptr;
		if (_wfopen_s(&File, Path.c_str(), L"wb") != 0 || !File)
		{
			donut::log::error("Prism: cannot write the reference image to %s", Path.string().c_str());
			return false;
		}

		fprintf(File, "PRISM1\n%u %u %u\n", Image.Size.Width, Image.Size.Height, Image.Channels);
		const size_t Written = fwrite(Image.Pixels.data(), sizeof(float), Image.Pixels.size(), File);
		fclose(File);

		if (Written != Image.Pixels.size())
		{
			donut::log::error("Prism: short write for %s", Path.string().c_str());
			return false;
		}

		donut::log::info("Prism: reference image written to %s (%u x %u, %u channels).", Path.string().c_str(),
						 Image.Size.Width, Image.Size.Height, Image.Channels);
		return true;
	}

	TResult<FFloatImage> LoadFloatImage(const std::filesystem::path& Path)
	{
		FILE* File = nullptr;
		if (_wfopen_s(&File, Path.c_str(), L"rb") != 0 || !File)
			return FStatus::Error(EErrorCode::ResourceMissing, "cannot open the reference image: " + Path.string());

		char Magic[16] = {};
		if (fscanf_s(File, "%15s", Magic, unsigned(_countof(Magic))) != 1 || strcmp(Magic, "PRISM1") != 0)
		{
			fclose(File);
			return FStatus::Error(EErrorCode::FormatMismatch,
								  "not a Prism reference image (expected the PRISM1 header): " + Path.string());
		}

		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Channels = 0;
		if (fscanf_s(File, "%u %u %u", &Width, &Height, &Channels) != 3)
		{
			fclose(File);
			return FStatus::Error(EErrorCode::FormatMismatch, "malformed reference image header: " + Path.string());
		}

		// 必须吃掉表头最后的换行：否则二进制负载会从下一个字节开始，整体错位。
		if (fgetc(File) != '\n')
		{
			fclose(File);
			return FStatus::Error(EErrorCode::FormatMismatch, "malformed reference image header: " + Path.string());
		}

		FFloatImage Image;
		Image.Size = FExtent2D{Width, Height};
		Image.Channels = Channels;
		Image.Pixels.resize(size_t(Width) * size_t(Height) * size_t(Channels));

		const size_t Read = fread(Image.Pixels.data(), sizeof(float), Image.Pixels.size(), File);
		fclose(File);

		if (Read != Image.Pixels.size())
			return FStatus::Error(EErrorCode::FormatMismatch, "truncated reference image: " + Path.string());

		return Image;
	}

	FImageComparison CompareImages(const FFloatImage& Reference, const FFloatImage& Current, float Tolerance)
	{
		FImageComparison Comparison;

		if (!Reference.IsValid() || !Current.IsValid())
		{
			Comparison.Message = "one of the images is invalid";
			return Comparison;
		}

		if (Reference.Size != Current.Size || Reference.Channels != Current.Channels)
		{
			char Message[160] = {};
			snprintf(Message, sizeof(Message), "size mismatch: reference %ux%u x%u, current %ux%u x%u",
					 Reference.Size.Width, Reference.Size.Height, Reference.Channels, Current.Size.Width,
					 Current.Size.Height, Current.Channels);
			Comparison.Message = Message;
			return Comparison;
		}

		Comparison.bValid = true;

		double Sum = 0.0;
		uint64_t Samples = 0;

		for (uint32_t Y = 0; Y < Reference.Size.Height; ++Y)
		{
			for (uint32_t X = 0; X < Reference.Size.Width; ++X)
			{
				bool bPixelDiffers = false;
				bool bPixelNonFinite = false;

				for (uint32_t Channel = 0; Channel < Reference.Channels; ++Channel)
				{
					const float ReferenceValue = Reference.At(X, Y, Channel);
					const float CurrentValue = Current.At(X, Y, Channel);

					if (!std::isfinite(ReferenceValue) || !std::isfinite(CurrentValue))
					{
						bPixelNonFinite = true;
						continue;
					}

					const float Difference = std::fabs(ReferenceValue - CurrentValue);
					Sum += Difference;
					++Samples;

					if (Difference > Tolerance)
						bPixelDiffers = true;

					Comparison.MaxAbsolute = std::max(Comparison.MaxAbsolute, Difference);
				}

				if (bPixelNonFinite)
				{
					++Comparison.NonFinitePixels;
					++Comparison.DifferingPixels; // 非有限值同样算"不同"
					continue;
				}

				if (bPixelDiffers)
					++Comparison.DifferingPixels;
			}
		}

		if (Samples > 0)
			Comparison.MeanAbsolute = float(Sum / double(Samples));

		return Comparison;
	}
} // namespace Prism::Host
