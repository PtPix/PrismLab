#include "ShaderReload.h"
#include <donut/core/vfs/VFS.h>
#include <chrono>
#include <donut/core/json.h>
#include <fstream>

namespace Prism::Host
{
	void FShaderReload::Initialize(nvrhi::IDevice* InDevice, Gpu::FShaderLibrary& InShaders,
								   std::filesystem::path InExecutable)
	{
		Device = InDevice;
		ShaderLibrary = &InShaders;
		Executable = std::move(InExecutable);
		Packages.clear();
		auto Path = Executable;
		Path.replace_extension(".shaders.json");
		std::ifstream Input(Path);
		Json::Value Manifest;
		Json::CharReaderBuilder Reader;
		std::string Errors;
		if (!Input || !Json::parseFromStream(Reader, Input, &Manifest, &Errors) || !Manifest.isObject() ||
			Manifest["version"].asInt() != 1 || !Manifest["packages"].isArray())
		{
			StatusMessage = "Shader manifest unavailable. Configure and build this target.";
			return;
		}
		for (const auto& Package : Manifest["packages"])
			Packages.push_back({Package["name"].asString(), Package["virtualRoot"].asString()});
	}

	bool FShaderReload::Request()
	{
		if (!ShaderLibrary || Packages.empty() || Running())
			return false;
		const auto Stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		Output = Executable.parent_path() / ".shader-reload" / Executable.stem() / std::to_string(Stamp);
		auto Script = Executable;
		Script.replace_extension(".reload.cmake");
		const bool bStarted = Build.Start(Script, Output);
		StatusMessage = bStarted ? "Compiling shaders..." : Build.GetLog();
		return bStarted;
	}
	bool FShaderReload::Poll()
	{
		if (!Build.Poll())
			return false;
		if (!Build.Succeeded())
		{
			StatusMessage = Build.GetLog();
			return false;
		}
		auto Fs = std::make_shared<donut::vfs::RootFileSystem>();
		const auto Root = Executable.parent_path() / "shaders";
		Fs->mount("/shaders/donut", Root / "framework/dxil");
		for (const auto& Package : Packages)
			Fs->mount(Package.VirtualRoot, Output / Package.Name);
		Fs->mount("/shaders/prism", Root / "prism/dxil");
		auto Factory = std::make_shared<donut::engine::ShaderFactory>(Device, Fs, "/shaders");
		auto Status = ShaderLibrary->Reload(Factory);
		StatusMessage = Status ? "Reloaded shader generation " + std::to_string(ShaderLibrary->GetGeneration())
							   : Status.ToStringWithCode();
		return Status.IsOk();
	}
} // namespace Prism::Host
