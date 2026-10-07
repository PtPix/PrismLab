#pragma once
#include <framework/render/RenderServices.h>
#include <framework/render/data/ColorSpace.h>
namespace Prism::Gpu
{
	struct FDisplayInput
	{
		nvrhi::ITexture* SceneColor = nullptr;
		EColorSpace ColorSpace = EColorSpace::SceneLinear;
		nvrhi::IFramebuffer* OutputTarget = nullptr;
		FExtent2D OutputSize;
		float DeltaTimeSeconds = 0.f;
		uint64_t FrameIndex = 0;
	};
	// The sample owns display algorithms and their UI. Targets may be offscreen.
	class IDisplayChain
	{
	  public:
		virtual ~IDisplayChain() = default;
		virtual FStatus Initialize(FRenderServices& Services) = 0;
		virtual FStatus Record(FRenderServices& Services, nvrhi::ICommandList* Commands,
							   const FDisplayInput& Input) = 0;
		virtual void OnOutputResized(FRenderServices& Services, FExtent2D Size) = 0;
	};
} // namespace Prism::Gpu
