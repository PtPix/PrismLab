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
#include <framework/core/Status.h>
#include "ShaderReloadClient.h"

namespace Prism::Gpu
{
	using FShaderMacroList = std::vector<donut::engine::ShaderMacro>;

	class FShaderLibrary
	{
	  public:
		FShaderLibrary(nvrhi::IDevice* Device, std::shared_ptr<donut::engine::ShaderFactory> ShaderFactory);

		// virtualPath is relative to the mounted /shaders root, e.g. "prism/forward/forward.hlsl".
		// Returns nullptr on failure; the reason is kept in GetLastError().
		nvrhi::ShaderHandle GetShader(const char* VirtualPath, const char* EntryName, nvrhi::ShaderType Type,
									  const FShaderMacroList& Macros = FShaderMacroList());

		nvrhi::ShaderLibraryHandle GetShaderLibrary(const char* VirtualPath,
													const FShaderMacroList& Macros = FShaderMacroList());

		// Drops cached NVRHI shaders and the bytecode cache: used by shader hot reload.
		void ClearCache();

		void Register(IShaderReloadClient* Client);
		void Unregister(IShaderReloadClient* Client);
		// Called at a frame boundary; clients prepare before any version is replaced.
		FStatus Reload(std::shared_ptr<donut::engine::ShaderFactory> CandidateFactory);
		std::shared_ptr<donut::engine::ShaderFactory> GetFactory() const
		{
			return ShaderFactory;
		}
		uint64_t GetGeneration() const
		{
			return Generation;
		}

		[[nodiscard]] const std::string& GetLastError() const
		{
			return LastError;
		}
		[[nodiscard]] size_t GetVariantCount() const
		{
			return ShaderCache.size();
		}

	  private:
		static std::string MakeKey(const char* VirtualPath, const char* EntryName, nvrhi::ShaderType Type,
								   const FShaderMacroList& Macros);

		nvrhi::IDevice* Device = nullptr;
		std::shared_ptr<donut::engine::ShaderFactory> ShaderFactory;
		std::unordered_map<std::string, nvrhi::ShaderHandle> ShaderCache;
		std::unordered_map<std::string, std::string> Interfaces;
		const std::unordered_map<std::string, std::string>* ReloadBaseline = nullptr;
		std::unordered_map<std::string, nvrhi::ShaderLibraryHandle> LibraryCache;
		std::string LastError;
		std::vector<IShaderReloadClient*> Clients;
		uint64_t Generation = 0;
	};
} // namespace Prism::Gpu
