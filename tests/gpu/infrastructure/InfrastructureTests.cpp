#include <framework/app/Experiment.h>
#include <framework/tools/capture/ImageReference.h>
#include <framework/tools/shaders/ShaderReload.h>
#include <framework/render/passes/ComputePass.h>
#include <framework/render/passes/RasterPass.h>
#include <framework/tools/comparison/ComparisonPass.h>
#include <donut/core/vfs/VFS.h>
#include <donut/core/log.h>
#include <cmath>
#include <fstream>



namespace prism::host
{
    class InfrastructureTests final : public Experiment
    {
        gpu::ComputePass compute;
        gpu::RasterPass raster;
        gpu::ComparisonPass comparison;
        nvrhi::BindingLayoutHandle layout;
        nvrhi::TextureHandle a, b;
        nvrhi::FramebufferHandle framebuffer;
        nvrhi::BufferHandle indices;
        ShaderReload reload;
        ShaderBuildTask failedBuild;
        bool initializationFailure = false;
        bool passed = true, done = false, pending = false, reloading = false, failurePending = false;
        uint32_t round = 0, width = 17, height = 13;
        gpu::ComparisonMode mode = gpu::ComparisonMode::Difference;
        void Check(bool value, const char* name)
        { if (!value) { passed = false; donut::log::error("Infrastructure test failed: %s", name); } }
        void Check(Status value) { Check(bool(value), value.ToStringWithCode().c_str()); }
        void Resize(nvrhi::IDevice* device)
        {
            compute.ClearBindings();
            nvrhi::TextureDesc d; d.width = width; d.height = height; d.format = nvrhi::Format::RGBA32_FLOAT;
            d.isUAV = true; d.isRenderTarget = true; d.keepInitialState = true;
            d.initialState = nvrhi::ResourceStates::ShaderResource; d.debugName = "Test.A";
            a = device->createTexture(d); d.debugName = "Test.B"; b = device->createTexture(d);
            framebuffer = device->createFramebuffer(nvrhi::FramebufferDesc().addColorAttachment(b));
        }
        void Verify(nvrhi::IDevice* device)
        {
            auto output = ReadTextureAsFloat(device, comparison.Output());
            Check(bool(output), "readback"); if (!output) return;
            float maximum = 0;
            for (uint32_t y = 0; y < height; ++y)
                for (uint32_t x = 0; x < width; ++x)
                    for (uint32_t c = 0; c < 4; ++c)
                    {
                        const float av[] = {float(x) / width, float(y) / height, .5f, 1};
                        const float bv[] = {.25f, .5f, .75f, 1};
                        float expected = av[c];
                        if (mode == gpu::ComparisonMode::Difference) expected = c == 3 ? 1 : std::abs(av[c] - bv[c]);
                        if (mode == gpu::ComparisonMode::B || (mode == gpu::ComparisonMode::Wipe && x >= width / 2)) expected = bv[c];
                        maximum = std::max(maximum, std::abs(output.Value().At(x, y, c) - expected));
                    }
            Check(maximum < .001f, "GPU pixels: dispatch, indexed raster, freeze, comparison");
        }
        std::shared_ptr<donut::engine::ShaderFactory> Factory(nvrhi::IDevice* device, bool incompatible)
        {
            auto fs = std::make_shared<donut::vfs::RootFileSystem>();
            const std::filesystem::path root = PRISM_TEST_BIN;
            if (incompatible) fs->mount("/shaders/prism/PrismTestCompute", root / "test-incompatible");
            fs->mount("/shaders/prism", root / "shaders/prism/dxil");
            fs->mount("/shaders/donut", root / "shaders/framework/dxil");
            return std::make_shared<donut::engine::ShaderFactory>(device, fs, "/shaders");
        }
    public:
        const char* GetName() const override { return "InfrastructureTests"; }
        Status Initialize(ExperimentContext& context) override
        {
            initializationFailure = context.config && context.config->sourcePath.stem() == "init-failure";
            if (initializationFailure) return Status::Error(ErrorCode::Internal, "expected initialization failure");

            gpu::TextureSlot first("Repeated", PixelFormat::RGBA16_FLOAT, gpu::TextureUsage::ShaderResource);
            gpu::TextureSlot second("Repeated", PixelFormat::RGBA16_FLOAT, gpu::TextureUsage::ShaderResource);
            auto& resources = *context.gpu.resources;
            Check(resources.Get(first) != resources.Get(second), "same labels have independent identities");
            const auto shared = first;
            Check(resources.Get(shared) == resources.Get(first), "copied resource identity");
            gpu::BufferSlot pixels("Repeated", 16, gpu::BufferUsage::ShaderResource, 1);
            const auto size = resources.Buffers().GetRenderSize();
            resources.SetRenderSize({17,13});
            Check(resources.Get(pixels)->getDesc().byteSize == 17 * 13 * 16, "pixel buffer initial size");
            resources.SetRenderSize({20,12});
            Check(resources.Get(pixels)->getDesc().byteSize == 20 * 12 * 16, "pixel buffer resize");
            resources.SetRenderSize(size);
            nvrhi::BindingLayoutDesc d; d.visibility = nvrhi::ShaderType::Compute;
            d.bindings = {nvrhi::BindingLayoutItem::Texture_UAV(0)};
            layout = context.gpu.device->createBindingLayout(d);
            Check(compute.Initialize(context.gpu.device, *context.gpu.shaders,
                {"prism/PrismTestCompute/TestPasses.hlsl", "main_cs", nvrhi::ShaderType::Compute, {}}, {layout}, dm::uint3(4,4,1)));
            nvrhi::GraphicsPipelineDesc graphics;
            graphics.renderState.depthStencilState.depthTestEnable = false;
            graphics.renderState.depthStencilState.depthWriteEnable = false;
            graphics.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
            Check(raster.Initialize(context.gpu.device, *context.gpu.shaders, graphics, {
                {"prism/PrismInfrastructureTests/TestPasses.hlsl", "main_vs", nvrhi::ShaderType::Vertex, {}},
                {"prism/PrismInfrastructureTests/TestPasses.hlsl", "main_ps", nvrhi::ShaderType::Pixel, {}}}));
            Check(comparison.Initialize(context.gpu.device, *context.gpu.shaders, *context.gpu.commonPasses));
            nvrhi::BufferDesc indexDesc; indexDesc.byteSize = 12; indexDesc.isIndexBuffer = true;
            indexDesc.initialState = nvrhi::ResourceStates::IndexBuffer; indexDesc.keepInitialState = true;
            indices = context.gpu.device->createBuffer(indexDesc);
            Resize(context.gpu.device);
            reload.Initialize(context.gpu.device, *context.gpu.shaders, std::filesystem::path(PRISM_TEST_BIN) / "PrismInfrastructureTests.exe");
            return passed ? Status::Ok() : Status::Error(ErrorCode::Internal, "infrastructure initialization failed");
        }
        void BeginFrame(ExperimentContext& context, const ExperimentFrame& frame) override
        {
            if (frame.frame.submissionIndex > 10000) { Check(false, "reload timeout"); done = true; }
            if (pending) { Verify(context.gpu.device); pending = false; ++round; }
            if (round == 1) { width = 20; height = 12; Resize(context.gpu.device); }
            if (round == 5 && !reloading && !done)
            {
                struct Veto final : gpu::ShaderReloadClient
                {
                    Status PrepareShaders(gpu::ShaderLibrary&) override { return Status::Error(ErrorCode::Internal, "expected test veto"); }
                    void CommitShaders() override {} void DiscardShaders() override {}
                } veto;
                auto* shaders = context.gpu.shaders;
                nvrhi::ComputePipelineHandle original = compute.GetPipeline();
                shaders->Register(&veto); auto status = shaders->Reload(Factory(context.gpu.device, false)); shaders->Unregister(&veto);
                Check(!status && compute.GetPipeline() == original && shaders->GetGeneration() == 0, "transaction rollback");
                auto incompatible = shaders->Reload(Factory(context.gpu.device, true));
                Check(!incompatible && incompatible.ToStringWithCode().find("interface changed") != std::string::npos &&
                    compute.GetPipeline() == original && shaders->GetGeneration() == 0, "thread group change rejected");
                Check(reload.Request(), "start asynchronous compiler"); reloading = true;
            }
            if (reloading)
            {
                bool committed = reload.Poll();
                if (!reload.Running())
                {
                    Check(committed && context.gpu.shaders->GetGeneration() == 1, reload.Message().c_str());
                    reloading = false; ++round;
                }
            }
            if (round == 7 && !failurePending && !done)
            {
                const auto script = std::filesystem::path(PRISM_TEST_BIN) / "expected-failure.cmake";
                const auto source = std::filesystem::path(PRISM_TEST_BIN) / "expected-invalid.hlsl";
                { std::ofstream out(source); out << "This is deliberately invalid HLSL."; }
                {
                    std::ofstream out(script);
                    out << "execute_process(COMMAND \"" << PRISM_TEST_DXC << "\" -T cs_6_5 -E main_cs \""
                        << source.generic_string() << "\" RESULT_VARIABLE result)\n"
                        << "if(NOT result EQUAL 0)\n message(FATAL_ERROR \"Shader compilation failed\")\nendif()\n";
                }
                Check(failedBuild.Start(script, std::filesystem::path(PRISM_TEST_BIN) / "failed-build"), "start failed build");
                failurePending = true;
            }
            if (failurePending && failedBuild.Poll())
            {
                Check(!failedBuild.Succeeded() && context.gpu.shaders->GetGeneration() == 1, "failed build preserves generation");
                done = true; failurePending = false;
            }
            if (!passed) done = true;
            if (done) { donut::log::info("Infrastructure tests %s.", passed ? "passed" : "FAILED"); context.callbacks.requestQuit(); }
        }
        nvrhi::ITexture* Render(ExperimentContext& context, const ExperimentFrame& frame) override
        {
            if (done || reloading || failurePending) return comparison.Output();
            auto* commands = frame.commands;
            const uint32_t indexData[] = {0,1,2}; commands->writeBuffer(indices, indexData, sizeof(indexData));
            nvrhi::BindingSetDesc bindings; bindings.bindings = {nvrhi::BindingSetItem::Texture_UAV(0, a)};
            Check(compute.DispatchExtent(commands, {compute.Bindings(bindings, layout)}, dm::uint3(width,height,1)));
            commands->clearTextureFloat(b, nvrhi::AllSubresources, nvrhi::Color(0));
            nvrhi::GraphicsState state; state.framebuffer = framebuffer; state.indexBuffer = {indices, nvrhi::Format::R32_UINT, 0};
            nvrhi::DrawArguments args; args.vertexCount = 3;
            Check(raster.Draw(commands, state, args, true));
            Check(comparison.Freeze(commands, {b}));
            commands->clearTextureFloat(b, nvrhi::AllSubresources, nvrhi::Color(0));
            mode = round == 2 ? gpu::ComparisonMode::A : round == 3 ? gpu::ComparisonMode::B :
                round == 4 ? gpu::ComparisonMode::Wipe : gpu::ComparisonMode::Difference;
            Check(comparison.Record(commands, {a}, comparison.Frozen(), {mode, .5f, 1}));
            auto invalid = comparison.Record(commands, {a, ColorSpace::SceneLinear}, {b, ColorSpace::DisplayEncoded}, {mode});
            Check(!invalid, "reject mixed color spaces");
            pending = true; (void)context; return comparison.Output();
        }
        void Shutdown(ExperimentContext& context) override
        {
            auto marker = std::filesystem::path(PRISM_TEST_BIN) / (initializationFailure ? "shutdown-failure.txt" : "shutdown-normal.txt");
            // Querying the device also verifies the hook precedes device teardown.
            std::ofstream output(marker);
            output << (context.gpu.device->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D12 ? "device-alive" : "wrong-device");
        }
        bool PassedVerification() const override { return passed && done; }
    };
    std::unique_ptr<Experiment> CreateExperiment() { return std::make_unique<InfrastructureTests>(); }
}
