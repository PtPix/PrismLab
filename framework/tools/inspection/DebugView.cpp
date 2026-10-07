#include "DebugView.h"
using namespace donut::math;
#include "shaders/DebugView_cb.h"
static_assert(sizeof(FDebugViewConstants) == 32);
namespace Prism::Gpu
{
	bool FDebugViewPass::Initialize(nvrhi::IDevice* Device, FShaderLibrary& Shaders)
	{
		if (!Device || !Constants.Initialize(Device, sizeof(FDebugViewConstants), "DebugView.Constants"))
			return false;
		PointClampSampler = Device->createSampler(
			nvrhi::SamplerDesc().setAllFilters(false).setAllAddressModes(nvrhi::SamplerAddressMode::Clamp));
		nvrhi::BindingLayoutDesc LayoutDescription;
		LayoutDescription.visibility = nvrhi::ShaderType::Pixel;
		LayoutDescription.bindings = {nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
									  nvrhi::BindingLayoutItem::Texture_SRV(0), nvrhi::BindingLayoutItem::Sampler(0)};
		Layout = Device->createBindingLayout(LayoutDescription);
		nvrhi::GraphicsPipelineDesc PipelineDescription;
		PipelineDescription.bindingLayouts = {Layout};
		PipelineDescription.primType = nvrhi::PrimitiveType::TriangleStrip;
		PipelineDescription.renderState.depthStencilState.setDepthTestEnable(false).setDepthWriteEnable(false);
		auto Status = Pass.Initialize(Device, Shaders, PipelineDescription,
									  {{"prism/PrismTools/DebugView.hlsl", "main_vs", nvrhi::ShaderType::Vertex, {}},
									   {"prism/PrismTools/DebugView.hlsl", "main_ps", nvrhi::ShaderType::Pixel, {}}});
		return bReady = PointClampSampler && Layout && bool(Status);
	}
	bool FDebugViewPass::Render(nvrhi::ICommandList* Commands, nvrhi::ITexture* Source, nvrhi::IFramebuffer* Target,
								const FDebugViewSettings& Settings)
	{
		if (!bReady || !Commands || !Source || !Target)
			return false;
		const auto& D = Source->getDesc();
		FDebugViewConstants ConstantsData{};
		ConstantsData.InverseSize = dm::float2(1.f / D.width, 1.f / D.height);
		ConstantsData.Mode = int(Settings.Mode);
		ConstantsData.Scale = Settings.Scale;
		ConstantsData.Bias = Settings.Bias;
		Constants.Write(Commands, ConstantsData);
		nvrhi::BindingSetDesc Bindings;
		Bindings.bindings = {nvrhi::BindingSetItem::ConstantBuffer(0, Constants.Get()),
							 nvrhi::BindingSetItem::Texture_SRV(0, Source),
							 nvrhi::BindingSetItem::Sampler(0, PointClampSampler)};
		nvrhi::GraphicsState State;
		State.framebuffer = Target;
		State.bindings = {Pass.GetOrCreateBindingSet(Bindings, Layout)};
		return bool(Pass.Draw(Commands, State, nvrhi::DrawArguments().setVertexCount(4)));
	}
} // namespace Prism::Gpu
