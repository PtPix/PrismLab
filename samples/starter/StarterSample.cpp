#include <framework/app/Experiment.h>
#include <framework/render/passes/FullscreenPass.h>
namespace prism::samples
{
    class StarterSample final : public host::Experiment
    {
        gpu::FullscreenPass m_Pass;
        gpu::PassConstants m_Constants;
        gpu::TextureRequest m_Output;
        nvrhi::BindingLayoutHandle m_Layout;
        struct Constants { dm::float2 size; float time, padding; };
    public:
        const char* GetName() const override { return "StarterSample"; }
        const char* GetDescription() const override { return "A scene-free fullscreen pass. Edit Gradient.hlsl and press F6."; }
        Status Initialize(host::ExperimentContext& context) override
        {
            m_Output.name = "Starter.Output";
            m_Output.format = PixelFormat::RGBA16_FLOAT;
            context.output.colorSpace = ColorSpace::DisplayEncoded;
            if (!m_Constants.Initialize(context.gpu.device, sizeof(Constants), "Starter.Constants"))
                return Status::Error(ErrorCode::DeviceError, "constant allocation failed");
            nvrhi::BindingLayoutDesc layout; layout.visibility = nvrhi::ShaderType::Pixel;
            layout.bindings = {nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)};
            m_Layout = context.gpu.device->createBindingLayout(layout);
            return m_Pass.Initialize(context.gpu.device, *context.gpu.shaders, *context.gpu.commonPasses,
                {"prism/PrismStarter/Gradient.hlsl", "main_ps", nvrhi::ShaderType::Pixel, {}}, {m_Layout});
        }
        nvrhi::ITexture* Render(host::ExperimentContext& context, const host::ExperimentFrame& frame) override
        {
            auto* output = context.gpu.targets->GetOrCreate(m_Output);
            if (!output) return nullptr;
            m_Constants.Write(frame.commands, Constants{dm::float2(float(frame.renderSize.width), float(frame.renderSize.height)), frame.frame.timeSeconds, 0});
            nvrhi::BindingSetDesc bindings;
            bindings.bindings = {nvrhi::BindingSetItem::ConstantBuffer(0, m_Constants.Get())};
            auto status = m_Pass.Record(frame.commands, context.gpu.targets->GetFramebuffer(output), {m_Pass.Bindings(bindings, m_Layout)});
            return status ? output : nullptr;
        }
        void OnResize(host::ExperimentContext&, const Extent2D&, const Extent2D&) override { m_Pass.ClearBindings(); }
    };
}
namespace prism::host
{
    std::unique_ptr<Experiment> CreateExperiment() { return std::make_unique<samples::StarterSample>(); }
}
