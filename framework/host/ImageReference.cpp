#include "ImageReference.h"

#include <framework/nvrhi/TextureReadback.h>

#include <donut/core/log.h>
#include <donut/core/math/math.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace renderlab::host
{
    namespace
    {
        PixelFormat PickReadbackFormat(nvrhi::Format format)
        {
            switch (format)
            {
            case nvrhi::Format::RGBA32_FLOAT: return PixelFormat::RGBA32_FLOAT;
            case nvrhi::Format::RGBA16_FLOAT: return PixelFormat::RGBA16_FLOAT;
            case nvrhi::Format::R32_FLOAT:    return PixelFormat::R32_FLOAT;
            case nvrhi::Format::R16_FLOAT:    return PixelFormat::R16_FLOAT;
            default:                          return PixelFormat::Unknown;
            }
        }
    }

    Result<FloatImage> ReadTextureAsFloat(nvrhi::IDevice* device, nvrhi::ITexture* texture)
    {
        if (!device || !texture)
            return Status::Error(ErrorCode::InvalidArgument, "device and texture are required");

        const PixelFormat format = PickReadbackFormat(texture->getDesc().format);
        if (format == PixelFormat::Unknown)
        {
            return Status::Error(ErrorCode::Unsupported,
                "the reference image path supports RGBA32_FLOAT / RGBA16_FLOAT / R32_FLOAT / R16_FLOAT outputs");
        }

        const Result<gpu::TextureData> data = gpu::ReadTexture(device, texture, format);
        if (!data.IsOk())
            return data.GetStatus();

        const gpu::TextureData& textureData = data.Value();

        FloatImage image;
        image.size = textureData.size;
        image.channels = 4;
        image.pixels.resize(size_t(image.size.width) * image.size.height * image.channels);

        for (uint32_t y = 0; y < image.size.height; ++y)
        {
            for (uint32_t x = 0; x < image.size.width; ++x)
            {
                const dm::float4 color = textureData.ColorAt(x, y);
                float* pixel = image.pixels.data() + (size_t(y) * image.size.width + x) * image.channels;
                pixel[0] = color.x;
                pixel[1] = color.y;
                pixel[2] = color.z;
                pixel[3] = color.w;
            }
        }

        return image;
    }

    bool SaveFloatImage(const std::filesystem::path& path, const FloatImage& image)
    {
        if (!image.IsValid())
        {
            donut::log::error("RenderLab: refusing to save an invalid reference image.");
            return false;
        }

        FILE* file = nullptr;
        if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || !file)
        {
            donut::log::error("RenderLab: cannot write the reference image to %s", path.string().c_str());
            return false;
        }

        fprintf(file, "RLFLOAT1\n%u %u %u\n", image.size.width, image.size.height, image.channels);
        const size_t written = fwrite(image.pixels.data(), sizeof(float), image.pixels.size(), file);
        fclose(file);

        if (written != image.pixels.size())
        {
            donut::log::error("RenderLab: short write for %s", path.string().c_str());
            return false;
        }

        donut::log::info("RenderLab: reference image written to %s (%u x %u, %u channels).",
            path.string().c_str(), image.size.width, image.size.height, image.channels);
        return true;
    }

    Result<FloatImage> LoadFloatImage(const std::filesystem::path& path)
    {
        FILE* file = nullptr;
        if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file)
            return Status::Error(ErrorCode::ResourceMissing, "cannot open the reference image: " + path.string());

        char magic[16] = {};
        if (fscanf_s(file, "%15s", magic, unsigned(_countof(magic))) != 1 || strcmp(magic, "RLFLOAT1") != 0)
        {
            fclose(file);
            return Status::Error(ErrorCode::FormatMismatch,
                "not a RenderLab reference image (expected the RLFLOAT1 header): " + path.string());
        }

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channels = 0;
        if (fscanf_s(file, "%u %u %u", &width, &height, &channels) != 3)
        {
            fclose(file);
            return Status::Error(ErrorCode::FormatMismatch, "malformed reference image header: " + path.string());
        }

        // 必须吃掉表头最后的换行：否则二进制负载会从下一个字节开始，整体错位。
        if (fgetc(file) != '\n')
        {
            fclose(file);
            return Status::Error(ErrorCode::FormatMismatch, "malformed reference image header: " + path.string());
        }

        FloatImage image;
        image.size = Extent2D{ width, height };
        image.channels = channels;
        image.pixels.resize(size_t(width) * size_t(height) * size_t(channels));

        const size_t read = fread(image.pixels.data(), sizeof(float), image.pixels.size(), file);
        fclose(file);

        if (read != image.pixels.size())
            return Status::Error(ErrorCode::FormatMismatch, "truncated reference image: " + path.string());

        return image;
    }

    ImageComparison CompareImages(const FloatImage& reference, const FloatImage& current, float tolerance)
    {
        ImageComparison comparison;

        if (!reference.IsValid() || !current.IsValid())
        {
            comparison.message = "one of the images is invalid";
            return comparison;
        }

        if (reference.size != current.size || reference.channels != current.channels)
        {
            char message[160] = {};
            snprintf(message, sizeof(message),
                "size mismatch: reference %ux%u x%u, current %ux%u x%u",
                reference.size.width, reference.size.height, reference.channels,
                current.size.width, current.size.height, current.channels);
            comparison.message = message;
            return comparison;
        }

        comparison.valid = true;

        double sum = 0.0;
        uint64_t samples = 0;

        for (uint32_t y = 0; y < reference.size.height; ++y)
        {
            for (uint32_t x = 0; x < reference.size.width; ++x)
            {
                bool pixelDiffers = false;
                bool pixelNonFinite = false;

                for (uint32_t channel = 0; channel < reference.channels; ++channel)
                {
                    const float referenceValue = reference.At(x, y, channel);
                    const float currentValue = current.At(x, y, channel);

                    if (!std::isfinite(referenceValue) || !std::isfinite(currentValue))
                    {
                        pixelNonFinite = true;
                        continue;
                    }

                    const float difference = std::fabs(referenceValue - currentValue);
                    sum += difference;
                    ++samples;

                    if (difference > tolerance)
                        pixelDiffers = true;

                    comparison.maxAbsolute = std::max(comparison.maxAbsolute, difference);
                }

                if (pixelNonFinite)
                {
                    ++comparison.nonFinitePixels;
                    ++comparison.differingPixels;   // 非有限值同样算"不同"
                    continue;
                }

                if (pixelDiffers)
                    ++comparison.differingPixels;
            }
        }

        if (samples > 0)
            comparison.meanAbsolute = float(sum / double(samples));

        return comparison;
    }
}
