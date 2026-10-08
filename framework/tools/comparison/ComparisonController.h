#pragma once
#include <framework/tools/comparison/ComparisonPass.h>
#include <memory>
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
			Pass = std::make_unique<Gpu::FComparisonPass>();
			const FStatus Status = Pass->Initialize(Device, Shaders, CommonPasses);
			bAvailable = Status.IsOk();
			StatusMessage = bAvailable ? std::string() : "Comparison unavailable: " + Status.ToStringWithCode();
			if (!bAvailable)
				Pass.reset();
			return Status;
		}
		[[nodiscard]] bool IsAvailable() const
		{
			return bAvailable;
		}
		void BeginFrame()
		{
			PublishedSources.clear();
		}
		void Publish(std::string Id, Gpu::FComparisonImage Image);
		void RequestFreeze()
		{
			bFreezeRequested = bAvailable;
		}
		void ClearFrozen()
		{
			if (Pass)
				Pass->ClearFrozen();
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
		std::unique_ptr<Gpu::FComparisonPass> Pass;
		std::vector<FSource> PublishedSources;
		bool bAvailable = false;
		bool bFreezeRequested = false;
		std::string StatusMessage;
	};
} // namespace Prism::Host
