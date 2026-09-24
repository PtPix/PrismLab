#pragma once
#include <framework/render/RenderServices.h>
#include <framework/render/data/FrameInfo.h>
namespace prism::gpu
{
    class ITemporalServices
    {
    public:
        virtual ~ITemporalServices() = default;

        virtual Status Initialize(RenderServices& services) = 0;

        virtual void ResetHistory(const char* owner, prism::ViewId viewId, prism::HistoryResetReason reason) = 0;

        virtual bool WasResetThisFrame(const char* owner, prism::ViewId viewId) const = 0;

        virtual uint32_t SampleIndex(prism::ViewId viewId, uint32_t sampleCount) = 0;

        virtual uint32_t PixelSeed(dm::uint2 pixel, uint32_t stream) const = 0;

        virtual void OnRenderSizeChanged(const Extent2D& renderSize) = 0;

        virtual void BeginFrame(const FrameInfo& frame) = 0;
        virtual void EndFrame() = 0;
    };
}
