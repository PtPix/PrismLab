#pragma once
#include <framework/render/RenderServices.h>
#include <framework/render/data/FrameInfo.h>
namespace Prism::Gpu
{
	class ITemporalServices
	{
	  public:
		virtual ~ITemporalServices() = default;

		virtual FStatus Initialize(FRenderServices& Services) = 0;

		virtual void ResetHistory(const char* Owner, Prism::FViewId ViewId, Prism::EHistoryResetReason Reason) = 0;

		virtual bool WasResetThisFrame(const char* Owner, Prism::FViewId ViewId) const = 0;

		virtual uint32_t SampleIndex(Prism::FViewId ViewId, uint32_t SampleCount) = 0;

		virtual uint32_t PixelSeed(dm::uint2 Pixel, uint32_t Stream) const = 0;

		virtual void OnRenderSizeChanged(const FExtent2D& RenderSize) = 0;

		virtual void BeginFrame(const FFrameInfo& Frame) = 0;
		virtual void EndFrame() = 0;
	};
} // namespace Prism::Gpu
