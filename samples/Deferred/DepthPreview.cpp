#include "DepthPreview.h"

using donut::math::float2;
#include "DepthDebugCb.h"

namespace dm = donut::math;

namespace Prism::Samples
{
	static_assert(sizeof(FDepthDebugConstants) == 32, "depth debug constants layout changed");

	FStatus FDepthPreview::Initialize(Gpu::FRenderServices& Gpu)
	{
		if (!Gpu.Device || !Gpu.Shaders || !Gpu.Targets || !Gpu.CommonPasses)
			return FStatus::Error(EErrorCode::NotInitialized, "depth preview needs GPU services");

		OutputRequest.Name = "Deferred.DepthPreview";
		OutputRequest.Format = EPixelFormat::RgbA16Float;
		OutputRequest.Usage = Gpu::ETextureUsage::RenderTarget | Gpu::ETextureUsage::ShaderResource;
		if (!Constants.Initialize(Gpu.Device, sizeof(FDepthDebugConstants), "Deferred.DepthDebug"))
			return FStatus::Error(EErrorCode::DeviceError, "depth debug constants allocation failed");

		nvrhi::BindingLayoutDesc Desc;
		Desc.visibility = nvrhi::ShaderType::Pixel;
		Desc.bindings = {nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
						 nvrhi::BindingLayoutItem::Texture_SRV(0), nvrhi::BindingLayoutItem::Sampler(0)};
		Layout = Gpu.Device->createBindingLayout(Desc);
		if (!Layout)
			return FStatus::Error(EErrorCode::PipelineCreationFailed, "depth debug binding layout failed");
		return Pass.Initialize(Gpu.Device, *Gpu.Shaders, *Gpu.CommonPasses,
			{"prism/PrismDeferred/DepthDebug.hlsl", "main_ps", nvrhi::ShaderType::Pixel, {}}, {Layout});
	}

	nvrhi::ITexture* FDepthPreview::Record(Gpu::FRenderServices& Gpu, nvrhi::ICommandList* Commands,
											 nvrhi::ITexture* Depth, const FCameraData& Camera, FExtent2D Size, int Mode)
	{
		if (!Depth || !Commands || !Size.IsValid())
			return nullptr;
		auto* Output = Gpu.Targets->GetOrCreate(OutputRequest);
		auto* Target = Output ? Gpu.Targets->GetFramebuffer(Output) : nullptr;
		if (!Target)
			return nullptr;

		const FDepthDebugConstants Data = {
			dm::float2(1.f / float(Size.Width), 1.f / float(Size.Height)), Camera.ZNearMeters,
			Camera.ZFarMeters, int(Camera.DepthConvention), Mode, dm::float2(0.f)};
		Constants.Write(Commands, Data);
		nvrhi::BindingSetDesc Bindings;
		Bindings.bindings = {nvrhi::BindingSetItem::ConstantBuffer(0, Constants.Get()),
							 nvrhi::BindingSetItem::Texture_SRV(0, Depth),
							 nvrhi::BindingSetItem::Sampler(0, Gpu.CommonPasses->m_PointClampSampler)};
		const auto Set = Pass.GetOrCreateBindingSet(Bindings, Layout);
		return Set && Pass.Record(Commands, Target, {Set}) ? Output : nullptr;
	}
} // namespace Prism::Samples
