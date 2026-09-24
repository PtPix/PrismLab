#pragma once

#include <framework/scene/SurfaceData.h>
#include <framework/scene/SceneFrameData.h>
#include <framework/render/data/CameraData.h>
#include <framework/scene/SceneGpuData.h>
#include <array>
#include <algorithm>

namespace prism::pipeline
{
    enum class SurfaceChannel : uint32_t
    {
        Depth, NormalRoughness, BaseColorMetalness, Emissive, Motion, InstanceId, MaterialId, Count
    };
    enum class MotionUnits { UV, Pixels };

    struct SurfaceRequirements
    {
        uint32_t mask = 0;
        void Require(SurfaceChannel channel) { mask |= 1u << uint32_t(channel); }
        bool Requires(SurfaceChannel channel) const { return (mask & (1u << uint32_t(channel))) != 0; }
    };

    struct SurfaceTextureView
    {
        nvrhi::ITexture* texture = nullptr;
        uint32_t mip = 0;
        uint32_t slice = 0;
        uint64_t generation = 0;
    };

    struct SceneSurfaceData
    {
        std::array<SurfaceTextureView, size_t(SurfaceChannel::Count)> channels{};
        Extent2D size;
        GBufferSchema schema;
        DepthConvention depthConvention = DepthConvention::ForwardZ0To1;
        MotionUnits motionUnits = MotionUnits::UV;
        // Motion is previous minus current, excluding jitter.
        bool motionIncludesJitter = false;

        SurfaceTextureView& operator[](SurfaceChannel c) { return channels[size_t(c)]; }
        const SurfaceTextureView& operator[](SurfaceChannel c) const { return channels[size_t(c)]; }

        Status Validate(const SurfaceRequirements& requirements) const
        {
            if (!size.IsValid())
                return Status::Error(ErrorCode::ExtentMismatch, "surface extent is empty");
            for (size_t i = 0; i < channels.size(); ++i)
            {
                if (!requirements.Requires(SurfaceChannel(i))) continue;
                const auto& view = channels[i];
                if (!view.texture)
                    return Status::Error(ErrorCode::ResourceMissing, "required surface channel " + std::to_string(i));
                const auto& d = view.texture->getDesc();
                if (view.mip >= d.mipLevels || view.mip >= 32 || view.slice >= d.arraySize || d.sampleCount != 1)
                    return Status::Error(ErrorCode::Unsupported, "surface requires a valid single-sample subresource");
                if (std::max(1u, d.width >> view.mip) != size.width || std::max(1u, d.height >> view.mip) != size.height)
                    return Status::Error(ErrorCode::ExtentMismatch, "surface channel extent differs");
            }
            return Status::Ok();
        }
    };

    struct SurfaceFrame
    {
        const FrameInfo& frame;
        const CameraData& camera;
        const SceneFrameData& scene;
        const gpu::SceneGpuData& gpuScene;
    };

    // Implemented by the application. Outputs are supplied and owned by the caller.
    class ISceneSurfacePipeline
    {
    public:
        virtual ~ISceneSurfacePipeline() = default;
        virtual Status Record(nvrhi::ICommandList* commands, const SurfaceFrame& frame,
            const SurfaceRequirements& requirements, const SceneSurfaceData& outputs) = 0;
    };
}
