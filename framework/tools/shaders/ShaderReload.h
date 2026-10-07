#pragma once
#include "ShaderBuildTask.h"
#include <vector>
#include <framework/render/shaders/ShaderLibrary.h>

namespace Prism::Host
{
	class FShaderReload
	{
	  public:
		void Initialize(nvrhi::IDevice* InDevice, Gpu::FShaderLibrary& InShaders, std::filesystem::path InExecutable);
		bool Request();
		// Returns true only after a complete transaction commits.
		bool Poll();
		bool Running() const
		{
			return Build.Running();
		}
		const std::string& GetMessage() const
		{
			return StatusMessage;
		}

	  private:
		nvrhi::IDevice* Device = nullptr;
		Gpu::FShaderLibrary* ShaderLibrary = nullptr;
		std::filesystem::path Executable, Output;
		struct FPackage
		{
			std::string Name, VirtualRoot;
		};
		std::vector<FPackage> Packages;
		FShaderBuildTask Build;
		std::string StatusMessage = "Ready (F6 to compile and reload)";
	};
} // namespace Prism::Host
