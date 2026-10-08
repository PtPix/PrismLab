#include "HostConfig.h"

#include <donut/app/ApplicationBase.h>
#include <donut/core/json.h>
#include <donut/core/log.h>
#include <donut/core/vfs/VFS.h>

#include <stdexcept>

namespace
{
	// Search upwards from the executable directory: binaries land in build/<preset>/bin while the
	// experiment's config.json sits next to its sources in samples/<name>/.
	std::filesystem::path FindFileUpwards(const std::filesystem::path& StartDirectory,
										  const std::filesystem::path& RelativeFilePath, int MaxDepth)
	{
		std::filesystem::path Directory = StartDirectory;

		for (int Depth = 0; Depth <= MaxDepth && !Directory.empty(); ++Depth)
		{
			const std::filesystem::path Candidate = Directory / RelativeFilePath;
			if (std::filesystem::exists(Candidate))
				return Candidate;

			const std::filesystem::path Parent = Directory.parent_path();
			if (Parent == Directory)
				break;

			Directory = Parent;
		}

		return std::filesystem::path();
	}
} // namespace

namespace Prism::Host
{
	FHostConfig LoadHostConfig(const std::filesystem::path& ExplicitPath, const std::filesystem::path& ExecutablePath)
	{
		FHostConfig Config;

		std::filesystem::path ConfigPath = ExplicitPath;
		if (ConfigPath.empty() && !ExecutablePath.empty())
		{
			// The experiment's own config.json, copied next to the executable by prism_add_target.
			std::filesystem::path FileName = ExecutablePath.filename();
			FileName.replace_extension(".json");

			const std::filesystem::path Candidate = donut::app::GetDirectoryWithExecutable() / FileName;
			if (std::filesystem::exists(Candidate))
				ConfigPath = Candidate;
		}

		if (ConfigPath.empty())
		{
			donut::log::warning("Prism: no config next to the executable, using built-in defaults.");
			return Config;
		}

		if (!std::filesystem::exists(ConfigPath))
		{
			donut::log::warning("Prism: config file does not exist: %s, using built-in defaults.",
								ConfigPath.string().c_str());
			return Config;
		}

		donut::vfs::NativeFileSystem FileSystem;
		Json::Value Root;
		if (!donut::json::LoadFromFile(FileSystem, ConfigPath, Root))
		{
			donut::log::warning("Prism: failed to parse config file: %s, using built-in defaults.",
								ConfigPath.string().c_str());
			return Config;
		}

		Config.SourcePath = ConfigPath;
		Config.bLoadedFromFile = true;

		if (Root.isMember("camera"))
		{
			const Json::Value& Camera = Root["camera"];
			Camera["position"] >> Config.Camera.Position;
			Camera["target"] >> Config.Camera.Target;
			Camera["fovDegrees"] >> Config.Camera.FovDegrees;
			Camera["zNear"] >> Config.Camera.ZNear;
			Camera["zFar"] >> Config.Camera.ZFar;
			Camera["moveSpeed"] >> Config.Camera.MoveSpeed;
			if (Camera.isMember("depthConvention"))
			{
				if (!Camera["depthConvention"].isString())
					throw std::invalid_argument("camera.depthConvention must be 'forward' or 'reverse'");
				const std::string Mode = Camera["depthConvention"].asString();
				if (Mode == "forward")
					Config.Camera.DepthConvention = EDepthConvention::ForwardZ0To1;
				else if (Mode == "reverse")
					Config.Camera.DepthConvention = EDepthConvention::ReversedZ0To1;
				else
					throw std::invalid_argument("invalid camera.depthConvention: " + Mode + " (expected forward or reverse)");
			}
		}

		if (Root.isMember("window"))
		{
			const Json::Value& Window = Root["window"];
			Window["width"] >> Config.Window.Width;
			Window["height"] >> Config.Window.Height;
			Window["vsync"] >> Config.Window.bVsync;
		}

		if (Root.isMember("lighting"))
		{
			const Json::Value& Lighting = Root["lighting"];
			Lighting["sunDirection"] >> Config.Lighting.SunDirection;
			Lighting["sunIrradiance"] >> Config.Lighting.SunIrradiance;
			Lighting["ambientIntensity"] >> Config.Lighting.AmbientIntensity;
		}

		if (Root.isMember("scene"))
		{
			const Json::Value& Scene = Root["scene"];
			Scene["source"] >> Config.Scene.Source;
			Scene["asset"] >> Config.Scene.Asset;
		}

		if (Root.isMember("render"))
		{
			const Json::Value& Render = Root["render"];
			Render["renderScale"] >> Config.Render.RenderScale;
			Render["enableGpuTiming"] >> Config.Render.bEnableGpuTiming;
		}

		return Config;
	}

	std::filesystem::path ResolveAssetPath(const std::string& Path)
	{
		if (Path.empty())
			return std::filesystem::path();

		const std::filesystem::path Candidate(Path);
		if (Candidate.is_absolute())
			return Candidate;

		std::error_code Error;
		if (std::filesystem::exists(Candidate, Error))
			return std::filesystem::absolute(Candidate, Error);

		// 运行目录通常是 build/<preset>/bin，而资产在仓库里：向上查找。
		const std::filesystem::path Found = FindFileUpwards(donut::app::GetDirectoryWithExecutable(), Candidate, 6);

		if (!Found.empty())
			return Found;

		return Candidate;
	}

	bool LoadExperimentSettings(const FHostConfig& Config, const char* ExperimentName, Json::Value& OutSettings)
	{
		if (!Config.bLoadedFromFile || !ExperimentName)
			return false;

		donut::vfs::NativeFileSystem FileSystem;
		Json::Value Root;
		if (!donut::json::LoadFromFile(FileSystem, Config.SourcePath, Root))
			return false;

		if (!Root.isMember("experiments"))
			return false;

		const Json::Value& Experiments = Root["experiments"];
		if (!Experiments.isMember(ExperimentName))
			return false;

		OutSettings = Experiments[ExperimentName];
		return true;
	}
} // namespace Prism::Host
