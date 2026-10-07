#include <framework/app/Experiment.h>
#include <framework/render/passes/FullscreenPass.h>
namespace Prism::Samples
{
	class FStarterSample final : public Host::IExperiment
	{
	  public:
		const char* GetName() const override
		{
			return "StarterSample";
		}
		const char* GetDescription() const override
		{
			return "A scene-free fullscreen pass. Edit Gradient.hlsl and press F6.";
		}
		FStatus Initialize(Host::FExperimentContext& Context) override
		{
			OutputRequest.Name = "Starter.Output";
			OutputRequest.Format = EPixelFormat::RgbA16Float;
			Context.Output.ColorSpace = EColorSpace::DisplayEncoded;
			if (!ConstantBuffer.Initialize(Context.Gpu.Device, sizeof(FShaderConstants), "Starter.Constants"))
				return FStatus::Error(EErrorCode::DeviceError, "constant allocation failed");

			nvrhi::BindingLayoutDesc LayoutDescription;
			LayoutDescription.visibility = nvrhi::ShaderType::Pixel;
			LayoutDescription.bindings = {nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)};
			BindingLayout = Context.Gpu.Device->createBindingLayout(LayoutDescription);
			return Pass.Initialize(Context.Gpu.Device, *Context.Gpu.Shaders, *Context.Gpu.CommonPasses,
								   {"prism/PrismStarter/Gradient.hlsl", "main_ps", nvrhi::ShaderType::Pixel, {}},
								   {BindingLayout});
		}

		nvrhi::ITexture* Render(Host::FExperimentContext& Context, const Host::FExperimentFrame& Frame) override
		{
			nvrhi::ITexture* OutputTexture = Context.Gpu.Targets->GetOrCreate(OutputRequest);
			if (!OutputTexture)
				return nullptr;

			const FShaderConstants ShaderConstants{
				dm::float2(float(Frame.RenderSize.Width), float(Frame.RenderSize.Height)), Frame.Frame.TimeSeconds,
				0.f};
			ConstantBuffer.Write(Frame.Commands, ShaderConstants);

			nvrhi::BindingSetDesc Bindings;
			Bindings.bindings = {nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer.Get())};
			const FStatus Status = Pass.Record(Frame.Commands, Context.Gpu.Targets->GetFramebuffer(OutputTexture),
											   {Pass.GetOrCreateBindingSet(Bindings, BindingLayout)});
			return Status ? OutputTexture : nullptr;
		}

		void OnResize(Host::FExperimentContext&, const FExtent2D&, const FExtent2D&) override
		{
			Pass.ClearBindings();
		}

	  private:
		struct FShaderConstants
		{
			dm::float2 Size;
			float Time;
			float Padding;
		};

		Gpu::FFullscreenPass Pass;
		Gpu::FPassConstants ConstantBuffer;
		Gpu::FTextureRequest OutputRequest;
		nvrhi::BindingLayoutHandle BindingLayout;
	};
} // namespace Prism::Samples
namespace Prism::Host
{
	std::unique_ptr<IExperiment> CreateExperiment()
	{
		return std::make_unique<Samples::FStarterSample>();
	}
} // namespace Prism::Host
