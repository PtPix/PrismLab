#pragma once
#include <framework/render/data/FrameInfo.h>
#include <framework/render/data/CameraData.h>
#include <nvrhi/nvrhi.h>
namespace donut::engine { class IView; }
namespace prism::host
{
    struct ExperimentFrame
    {
        nvrhi::ICommandList* commands = nullptr;

        prism::FrameInfo frame;
        prism::CameraData camera;

        donut::engine::IView* view = nullptr;
        donut::engine::IView* previousView = nullptr;

        Extent2D renderSize;
        Extent2D outputSize;
    };

}
