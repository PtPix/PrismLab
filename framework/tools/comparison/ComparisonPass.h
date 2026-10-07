#pragma once
#include <framework/render/passes/FullscreenPass.h>
#include <framework/render/data/ColorSpace.h>

namespace Prism::Gpu
{
	enum class EComparisonMode
	{
		Off,
		A,
		B,
		SideBySide,
		Wipe,
		Difference
	};
	struct FComparisonImage
	{
		nvrhi::ITexture* Texture = nullptr;
		EColorSpace ColorSpace = EColorSpace::SceneLinear;
	};
	struct FComparisonSettings
	{
		EComparisonMode Mode = EComparisonMode::Off;
		float Split = 0.5f;
		float Gain = 1.f;
	};

	class FComparisonPass
	{
	  public:
		FStatus Initialize(nvrhi::IDevice* InDevice, FShaderLibrary& InShaders,
						   donut::engine::CommonRenderPasses& InCommonPasses);
		FStatus Record(nvrhi::ICommandList* Commands, FComparisonImage A, FComparisonImage B,
					   const FComparisonSettings& Settings);
		FStatus Freeze(nvrhi::ICommandList* Commands, FComparisonImage Image);
		void ClearFrozen()
		{
			FrozenTexture = nullptr;
		}
		FComparisonImage GetFrozenImage() const
		{
			return {FrozenTexture, FrozenColorSpace};
		}
		nvrhi::ITexture* GetOutputTexture() const
		{
			return OutputTexture;
		}

	  private:
		nvrhi::IDevice* Device = nullptr;
		donut::engine::CommonRenderPasses* CommonPasses = nullptr;
		std::unique_ptr<donut::engine::BindingCache> BlitBindings;
		FFullscreenPass DifferencePass;
		FPassConstants Constants;
		nvrhi::BindingLayoutHandle Layout;
		nvrhi::TextureHandle OutputTexture;
		nvrhi::TextureHandle FrozenTexture;
		nvrhi::FramebufferHandle Framebuffer;
		EColorSpace FrozenColorSpace = EColorSpace::SceneLinear;
	};
} // namespace Prism::Gpu
