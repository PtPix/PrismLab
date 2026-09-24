#pragma once
#include <filesystem>
#include <string>

namespace prism::host
{
    class ShaderBuildTask
    {
    public:
        ShaderBuildTask() = default;
        ~ShaderBuildTask();
        ShaderBuildTask(const ShaderBuildTask&) = delete;
        ShaderBuildTask& operator=(const ShaderBuildTask&) = delete;
        bool Start(const std::filesystem::path& script, const std::filesystem::path& output);
        bool Poll();
        bool Running() const { return m_Process != nullptr; }
        bool Succeeded() const { return m_ExitCode == 0; }
        const std::string& Log() const { return m_Log; }
    private:
        void* m_Process = nullptr;
        void* m_Job = nullptr;
        unsigned long m_ExitCode = 1;
        std::filesystem::path m_LogPath;
        std::string m_Log;
    };
}
