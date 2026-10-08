#pragma once

#include <framework/scene/SurfaceData.h>
#include <framework/scene/SceneFrameData.h>
#include <framework/render/data/CameraData.h>
#include <framework/scene/SceneGpuData.h>
#include <array>
#include <algorithm>

namespace Prism::Pipeline
{
	enum class ESurfaceChannel : uint32_t
	{
		Depth,
		NormalRoughness,
		BaseColorMetalness,
		Emissive,
		Motion,
		InstanceId,
		MaterialId,
		Count
	};
	enum class EMotionUnits
	{
		UV,
		Pixels
	};

	struct FSurfaceRequirements
	{
		uint32_t Mask = 0;
		void Require(ESurfaceChannel Channel)
		{
			Mask |= 1u << uint32_t(Channel);
		}
		bool Requires(ESurfaceChannel Channel) const
		{
			return (Mask & (1u << uint32_t(Channel))) != 0;
		}
	};

	struct FSurfaceTextureView
	{
		nvrhi::ITexture* Texture = nullptr;
		uint32_t Mip = 0;
		uint32_t Slice = 0;
		uint64_t Generation = 0;
	};

	struct FSceneSurfaceData
	{
		std::array<FSurfaceTextureView, size_t(ESurfaceChannel::Count)> Channels{};
		FExtent2D Size;
		FGBufferSchema Schema;
		EDepthConvention DepthConvention = EDepthConvention::ForwardZ0To1;
		EMotionUnits MotionUnits = EMotionUnits::UV;
		// Motion is previous minus current, excluding jitter.
		bool bMotionIncludesJitter = false;

		FSurfaceTextureView& operator[](ESurfaceChannel C)
		{
			return Channels[size_t(C)];
		}
		const FSurfaceTextureView& operator[](ESurfaceChannel C) const
		{
			return Channels[size_t(C)];
		}

		FStatus Validate(const FSurfaceRequirements& Requirements, const FCameraData& Camera) const
		{
			if (Requirements.Requires(ESurfaceChannel::Depth) && DepthConvention != Camera.DepthConvention)
				return FStatus::Error(EErrorCode::InvalidArgument, "surface depth convention differs from camera");
			return Validate(Requirements);
		}

		FStatus Validate(const FSurfaceRequirements& Requirements) const
		{
			if (!Size.IsValid())
				return FStatus::Error(EErrorCode::ExtentMismatch, "surface extent is empty");
			for (size_t I = 0; I < Channels.size(); ++I)
			{
				if (!Requirements.Requires(ESurfaceChannel(I)))
					continue;
				const auto& View = Channels[I];
				if (!View.Texture)
					return FStatus::Error(EErrorCode::ResourceMissing, "required surface channel " + std::to_string(I));
				const auto& D = View.Texture->getDesc();
				if (View.Mip >= D.mipLevels || View.Mip >= 32 || View.Slice >= D.arraySize || D.sampleCount != 1)
					return FStatus::Error(EErrorCode::Unsupported,
										  "surface requires a valid single-sample subresource");
				if (std::max(1u, D.width >> View.Mip) != Size.Width ||
					std::max(1u, D.height >> View.Mip) != Size.Height)
					return FStatus::Error(EErrorCode::ExtentMismatch, "surface channel extent differs");
			}
			return FStatus::Ok();
		}
	};

	struct FSurfaceFrame
	{
		const FFrameInfo& Frame;
		const FCameraData& Camera;
		const FSceneFrameData& Scene;
		const Gpu::FSceneGpuData& GpuScene;
	};

	// Implemented by the application. Outputs are supplied and owned by the caller.
	class ISceneSurfacePipeline
	{
	  public:
		virtual ~ISceneSurfacePipeline() = default;
		virtual FStatus Record(nvrhi::ICommandList* Commands, const FSurfaceFrame& Frame,
							   const FSurfaceRequirements& Requirements, const FSceneSurfaceData& Outputs) = 0;
	};
} // namespace Prism::Pipeline
