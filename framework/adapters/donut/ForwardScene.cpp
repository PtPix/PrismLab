#include "ForwardScene.h"
#include <donut/core/vfs/VFS.h>
namespace Prism::Adapter
{
	FStatus FForwardScene::Initialize(Gpu::FRenderServices& Gpu, const FScenePreset& ScenePreset,
									  const FLightingPreset& LightingPreset)
	{
		auto Factory = Gpu.Shaders->GetFactory();
		if (!Pipeline.Initialize(Gpu.Device, Factory))
			return FStatus::Error(EErrorCode::PipelineCreationFailed, "legacy forward pipeline failed");
		AmbientTop = dm::float3(LightingPreset.AmbientIntensity);
		AmbientBottom = AmbientTop * 0.6f;
		return Scene.Load(Gpu.Device, Factory, std::make_shared<donut::vfs::NativeFileSystem>(), ScenePreset,
						  LightingPreset);
	}
	void FForwardScene::Record(nvrhi::ICommandList* Commands, uint64_t Submission, const donut::engine::IView& View,
							   const donut::engine::IView& Previous, nvrhi::IFramebuffer* Target)
	{
		Scene.Update(Commands, uint32_t(Submission));
		if (Scene.GetData().Graph)
			Pipeline.RenderScene(Commands, *Scene.GetData().Graph, View, Previous, Target, AmbientTop, AmbientBottom);
	}
} // namespace Prism::Adapter
