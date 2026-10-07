#pragma once
#include <framework/render/data/FrameInfo.h>
#include <framework/render/data/CameraData.h>
#include <nvrhi/nvrhi.h>
namespace donut::engine
{
	class IView;
}
namespace Prism::Host
{
	struct FExperimentFrame
	{
		nvrhi::ICommandList* Commands = nullptr;

		Prism::FFrameInfo Frame;
		Prism::FCameraData Camera;

		donut::engine::IView* View = nullptr;
		donut::engine::IView* PreviousView = nullptr;

		FExtent2D RenderSize;
		FExtent2D OutputSize;
	};

} // namespace Prism::Host
