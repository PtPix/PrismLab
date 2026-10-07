#pragma once
#include <framework/render/data/CameraPose.h>
#include <framework/core/Status.h>
#include <donut/core/json.h>
#include <filesystem>
#include <vector>

namespace Prism::Host
{
	struct FReplaySample
	{
		FCameraPose Camera;
		Json::Value Parameters;
	};
	struct FReplayTrack
	{
		double FixedDelta = 1.0 / 60.0;
		uint32_t Seed = 1;
		std::vector<FReplaySample> Samples;
		FStatus Save(const std::filesystem::path& Path) const;
		FStatus Load(const std::filesystem::path& Path);
	};
} // namespace Prism::Host
