#include "ShaderReload.h"
#include <donut/core/vfs/VFS.h>
#include <chrono>
#include <donut/core/json.h>
#include <fstream>

namespace prism::host
{
    void ShaderReload::Initialize(nvrhi::IDevice* device, gpu::ShaderLibrary& shaders, std::filesystem::path executable)
    {
        m_Device = device; m_Shaders = &shaders; m_Executable = std::move(executable);
        m_Packages.clear();
        auto path = m_Executable; path.replace_extension(".shaders.json");
        std::ifstream input(path); Json::Value manifest; Json::CharReaderBuilder reader; std::string errors;
        if (!input || !Json::parseFromStream(reader, input, &manifest, &errors) ||
            !manifest.isObject() || manifest["version"].asInt() != 1 || !manifest["packages"].isArray())
        { m_Message = "Shader manifest unavailable. Configure and build this target."; return; }
        for (const auto& package : manifest["packages"])
            m_Packages.push_back({package["name"].asString(), package["virtualRoot"].asString()});
    }

    bool ShaderReload::Request()
    {
        if (!m_Shaders || m_Packages.empty() || Running()) return false;
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        m_Output = m_Executable.parent_path() / ".shader-reload" / m_Executable.stem() / std::to_string(stamp);
        auto script = m_Executable; script.replace_extension(".reload.cmake");
        const bool started = m_Build.Start(script, m_Output);
        m_Message = started ? "Compiling shaders..." : m_Build.Log();
        return started;
    }
    bool ShaderReload::Poll()
    {
        if (!m_Build.Poll()) return false;
        if (!m_Build.Succeeded()) { m_Message = m_Build.Log(); return false; }
        auto fs = std::make_shared<donut::vfs::RootFileSystem>();
        const auto root = m_Executable.parent_path() / "shaders";
        fs->mount("/shaders/donut", root / "framework/dxil");
        for (const auto& package : m_Packages)
            fs->mount(package.virtualRoot, m_Output / package.name);
        fs->mount("/shaders/prism", root / "prism/dxil");
        auto factory = std::make_shared<donut::engine::ShaderFactory>(m_Device, fs, "/shaders");
        auto status = m_Shaders->Reload(factory);
        m_Message = status ? "Reloaded shader generation " + std::to_string(m_Shaders->GetGeneration()) : status.ToStringWithCode();
        return status.IsOk();
    }
}
