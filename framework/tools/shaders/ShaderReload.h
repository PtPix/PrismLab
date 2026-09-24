#pragma once
#include "ShaderBuildTask.h"
#include <vector>
#include <framework/render/shaders/ShaderLibrary.h>

namespace prism::host
{
    class ShaderReload
    {
    public:
        void Initialize(nvrhi::IDevice* device, gpu::ShaderLibrary& shaders, std::filesystem::path executable);
        bool Request();
        // Returns true only after a complete transaction commits.
        bool Poll();
        bool Running() const { return m_Build.Running(); }
        const std::string& Message() const { return m_Message; }
    private:
        nvrhi::IDevice* m_Device = nullptr;
        gpu::ShaderLibrary* m_Shaders = nullptr;
        std::filesystem::path m_Executable, m_Output;
        struct Package { std::string name, virtualRoot; };
        std::vector<Package> m_Packages;
        ShaderBuildTask m_Build;
        std::string m_Message = "Ready (F6 to compile and reload)";
    };
}
