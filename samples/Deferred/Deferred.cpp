#include "Deferred.h"

#include <framework/tools/comparison/ComparisonController.h>
#include <framework/tools/inspection/DebugViewRegistry.h>

#include <donut/core/json.h>
#include <donut/core/log.h>
#include <donut/core/vfs/VFS.h>
#include <imgui.h>

namespace dm = donut::math;

namespace Prism::Samples
{
    FStatus FDeferredExperiment::Initialize(Host::FExperimentContext& Context)
    {
        if (!Context.Gpu.Device || !Context.Gpu.Shaders || !Context.Gpu.Targets || !Context.Gpu.CommonPasses)
            return FStatus::Error(EErrorCode::NotInitialized, "deferred sample needs GPU services");

        const Host::FHostConfig Defaults;
        const auto& Config = Context.Config ? *Context.Config : Defaults;
        if (Config.Scene.Source != "procedural" || !Config.Scene.Asset.empty())
            return FStatus::Error(EErrorCode::Unsupported, "the depth sample currently supports procedural geometry only");

        const auto SceneStatus = Scene.Load(Context.Gpu.Device, Context.Gpu.Shaders->GetFactory(),
                                            std::make_shared<donut::vfs::NativeFileSystem>(),
                                            Config.Scene, Config.Lighting);
        if (!SceneStatus)
            return SceneStatus;
        if (!Scene.GetData().Geometry.IsValid())
            return FStatus::Error(EErrorCode::ResourceMissing, "the scene contains no drawable geometry");
        Context.Scene.Stats = Scene.GetData().Stats;
        Context.Scene.Description = Scene.GetData().Description;

        if (Context.Config)
        {
            Json::Value Settings;
            if (Host::LoadExperimentSettings(*Context.Config, GetName(), Settings) && Settings["debugMode"].isInt())
                DebugMode = Settings["debugMode"].asInt() == 1 ? 1 : 0;
        }

        DepthRequest.Name = "Deferred.Depth";
        DepthRequest.Format = EPixelFormat::D32Float;
        DepthRequest.Usage = Gpu::ETextureUsage::DepthStencil | Gpu::ETextureUsage::ShaderResource;
        DepthRequest.ClearDepth = GetDepthClearValue(Config.Camera.DepthConvention);
        Context.Output.ColorSpace = EColorSpace::DisplayEncoded;

        auto Status = DepthRenderer.Initialize(Context.Gpu.Device, *Context.Gpu.Shaders);
        return Status ? Preview.Initialize(Context.Gpu) : Status;
    }

    nvrhi::ITexture* FDeferredExperiment::Render(Host::FExperimentContext& Context, const Host::FExperimentFrame& Frame)
    {
        auto& Targets = *Context.Gpu.Targets;
        const float ClearDepth = GetDepthClearValue(Frame.Camera.DepthConvention);
        if (DepthRequest.ClearDepth != ClearDepth)
        {
            DepthRequest.ClearDepth = ClearDepth;
            Preview.OnResize();
        }
        nvrhi::ITexture* Depth = Targets.GetOrCreate(DepthRequest);
        nvrhi::IFramebuffer* DepthTarget = Depth ? Targets.GetFramebuffer(nullptr, Depth) : nullptr;
        if (!DepthTarget)
            return nullptr;

        Scene.Update(Frame.Commands, uint32_t(Frame.Frame.SubmissionIndex));
        const auto& Geometry = Scene.GetData().Geometry;
        DepthBatch.BufferGroups.clear();
        DepthBatch.Draws.clear();
        DepthBatch.BufferGroups.reserve(Geometry.BufferGroups.size());
        DepthBatch.Draws.reserve(Geometry.Draws.size());
        for (const auto& Group : Geometry.BufferGroups)
            DepthBatch.BufferGroups.push_back({Group.VertexBuffer, Group.PositionRange,
                                               Group.VertexStride ? Group.VertexStride : uint32_t(sizeof(dm::float3)),
                                               Group.IndexBuffer, Group.IndexFormat});
        for (const auto& Draw : Geometry.Draws)
        {
            if (Draw.BaseVertex < 0)
            {
                donut::log::error("Deferred: negative base vertex is not supported by the depth batch");
                return nullptr;
            }
            DepthBatch.Draws.push_back({Draw.BufferGroupIndex, Draw.FirstIndex, Draw.IndexCount,
                                        uint32_t(Draw.BaseVertex), Draw.ObjectToWorld});
        }
        const auto Status = DepthRenderer.Record(Frame.Commands, DepthBatch, Frame.Camera.Raster.WorldToClip,
                                                 Frame.Camera.DepthConvention, Depth, DepthTarget);
        if (!Status)
        {
            donut::log::error("Deferred: depth pass failed: %s", Status.ToStringWithCode().c_str());
            return nullptr;
        }

        if (Context.Tools.DebugViews)
            Context.Tools.DebugViews->Publish(GetName(), "Device depth", Depth);
        nvrhi::ITexture* Output = Preview.Record(Context.Gpu, Frame.Commands, Depth, Frame.Camera,
                                                  Frame.RenderSize, DebugMode);
        if (Output && Context.Tools.Comparison)
            Context.Tools.Comparison->Publish("Depth preview", {Output, EColorSpace::DisplayEncoded});
        return Output;
    }

    void FDeferredExperiment::BuildUI(Host::FExperimentContext&)
    {
        const char* Modes[] = {"Device depth", "Linear depth"};
        ImGui::Combo("Depth preview", &DebugMode, Modes, 2);
    }

    void FDeferredExperiment::OnResize(Host::FExperimentContext&, const FExtent2D&, const FExtent2D&)
    {
        Preview.OnResize();
    }
} // namespace Prism::Samples

namespace Prism::Host
{
    std::unique_ptr<IExperiment> CreateExperiment()
    {
        return std::make_unique<Samples::FDeferredExperiment>();
    }
} // namespace Prism::Host
