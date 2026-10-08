#pragma once
#include <framework/render/RenderServices.h>
#include <framework/scene/SceneStats.h>
#include <framework/render/presentation/DisplayChain.h>
#include <framework/render/temporal/TemporalServices.h>
#include "HostConfig.h"
#include <filesystem>
#include <functional>
namespace Prism::Host
{
	class FComparisonController;
	class FReplayController;
	class FDebugViewRegistry;
	class FMetrics;

	struct FExperimentCallbacks
	{

		std::function<void(Prism::EHistoryResetReason)> RequestHistoryReset;
		std::function<void(Prism::EDepthConvention)> SetPrimaryDepthConvention;

		std::function<bool(nvrhi::ITexture*, const std::filesystem::path&, nvrhi::ResourceStates)> SaveTexture;

		std::function<void()> RequestQuit;
	};

	struct FExperimentContext
	{

		struct FSceneServices
		{
			FSceneStats Stats;
			std::string Description = "(none)";
		};

		struct FOutputServices
		{
			Gpu::IDisplayChain* DisplayChain = nullptr;
			EColorSpace ColorSpace = EColorSpace::SceneLinear;
		};
		struct FToolServices
		{
			FComparisonController* Comparison = nullptr;
			FReplayController* Replay = nullptr;
			FDebugViewRegistry* DebugViews = nullptr;
			FMetrics* Metrics = nullptr;
		};
		struct FTemporalState
		{
			Gpu::ITemporalServices* Services = nullptr;
			uint32_t JitterSampleCount = 0;
		};
		Gpu::FRenderServices Gpu;
		FSceneServices Scene;
		FOutputServices Output;
		FToolServices Tools;
		FTemporalState Temporal;
		FExperimentCallbacks Callbacks;

		const Host::FHostConfig* Config = nullptr;
		std::filesystem::path AssetsDirectory;
	};

} // namespace Prism::Host
