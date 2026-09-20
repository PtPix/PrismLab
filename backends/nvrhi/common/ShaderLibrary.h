#pragma once

// NVRHI layer: shader loading with variant caching.
//
// Shader bridging is not a binary ABI: the same .hlsli is recompiled for each host, so the cache key
// must contain every option that changes the generated code (path, entry point, type, macros).

#include <donut/engine/ShaderFactory.h>

#include <nvrhi/nvrhi.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace renderlab::gpu
{
    using ShaderMacroList = std::vector<donut::engine::ShaderMacro>;

    class ShaderLibrary
    {
    public:
        ShaderLibrary(nvrhi::IDevice* device, std::shared_ptr<donut::engine::ShaderFactory> shaderFactory);

        // virtualPath is relative to the mounted /shaders root, e.g. "renderlab/forward/forward.hlsl".
        // Returns nullptr on failure; the reason is kept in GetLastError().
        nvrhi::ShaderHandle GetShader(
            const char* virtualPath,
            const char* entryName,
            nvrhi::ShaderType type,
            const ShaderMacroList& macros = ShaderMacroList());

        nvrhi::ShaderLibraryHandle GetShaderLibrary(
            const char* virtualPath,
            const ShaderMacroList& macros = ShaderMacroList());

        // Drops cached NVRHI shaders and the bytecode cache: used by shader hot reload.
        void ClearCache();

        [[nodiscard]] const std::string& GetLastError() const { return m_LastError; }
        [[nodiscard]] size_t GetVariantCount() const { return m_ShaderCache.size(); }

    private:
        static std::string MakeKey(const char* virtualPath, const char* entryName, nvrhi::ShaderType type, const ShaderMacroList& macros);

        nvrhi::IDevice* m_Device = nullptr;
        std::shared_ptr<donut::engine::ShaderFactory> m_ShaderFactory;
        std::unordered_map<std::string, nvrhi::ShaderHandle> m_ShaderCache;
        std::unordered_map<std::string, nvrhi::ShaderLibraryHandle> m_LibraryCache;
        std::string m_LastError;
    };
}
