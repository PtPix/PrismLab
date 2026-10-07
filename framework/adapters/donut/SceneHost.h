#pragma once

// Donut facilities: owns the active scene and keeps it up to date.
//
// Two sources are supported:
//   * procedural  —— built in code, no assets (default for algorithm work)
//   * gltf        —— loaded through Donut's scene loader, for realism later on
//
// Everything the experiments see goes through SceneData: the Donut graph for the shared scene pipeline, plus
// contract-level light records and a geometry batch for algorithm-side passes.

#include <framework/scene/ScenePreset.h>
#include "SceneData.h"

#include <framework/core/Status.h>

#include <donut/engine/Scene.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/TextureCache.h>

#include <memory>

namespace donut::vfs
{
	class IFileSystem;
}

namespace Prism::Adapter
{
	class FSceneHost
	{
	  public:
		// Creates the scene and records its first uploads. Does not return a command list to the caller:
		// the scene is ready to draw when the call returns.
		FStatus Load(nvrhi::IDevice* InDevice, const std::shared_ptr<donut::engine::ShaderFactory>& InShaderFactory,
					 const std::shared_ptr<donut::vfs::IFileSystem>& InFileSystem, const FScenePreset& InScenePreset,
					 const FLightingPreset& InLightingPreset);

		// 每帧调用（在已打开的命令列表中）：刷新动画、变换和缓冲。程序化场景是空实现。
		void Update(nvrhi::ICommandList* Commands, uint32_t FrameIndex);

		void Reset();

		[[nodiscard]] const FSceneData& GetData() const
		{
			return Scene;
		}
		[[nodiscard]] bool IsLoaded() const
		{
			return Scene.IsLoaded();
		}
		[[nodiscard]] bool IsAssetScene() const
		{
			return LoadedScene != nullptr;
		}

	  private:
		void BuildGeometryBatchFromSceneGraph();
		void CollectStats();

		nvrhi::IDevice* Device = nullptr;
		std::shared_ptr<donut::vfs::IFileSystem> FileSystem;
		std::shared_ptr<donut::engine::TextureCache> TextureCache;
		std::unique_ptr<donut::engine::Scene> LoadedScene;
		FSceneData Scene;
	};
} // namespace Prism::Adapter
