#include <framework/app/Experiment.h>
#include <framework/tools/capture/ImageReference.h>
#include <framework/tools/shaders/ShaderReload.h>
#include <framework/render/passes/ComputePass.h>
#include <framework/render/passes/RasterPass.h>
#include <framework/tools/comparison/ComparisonPass.h>
#include <framework/tools/comparison/ComparisonController.h>
#include <donut/core/vfs/VFS.h>
#include <donut/core/log.h>
#include <cmath>
#include <fstream>

namespace Prism::Host
{
	class FInfrastructureTests final : public IExperiment
	{
		Gpu::FComputePass Compute;
		Gpu::FRasterPass Raster;
		Gpu::FComparisonPass Comparison;
		nvrhi::BindingLayoutHandle Layout;
		nvrhi::TextureHandle A, B;
		nvrhi::FramebufferHandle Framebuffer;
		nvrhi::BufferHandle Indices;
		FShaderReload Reload;
		FShaderBuildTask FailedBuild;
		bool bInitializationFailure = false;
		bool bPassed = true, bDone = false, bPending = false, bReloading = false, bFailurePending = false;
		uint32_t Round = 0, Width = 17, Height = 13;
		Gpu::EComparisonMode Mode = Gpu::EComparisonMode::Difference;
		void Check(bool bValue, const char* Name)
		{
			if (!bValue)
			{
				bPassed = false;
				donut::log::error("Infrastructure test failed: %s", Name);
			}
		}
		void Check(FStatus Value)
		{
			Check(bool(Value), Value.ToStringWithCode().c_str());
		}
		void Resize(nvrhi::IDevice* Device)
		{
			Compute.ClearBindings();
			nvrhi::TextureDesc D;
			D.width = Width;
			D.height = Height;
			D.format = nvrhi::Format::RGBA32_FLOAT;
			D.isUAV = true;
			D.isRenderTarget = true;
			D.keepInitialState = true;
			D.initialState = nvrhi::ResourceStates::ShaderResource;
			D.debugName = "Test.A";
			A = Device->createTexture(D);
			D.debugName = "Test.B";
			B = Device->createTexture(D);
			Framebuffer = Device->createFramebuffer(nvrhi::FramebufferDesc().addColorAttachment(B));
		}
		void Verify(nvrhi::IDevice* Device)
		{
			auto Output = ReadTextureAsFloat(Device, Comparison.GetOutputTexture());
			Check(bool(Output), "readback");
			if (!Output)
				return;
			float Maximum = 0;
			for (uint32_t Y = 0; Y < Height; ++Y)
				for (uint32_t X = 0; X < Width; ++X)
					for (uint32_t C = 0; C < 4; ++C)
					{
						const float Av[] = {float(X) / Width, float(Y) / Height, .5f, 1};
						const float Bv[] = {.25f, .5f, .75f, 1};
						float Expected = Av[C];
						if (Mode == Gpu::EComparisonMode::Difference)
							Expected = C == 3 ? 1 : std::abs(Av[C] - Bv[C]);
						if (Mode == Gpu::EComparisonMode::B || (Mode == Gpu::EComparisonMode::Wipe && X >= Width / 2))
							Expected = Bv[C];
						Maximum = std::max(Maximum, std::abs(Output.GetValue().At(X, Y, C) - Expected));
					}
			Check(Maximum < .001f, "GPU pixels: dispatch, indexed raster, freeze, comparison");
		}
		std::shared_ptr<donut::engine::ShaderFactory> Factory(nvrhi::IDevice* Device, bool bIncompatible)
		{
			auto FileSystem = std::make_shared<donut::vfs::RootFileSystem>();
			const std::filesystem::path Root = PRISM_TEST_BIN;
			if (bIncompatible)
				FileSystem->mount("/shaders/prism/PrismTestCompute", Root / "test-incompatible");
			FileSystem->mount("/shaders/prism", Root / "shaders/prism/dxil");
			FileSystem->mount("/shaders/donut", Root / "shaders/framework/dxil");
			return std::make_shared<donut::engine::ShaderFactory>(Device, FileSystem, "/shaders");
		}

	  public:
		const char* GetName() const override
		{
			return "InfrastructureTests";
		}
		FStatus Initialize(FExperimentContext& Context) override
		{
			bInitializationFailure = Context.Config && Context.Config->SourcePath.stem() == "init-failure";
			if (bInitializationFailure)
				return FStatus::Error(EErrorCode::Internal, "expected initialization failure");

			Gpu::FTextureSlot First("Repeated", EPixelFormat::RgbA16Float, Gpu::ETextureUsage::ShaderResource);
			Gpu::FTextureSlot Second("Repeated", EPixelFormat::RgbA16Float, Gpu::ETextureUsage::ShaderResource);
			auto& Resources = *Context.Gpu.Resources;
			Check(Resources.Get(First) != Resources.Get(Second), "same labels have independent identities");
			const auto Shared = First;
			Check(Resources.Get(Shared) == Resources.Get(First), "copied resource identity");
			const Gpu::FComparisonImage Fallback{Resources.Get(First), EColorSpace::DisplayEncoded};
			{
				auto EmptyFiles = std::make_shared<donut::vfs::RootFileSystem>();
				auto EmptyFactory = std::make_shared<donut::engine::ShaderFactory>(Context.Gpu.Device, EmptyFiles,
					"/shaders");
				Gpu::FShaderLibrary MissingShaders(Context.Gpu.Device, EmptyFactory);
				FComparisonController UnavailableComparison;
				const auto Failure = UnavailableComparison.Initialize(Context.Gpu.Device, MissingShaders,
					*Context.Gpu.CommonPasses);
				UnavailableComparison.Publish("source", Fallback);
				UnavailableComparison.RequestFreeze();
				const auto Unchanged = UnavailableComparison.Record(nullptr, Fallback);
				Check(!Failure && !UnavailableComparison.IsAvailable() && UnavailableComparison.GetSources().empty() &&
					  Unchanged.Texture == Fallback.Texture && Unchanged.ColorSpace == Fallback.ColorSpace,
					  "comparison shader failure leaves experiment output unchanged");
			}
			Gpu::FBufferSlot Pixels("Repeated", 16, Gpu::EBufferUsage::ShaderResource, 1);
			const auto Size = Resources.GetBufferCache().GetRenderSize();
			Resources.SetRenderSize({17, 13});
			Check(Resources.Get(Pixels)->getDesc().byteSize == 17 * 13 * 16, "pixel buffer initial size");
			Resources.SetRenderSize({20, 12});
			Check(Resources.Get(Pixels)->getDesc().byteSize == 20 * 12 * 16, "pixel buffer resize");
			Resources.SetRenderSize(Size);
			Check(Resources.Get(Pixels)->getDesc().canHaveRawViews == false,
				  "structured buffers do not need raw views");
			Gpu::FBufferSlot Raw("Raw", 0, Gpu::EBufferUsage::ShaderResource, 0, 64);
			Check(Resources.Get(Raw) && Resources.Get(Raw)->getDesc().canHaveRawViews,
				  "stride-free shader resource gets a raw view");
			// volatile 常量缓冲必须带非零 maxVersions，否则 NVRHI 会拒绝创建。
			Gpu::FBufferSlot Constants("Constants", 0, Gpu::EBufferUsage::Constant, 0, 0, 256, true);
			nvrhi::IBuffer* ConstantBuffer = Resources.Get(Constants);
			Check(ConstantBuffer != nullptr, "cpu-writable constant buffer is created");
			if (ConstantBuffer)
			{
				const auto& ConstantDesc = ConstantBuffer->getDesc();
				Check(ConstantDesc.isVolatile && ConstantDesc.maxVersions > 0,
					  "cpu-writable constant buffer is volatile and versioned");
			}
			// 自相矛盾的用法必须被拒绝，而不是交给 NVRHI 在 Debug 下报错、在 Release 下静默。
			Gpu::FBufferSlot Conflicting(
				"Conflicting", 0, Gpu::EBufferUsage::Constant | Gpu::EBufferUsage::UnorderedAccess, 0, 0, 256, true);
			Check(Resources.Get(Conflicting) == nullptr, "a volatile constant buffer cannot also be a UAV");
			Gpu::FBufferSlot StridedConstants("StridedConstants", 16, Gpu::EBufferUsage::Constant, 0, 4);
			Check(Resources.Get(StridedConstants) == nullptr, "a constant buffer cannot have a struct stride");
			nvrhi::BindingLayoutDesc D;
			D.visibility = nvrhi::ShaderType::Compute;
			D.bindings = {nvrhi::BindingLayoutItem::Texture_UAV(0)};
			Layout = Context.Gpu.Device->createBindingLayout(D);
			Check(Compute.Initialize(
				Context.Gpu.Device, *Context.Gpu.Shaders,
				{"prism/PrismTestCompute/TestPasses.hlsl", "main_cs", nvrhi::ShaderType::Compute, {}}, {Layout},
				dm::uint3(4, 4, 1)));
			nvrhi::GraphicsPipelineDesc Graphics;
			Graphics.renderState.depthStencilState.depthTestEnable = false;
			Graphics.renderState.depthStencilState.depthWriteEnable = false;
			Graphics.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
			Check(Raster.Initialize(
				Context.Gpu.Device, *Context.Gpu.Shaders, Graphics,
				{{"prism/PrismInfrastructureTests/TestPasses.hlsl", "main_vs", nvrhi::ShaderType::Vertex, {}},
				 {"prism/PrismInfrastructureTests/TestPasses.hlsl", "main_ps", nvrhi::ShaderType::Pixel, {}}}));
			Check(Comparison.Initialize(Context.Gpu.Device, *Context.Gpu.Shaders, *Context.Gpu.CommonPasses));
			nvrhi::BufferDesc IndexDesc;
			IndexDesc.byteSize = 12;
			IndexDesc.isIndexBuffer = true;
			IndexDesc.initialState = nvrhi::ResourceStates::IndexBuffer;
			IndexDesc.keepInitialState = true;
			Indices = Context.Gpu.Device->createBuffer(IndexDesc);
			Resize(Context.Gpu.Device);
			Reload.Initialize(Context.Gpu.Device, *Context.Gpu.Shaders,
							  std::filesystem::path(PRISM_TEST_BIN) / "PrismInfrastructureTests.exe");
			return bPassed ? FStatus::Ok()
						   : FStatus::Error(EErrorCode::Internal, "infrastructure initialization failed");
		}
		void BeginFrame(FExperimentContext& Context, const FExperimentFrame& Frame) override
		{
			if (Frame.Frame.SubmissionIndex > 10000)
			{
				Check(false, "Reload timeout");
				bDone = true;
			}
			if (bPending)
			{
				Verify(Context.Gpu.Device);
				bPending = false;
				++Round;
			}
			if (Round == 1)
			{
				Width = 20;
				Height = 12;
				Resize(Context.Gpu.Device);
			}
			if (Round == 5 && !bReloading && !bDone)
			{
				struct FVeto final : Gpu::IShaderReloadClient
				{
					FStatus PrepareShaders(Gpu::FShaderLibrary&) override
					{
						return FStatus::Error(EErrorCode::Internal, "expected test veto");
					}
					void CommitShaders() override
					{
					}
					void DiscardShaders() override
					{
					}
				} Veto;
				auto* Shaders = Context.Gpu.Shaders;
				nvrhi::ComputePipelineHandle Original = Compute.GetPipeline();
				Shaders->Register(&Veto);
				auto Status = Shaders->Reload(Factory(Context.Gpu.Device, false));
				Shaders->Unregister(&Veto);
				Check(!Status && Compute.GetPipeline() == Original && Shaders->GetGeneration() == 0,
					  "transaction rollback");
				auto Incompatible = Shaders->Reload(Factory(Context.Gpu.Device, true));
				Check(!Incompatible && Incompatible.ToStringWithCode().find("interface changed") != std::string::npos &&
						  Compute.GetPipeline() == Original && Shaders->GetGeneration() == 0,
					  "thread group change rejected");
				Check(Reload.Request(), "start asynchronous compiler");
				bReloading = true;
			}
			if (bReloading)
			{
				const bool bCommitted = Reload.Poll();
				if (!Reload.Running())
				{
					Check(bCommitted && Context.Gpu.Shaders->GetGeneration() == 1, Reload.GetMessage().c_str());
					bReloading = false;
					++Round;
				}
			}
			if (Round == 7 && !bFailurePending && !bDone)
			{
				const auto Script = std::filesystem::path(PRISM_TEST_BIN) / "expected-failure.cmake";
				const auto Source = std::filesystem::path(PRISM_TEST_BIN) / "expected-invalid.hlsl";
				{
					std::ofstream Out(Source);
					Out << "This is deliberately invalid HLSL.";
				}
				{
					std::ofstream Out(Script);
					Out << "execute_process(COMMAND \"" << PRISM_TEST_DXC << "\" -T cs_6_5 -E main_cs \""
						<< Source.generic_string() << "\" RESULT_VARIABLE result)\n"
						<< "if(NOT result EQUAL 0)\n message(FATAL_ERROR \"Shader compilation failed\")\nendif()\n";
				}
				Check(FailedBuild.Start(Script, std::filesystem::path(PRISM_TEST_BIN) / "failed-build"),
					  "start failed build");
				bFailurePending = true;
			}
			if (bFailurePending && FailedBuild.Poll())
			{
				Check(!FailedBuild.Succeeded() && Context.Gpu.Shaders->GetGeneration() == 1,
					  "failed build preserves generation");
				bDone = true;
				bFailurePending = false;
			}
			if (!bPassed)
				bDone = true;
			if (bDone)
			{
				donut::log::info("Infrastructure tests %s.", bPassed ? "passed" : "FAILED");
				Context.Callbacks.RequestQuit();
			}
		}
		nvrhi::ITexture* Render(FExperimentContext& Context, const FExperimentFrame& Frame) override
		{
			if (bDone || bReloading || bFailurePending)
				return Comparison.GetOutputTexture();
			auto* Commands = Frame.Commands;
			const uint32_t IndexData[] = {0, 1, 2};
			Commands->writeBuffer(Indices, IndexData, sizeof(IndexData));
			nvrhi::BindingSetDesc Bindings;
			Bindings.bindings = {nvrhi::BindingSetItem::Texture_UAV(0, A)};
			Check(Compute.DispatchExtent(Commands, {Compute.GetOrCreateBindingSet(Bindings, Layout)},
										 dm::uint3(Width, Height, 1)));
			Commands->clearTextureFloat(B, nvrhi::AllSubresources, nvrhi::Color(0));
			nvrhi::GraphicsState State;
			State.framebuffer = Framebuffer;
			State.indexBuffer = {Indices, nvrhi::Format::R32_UINT, 0};
			nvrhi::DrawArguments Args;
			Args.vertexCount = 3;
			Check(Raster.Draw(Commands, State, Args, true));
			Check(Comparison.Freeze(Commands, {B}));
			Commands->clearTextureFloat(B, nvrhi::AllSubresources, nvrhi::Color(0));
			Mode = Round == 2	? Gpu::EComparisonMode::A
				   : Round == 3 ? Gpu::EComparisonMode::B
				   : Round == 4 ? Gpu::EComparisonMode::Wipe
								: Gpu::EComparisonMode::Difference;
			Check(Comparison.Record(Commands, {A}, Comparison.GetFrozenImage(), {Mode, .5f, 1}));
			auto Invalid =
				Comparison.Record(Commands, {A, EColorSpace::SceneLinear}, {B, EColorSpace::DisplayEncoded}, {Mode});
			Check(!Invalid, "reject mixed color spaces");
			bPending = true;
			(void)Context;
			return Comparison.GetOutputTexture();
		}
		void Shutdown(FExperimentContext& Context) override
		{
			auto Marker = std::filesystem::path(PRISM_TEST_BIN) /
						  (bInitializationFailure ? "shutdown-failure.txt" : "shutdown-normal.txt");
			// Querying the Device also verifies the hook precedes Device teardown.
			std::ofstream Output(Marker);
			Output << (Context.Gpu.Device->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D12 ? "device-alive"
																						 : "wrong-device");
		}
		bool PassedVerification() const override
		{
			return bPassed && bDone;
		}
	};
	std::unique_ptr<IExperiment> CreateExperiment()
	{
		return std::make_unique<FInfrastructureTests>();
	}
} // namespace Prism::Host
