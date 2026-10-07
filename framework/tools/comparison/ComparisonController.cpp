#include "ComparisonController.h"

namespace Prism::Host
{
	void FComparisonController::Publish(std::string Id, Gpu::FComparisonImage Image)
	{
		for (FSource& Source : PublishedSources)
			if (Source.Id == Id)
			{
				Source.Image = Image;
				return;
			}
		PublishedSources.push_back({std::move(Id), Image});
	}
	Gpu::FComparisonImage FComparisonController::Find(const std::string& Id) const
	{
		for (const FSource& Source : PublishedSources)
			if (Source.Id == Id)
				return Source.Image;
		return {};
	}
	Gpu::FComparisonImage FComparisonController::Record(nvrhi::ICommandList* Commands, Gpu::FComparisonImage Fallback)
	{
		Publish("Output", Fallback);
		StatusMessage.clear();
		auto ImageA = Find(SourceA), ImageB = Find(SourceB);
		if (bFreezeRequested)
		{
			bFreezeRequested = false;
			auto Status = Pass.Freeze(Commands, ImageB);
			if (!Status)
				StatusMessage = Status.ToStringWithCode();
			else
				bUseFrozenB = true;
		}
		if (Settings.Mode == Gpu::EComparisonMode::Off)
			return Fallback;
		if (bUseFrozenB)
			ImageB = Pass.GetFrozenImage();
		auto Status = Pass.Record(Commands, ImageA, ImageB, Settings);
		if (!Status)
		{
			StatusMessage = Status.ToStringWithCode();
			return Fallback;
		}
		return {Pass.GetOutputTexture(),
				Settings.Mode == Gpu::EComparisonMode::Difference ? EColorSpace::DisplayEncoded : ImageA.ColorSpace};
	}
} // namespace Prism::Host
