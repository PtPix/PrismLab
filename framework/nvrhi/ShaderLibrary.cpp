#include "ShaderLibrary.h"

#include <donut/core/log.h>

namespace renderlab::gpu
{
    namespace
    {
        const char* ToString(nvrhi::ShaderType type)
        {
            switch (type)
            {
            case nvrhi::ShaderType::Vertex:    return "vs";
            case nvrhi::ShaderType::Pixel:     return "ps";
            case nvrhi::ShaderType::Compute:   return "cs";
            case nvrhi::ShaderType::Geometry:  return "gs";
            case nvrhi::ShaderType::Hull:      return "hs";
            case nvrhi::ShaderType::Domain:    return "ds";
            case nvrhi::ShaderType::Amplification: return "as";
            case nvrhi::ShaderType::Mesh:      return "ms";
            default:                           return "unknown";
            }
        }
    }

    ShaderLibrary::ShaderLibrary(nvrhi::IDevice* device, std::shared_ptr<donut::engine::ShaderFactory> shaderFactory)
        : m_Device(device)
        , m_ShaderFactory(std::move(shaderFactory))
    {
    }

    std::string ShaderLibrary::MakeKey(const char* virtualPath, const char* entryName, nvrhi::ShaderType type, const ShaderMacroList& macros)
    {
        std::string key = virtualPath ? virtualPath : "";
        key += '|';
        key += entryName ? entryName : "";
        key += '|';
        key += ToString(type);

        for (const donut::engine::ShaderMacro& macro : macros)
        {
            key += '|';
            key += macro.name;
            key += '=';
            key += macro.definition;
        }

        return key;
    }

    nvrhi::ShaderHandle ShaderLibrary::GetShader(
        const char* virtualPath,
        const char* entryName,
        nvrhi::ShaderType type,
        const ShaderMacroList& macros)
    {
        if (!m_ShaderFactory || !virtualPath || !entryName)
        {
            m_LastError = "shader library is not initialized or the request is incomplete";
            return nullptr;
        }

        const std::string key = MakeKey(virtualPath, entryName, type, macros);

        const auto cached = m_ShaderCache.find(key);
        if (cached != m_ShaderCache.end())
            return cached->second;

        nvrhi::ShaderHandle shader = m_ShaderFactory->CreateShader(virtualPath, entryName, &macros, type);
        if (!shader)
        {
            m_LastError = std::string("failed to load ") + virtualPath + " (" + entryName + ", " + ToString(type) + ")";
            donut::log::error("RenderLab: %s", m_LastError.c_str());
            return nullptr;
        }

        m_ShaderCache.emplace(key, shader);
        return shader;
    }

    nvrhi::ShaderLibraryHandle ShaderLibrary::GetShaderLibrary(const char* virtualPath, const ShaderMacroList& macros)
    {
        if (!m_ShaderFactory || !virtualPath)
        {
            m_LastError = "shader library is not initialized or the request is incomplete";
            return nullptr;
        }

        std::string key = virtualPath;
        for (const donut::engine::ShaderMacro& macro : macros)
        {
            key += '|';
            key += macro.name;
            key += '=';
            key += macro.definition;
        }

        const auto cached = m_LibraryCache.find(key);
        if (cached != m_LibraryCache.end())
            return cached->second;

        nvrhi::ShaderLibraryHandle library = m_ShaderFactory->CreateShaderLibrary(virtualPath, &macros);
        if (!library)
        {
            m_LastError = std::string("failed to load shader library ") + virtualPath;
            donut::log::error("RenderLab: %s", m_LastError.c_str());
            return nullptr;
        }

        m_LibraryCache.emplace(key, library);
        return library;
    }

    void ShaderLibrary::ClearCache()
    {
        m_ShaderCache.clear();
        m_LibraryCache.clear();

        if (m_ShaderFactory)
            m_ShaderFactory->ClearCache();
    }
}
