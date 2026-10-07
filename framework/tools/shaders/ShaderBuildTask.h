#pragma once
#include <filesystem>
#include <string>

namespace Prism::Host
{
	class FShaderBuildTask
	{
	  public:
		FShaderBuildTask() = default;
		~FShaderBuildTask();
		FShaderBuildTask(const FShaderBuildTask&) = delete;
		FShaderBuildTask& operator=(const FShaderBuildTask&) = delete;
		bool Start(const std::filesystem::path& Script, const std::filesystem::path& Output);
		bool Poll();
		bool Running() const
		{
			return ProcessHandle != nullptr;
		}
		bool Succeeded() const
		{
			return ExitCode == 0;
		}
		const std::string& GetLog() const
		{
			return BuildLog;
		}

	  private:
		void* ProcessHandle = nullptr;
		void* JobHandle = nullptr;
		unsigned long ExitCode = 1;
		std::filesystem::path LogPath;
		std::string BuildLog;
	};
} // namespace Prism::Host
