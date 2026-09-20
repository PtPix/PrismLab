#include "TextureReadback.h"

#include <donut/core/log.h>
#include <donut/engine/TextureCache.h>

#include <cmath>
#include <cstring>

namespace renderlab::gpu
{
    bool SaveTextureToImage(
        nvrhi::IDevice* device,
        donut::engine::CommonRenderPasses* commonPasses,
        nvrhi::ITexture* texture,
        nvrhi::ResourceStates textureState,
        const std::filesystem::path& path,
        bool saveAlphaChannel)
    {
        if (!device || !commonPasses || !texture)
        {
            donut::log::error("SaveTextureToImage: device, common passes and texture are required.");
            return false;
        }

        const std::string fileName = path.string();
        const bool saved = donut::engine::SaveTextureToFile(
            device, commonPasses, texture, textureState, fileName.c_str(), saveAlphaChannel);

        if (!saved)
        {
            donut::log::error("SaveTextureToImage: failed to write %s.", fileName.c_str());
            return false;
        }

        donut::log::info("RenderLab: wrote %s", fileName.c_str());
        return true;
    }

    namespace
    {
        // 把一行像素从半精度浮点转换为 32 位浮点。
        float HalfToFloat(uint16_t value)
        {
            const uint32_t sign = uint32_t(value & 0x8000u) << 16;
            const uint32_t exponent = (value >> 10) & 0x1fu;
            const uint32_t mantissa = value & 0x3ffu;

            uint32_t bits = sign;
            if (exponent == 0)
            {
                if (mantissa == 0)
                {
                    bits = sign;
                }
                else
                {
                    // 次正规数
                    uint32_t e = 127 - 15 + 1;
                    uint32_t m = mantissa;
                    while ((m & 0x400u) == 0)
                    {
                        m <<= 1;
                        --e;
                    }

                    bits |= (e << 23) | ((m & 0x3ffu) << 13);
                }
            }
            else if (exponent == 0x1fu)
            {
                bits |= 0x7f800000u | (mantissa << 13);
            }
            else
            {
                bits |= ((exponent + 127 - 15) << 23) | (mantissa << 13);
            }

            float result = 0.f;
            std::memcpy(&result, &bits, sizeof(result));
            return result;
        }
    }

    float TextureData::FloatAt(uint32_t x, uint32_t y, uint32_t channel) const
    {
        if (x >= size.width || y >= size.height || channel >= channels)
            return 0.f;

        const uint8_t* pixel = bytes.data() + size_t(y) * rowPitch + size_t(x) * GetBytesPerPixel(format);

        switch (format)
        {
        case PixelFormat::RGBA32_FLOAT:
            return reinterpret_cast<const float*>(pixel)[channel];
        case PixelFormat::R32_FLOAT:
            return reinterpret_cast<const float*>(pixel)[0];
        case PixelFormat::RG16_FLOAT:
        case PixelFormat::R16_FLOAT:
        case PixelFormat::RGBA16_FLOAT:
            return HalfToFloat(reinterpret_cast<const uint16_t*>(pixel)[channel]);
        default:
            return 0.f;
        }
    }

    dm::float4 TextureData::ColorAt(uint32_t x, uint32_t y) const
    {
        if (x >= size.width || y >= size.height)
            return dm::float4(0.f);

        const uint8_t* pixel = bytes.data() + size_t(y) * rowPitch + size_t(x) * GetBytesPerPixel(format);

        switch (format)
        {
        case PixelFormat::RGBA32_FLOAT:
        {
            const float* values = reinterpret_cast<const float*>(pixel);
            return dm::float4(values[0], values[1], values[2], values[3]);
        }
        case PixelFormat::RGBA16_FLOAT:
            return dm::float4(
                HalfToFloat(reinterpret_cast<const uint16_t*>(pixel)[0]),
                HalfToFloat(reinterpret_cast<const uint16_t*>(pixel)[1]),
                HalfToFloat(reinterpret_cast<const uint16_t*>(pixel)[2]),
                HalfToFloat(reinterpret_cast<const uint16_t*>(pixel)[3]));
        case PixelFormat::R32_FLOAT:
        {
            const float value = *reinterpret_cast<const float*>(pixel);
            return dm::float4(value, 0.f, 0.f, 1.f);
        }
        case PixelFormat::R16_FLOAT:
        {
            const float value = HalfToFloat(*reinterpret_cast<const uint16_t*>(pixel));
            return dm::float4(value, 0.f, 0.f, 1.f);
        }
        case PixelFormat::RG16_FLOAT:
            return dm::float4(
                HalfToFloat(reinterpret_cast<const uint16_t*>(pixel)[0]),
                HalfToFloat(reinterpret_cast<const uint16_t*>(pixel)[1]),
                0.f, 1.f);
        case PixelFormat::RGBA8_UNORM:
            return dm::float4(pixel[0] / 255.f, pixel[1] / 255.f, pixel[2] / 255.f, pixel[3] / 255.f);
        case PixelFormat::R8_UNORM:
            return dm::float4(pixel[0] / 255.f, 0.f, 0.f, 1.f);
        default:
            return dm::float4(0.f);
        }
    }

    Result<TextureData> ReadTexture(nvrhi::IDevice* device, nvrhi::ITexture* texture, PixelFormat format)
    {
        if (!device || !texture)
            return Status::Error(ErrorCode::InvalidArgument, "device and texture are required");

        if (format == PixelFormat::Unknown)
            return Status::Error(ErrorCode::InvalidArgument, "readback format must be specified explicitly");

        const nvrhi::TextureDesc& sourceDesc = texture->getDesc();

        // 只改 staging 需要的东西：拷贝尺寸由 mip/array/format 决定，其余标志保持与源纹理一致。
        // 清掉 isShaderResource 之类的标志会让 D3D12 后端在创建 staging 时崩溃（实测），
        // 所以这里刻意不碰它们，只做最小的规范化。
        nvrhi::TextureDesc stagingDesc = sourceDesc;
        stagingDesc.dimension = nvrhi::TextureDimension::Texture2D;
        stagingDesc.mipLevels = 1;
        stagingDesc.arraySize = 1;
        stagingDesc.isVirtual = false;
        stagingDesc.isTiled = false;
        stagingDesc.debugName = sourceDesc.debugName + "_Readback";

        nvrhi::StagingTextureHandle staging = device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
        if (!staging)
            return Status::Error(ErrorCode::DeviceError, "failed to create a staging texture");

        // 读回走独立命令列表并等待 GPU 完成：只用于截图与验证，不进入每帧路径。
        nvrhi::CommandListHandle commands = device->createCommandList();
        commands->open();
        commands->copyTexture(
            staging,
            nvrhi::TextureSlice().setMipLevel(0).setArraySlice(0),
            texture,
            nvrhi::TextureSlice().setMipLevel(0).setArraySlice(0));
        commands->close();
        device->executeCommandList(commands);
        device->waitForIdle();

        size_t rowPitch = 0;
        const void* mapped = device->mapStagingTexture(staging, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch);
        if (!mapped)
            return Status::Error(ErrorCode::DeviceError, "failed to map the staging texture");

        TextureData data;
        data.size = Extent2D{ sourceDesc.width, sourceDesc.height };
        data.format = format;
        data.channels = GetChannelCount(format);
        data.rowPitch = uint32_t(rowPitch);

        const uint32_t bytesPerPixel = GetBytesPerPixel(format);
        if (bytesPerPixel == 0)
        {
            device->unmapStagingTexture(staging);
            return Status::Error(ErrorCode::Unsupported, "the requested readback format has no linear layout");
        }

        data.bytes.resize(size_t(data.size.width) * data.size.height * bytesPerPixel);
        for (uint32_t y = 0; y < data.size.height; ++y)
        {
            const uint8_t* source = static_cast<const uint8_t*>(mapped) + size_t(y) * data.rowPitch;
            std::memcpy(data.bytes.data() + size_t(y) * data.size.width * bytesPerPixel, source, size_t(data.size.width) * bytesPerPixel);
        }

        device->unmapStagingTexture(staging);

        // 读回后紧凑排列，方便调用方按下标访问
        data.rowPitch = data.size.width * bytesPerPixel;

        return data;
    }

    ImageDifference CompareFloatImages(const TextureData& a, const TextureData& b, uint32_t channelCount, float tolerance)
    {
        ImageDifference difference;

        if (a.size != b.size || a.channels == 0 || b.channels == 0)
            return difference;

        const uint32_t channels = std::min(channelCount, std::min(a.channels, b.channels));
        double sum = 0.0;
        uint64_t samples = 0;

        for (uint32_t y = 0; y < a.size.height; ++y)
        {
            for (uint32_t x = 0; x < a.size.width; ++x)
            {
                bool pixelDiffers = false;

                for (uint32_t channel = 0; channel < channels; ++channel)
                {
                    const float differenceValue = std::fabs(a.FloatAt(x, y, channel) - b.FloatAt(x, y, channel));
                    sum += differenceValue;
                    ++samples;

                    if (differenceValue > tolerance)
                        pixelDiffers = true;

                    difference.maxAbsolute = std::max(difference.maxAbsolute, differenceValue);
                }

                if (pixelDiffers)
                    ++difference.differingPixels;
            }
        }

        if (samples > 0)
            difference.meanAbsolute = float(sum / double(samples));

        return difference;
    }
}
