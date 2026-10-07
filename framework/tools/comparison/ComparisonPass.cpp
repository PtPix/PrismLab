#include "ComparisonPass.h"
#include <algorithm>
#include <cmath>

namespace Prism::Gpu
{
	namespace
	{
		bool Valid(nvrhi::ITexture* Texture)
		{
			if (!Texture)
				return false;
			const auto& D = Texture->getDesc();
			return D.dimension == nvrhi::TextureDimension::Texture2D && D.sampleCount == 1 && D.isShaderResource;
		}
	} // namespace
	FStatus FComparisonPass::Initialize(nvrhi::IDevice* InDevice, FShaderLibrary& InShaders,
										donut::engine::CommonRenderPasses& InCommonPasses)
	{
		Device = InDevice;
		CommonPasses = &InCommonPasses;
		BlitBindings = std::make_unique<donut::engine::BindingCache>(Device);
		if (!Constants.Initialize(Device, 16, "Comparison.Constants"))
			return FStatus::Error(EErrorCode::DeviceError, "comparison constants failed");
		nvrhi::BindingLayoutDesc LayoutDescription;
		LayoutDescription.visibility = nvrhi::ShaderType::Pixel;
		LayoutDescription.bindings = {nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
									  nvrhi::BindingLayoutItem::Texture_SRV(0),
									  nvrhi::BindingLayoutItem::Texture_SRV(1)};
		Layout = Device->createBindingLayout(LayoutDescription);
		if (!Layout)
			return FStatus::Error(EErrorCode::DeviceError, "comparison layout failed");
		return DifferencePass.Initialize(Device, InShaders, InCommonPasses,
										 {"prism/PrismTools/Difference.hlsl", "main_ps", nvrhi::ShaderType::Pixel, {}},
										 {Layout});
	}
	FStatus FComparisonPass::Freeze(nvrhi::ICommandList* Commands, FComparisonImage Image)
	{
		if (!Commands || !Valid(Image.Texture))
			return FStatus::Error(EErrorCode::InvalidArgument, "freeze requires a 2D single-sample SRV");
		auto Desc = Image.Texture->getDesc();
		Desc.debugName = "Comparison.Frozen";
		Desc.mipLevels = 1;
		Desc.isVirtual = false;
		Desc.sharedResourceFlags = nvrhi::SharedResourceFlags::None;
		Desc.initialState = nvrhi::ResourceStates::ShaderResource;
		Desc.keepInitialState = true;
		auto NewFrozenTexture = Device->createTexture(Desc);
		if (!NewFrozenTexture)
			return FStatus::Error(EErrorCode::DeviceError, "freeze allocation failed");
		Commands->copyTexture(NewFrozenTexture, nvrhi::TextureSlice(), Image.Texture, nvrhi::TextureSlice());
		FrozenTexture = NewFrozenTexture;
		FrozenColorSpace = Image.ColorSpace;
		BlitBindings->Clear();
		DifferencePass.ClearBindings();
		return FStatus::Ok();
	}
	FStatus FComparisonPass::Record(nvrhi::ICommandList* Commands, FComparisonImage A, FComparisonImage B,
									const FComparisonSettings& Settings)
	{
		if (!Commands || !Valid(A.Texture) || !Valid(B.Texture) || !std::isfinite(Settings.Split) ||
			!std::isfinite(Settings.Gain))
			return FStatus::Error(EErrorCode::InvalidArgument,
								  "comparison requires two 2D single-sample SRVs and finite settings");
		const auto& Da = A.Texture->getDesc();
		const auto& Db = B.Texture->getDesc();
		if (Da.width != Db.width || Da.height != Db.height)
			return FStatus::Error(EErrorCode::ExtentMismatch, "comparison inputs must have equal extents");
		if (A.ColorSpace != B.ColorSpace)
			return FStatus::Error(EErrorCode::FormatMismatch, "comparison inputs must use the same color space");
		if (A.Texture == OutputTexture || B.Texture == OutputTexture)
			return FStatus::Error(EErrorCode::InvalidArgument, "comparison output cannot be an input");
		if (!OutputTexture || OutputTexture->getDesc().width != Da.width ||
			OutputTexture->getDesc().height != Da.height)
		{
			nvrhi::TextureDesc Desc;
			Desc.width = Da.width;
			Desc.height = Da.height;
			Desc.format = nvrhi::Format::RGBA16_FLOAT;
			Desc.isRenderTarget = true;
			Desc.initialState = nvrhi::ResourceStates::RenderTarget;
			Desc.keepInitialState = true;
			Desc.debugName = "Comparison.Output";
			OutputTexture = Device->createTexture(Desc);
			if (!OutputTexture)
				return FStatus::Error(EErrorCode::DeviceError, "comparison output allocation failed");
			Framebuffer = Device->createFramebuffer(nvrhi::FramebufferDesc().addColorAttachment(OutputTexture));
			BlitBindings->Clear();
			DifferencePass.ClearBindings();
		}
		if (!Framebuffer)
			return FStatus::Error(EErrorCode::DeviceError, "comparison framebuffer failed");
		if (Settings.Mode == EComparisonMode::Difference)
		{
			struct FConstants
			{
				float Gain;
				float Pad[3];
			} ConstantsData{Settings.Gain, {0, 0, 0}};
			Constants.Write(Commands, ConstantsData);
			nvrhi::BindingSetDesc Bindings;
			Bindings.bindings = {nvrhi::BindingSetItem::ConstantBuffer(0, Constants.Get()),
								 nvrhi::BindingSetItem::Texture_SRV(0, A.Texture),
								 nvrhi::BindingSetItem::Texture_SRV(1, B.Texture)};
			return DifferencePass.Record(Commands, Framebuffer,
										 {DifferencePass.GetOrCreateBindingSet(Bindings, Layout)});
		}
		auto Blit = [&](nvrhi::ITexture* Texture, float Left, float Right, bool bCrop)
		{
			if (Right <= Left)
				return;
			donut::engine::BlitParameters P;
			P.sourceTexture = Texture;
			P.targetFramebuffer = Framebuffer;
			P.targetBox = dm::box2(dm::float2(Left, 0), dm::float2(Right, 1));
			if (bCrop)
				P.sourceBox = P.targetBox;
			CommonPasses->BlitTexture(Commands, P, BlitBindings.get());
		};
		const float Split = std::clamp(Settings.Split, 0.f, 1.f);
		if (Settings.Mode == EComparisonMode::A || Settings.Mode == EComparisonMode::Off)
			Blit(A.Texture, 0, 1, false);
		else if (Settings.Mode == EComparisonMode::B)
			Blit(B.Texture, 0, 1, false);
		else
		{
			Blit(A.Texture, 0, Split, Settings.Mode == EComparisonMode::Wipe);
			Blit(B.Texture, Split, 1, Settings.Mode == EComparisonMode::Wipe);
		}
		return FStatus::Ok();
	}
} // namespace Prism::Gpu
