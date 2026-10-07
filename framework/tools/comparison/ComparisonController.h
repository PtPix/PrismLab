#pragma once
#include <framework/tools/comparison/ComparisonPass.h>
#include <string>
#include <vector>

namespace Prism::Host
{
	class FComparisonController
	{
	  public:
		struct FSource
		{
			std::string Id;
			Gpu::FComparisonImage Image;
		};
		Gpu::FComparisonSettings Settings;
		std::string SourceA = "Output", SourceB = "Output";
		bool bUseFrozenB = false;
		FStatus Initialize(nvrhi::IDevice* Device, Gpu::FShaderLibrary& Shaders,
						   donut::engine::CommonRenderPasses& CommonPasses)
		{
			return Pass.Initialize(Device, Shaders, CommonPasses);
		}
		void BeginFrame()
		{
			PublishedSources.clear();
		}
		void Publish(std::string Id, Gpu::FComparisonImage Image);
		void RequestFreeze()
		{
			bFreezeRequested = true;
		}
		void ClearFrozen()
		{
			Pass.ClearFrozen();
			bUseFrozenB = false;
		}
		Gpu::FComparisonImage Record(nvrhi::ICommandList* Commands, Gpu::FComparisonImage Fallback);
		const std::vector<FSource>& GetSources() const
		{
			return PublishedSources;
		}
		const std::string& GetMessage() const
		{
			return StatusMessage;
		}

	  private:
		Gpu::FComparisonImage Find(const std::string& Id) const;
		Gpu::FComparisonPass Pass;
		std::vector<FSource> PublishedSources;
		bool bFreezeRequested = false;
		std::string StatusMessage;
	};
} // namespace Prism::Host
