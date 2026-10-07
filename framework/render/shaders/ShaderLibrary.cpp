#include "ShaderLibrary.h"
#include "ShaderInterface.h"

#include <donut/core/log.h>
#include <algorithm>

namespace Prism::Gpu
{
	namespace
	{
		const char* ToString(nvrhi::ShaderType Type)
		{
			switch (Type)
			{
				case nvrhi::ShaderType::Vertex:
					return "vs";
				case nvrhi::ShaderType::Pixel:
					return "ps";
				case nvrhi::ShaderType::Compute:
					return "cs";
				case nvrhi::ShaderType::Geometry:
					return "gs";
				case nvrhi::ShaderType::Hull:
					return "hs";
				case nvrhi::ShaderType::Domain:
					return "ds";
				case nvrhi::ShaderType::Amplification:
					return "as";
				case nvrhi::ShaderType::Mesh:
					return "ms";
				default:
					return "unknown";
			}
		}
	} // namespace

	FShaderLibrary::FShaderLibrary(nvrhi::IDevice* Device, std::shared_ptr<donut::engine::ShaderFactory> ShaderFactory)
		: Device(Device), ShaderFactory(std::move(ShaderFactory))
	{
	}

	std::string FShaderLibrary::MakeKey(const char* VirtualPath, const char* EntryName, nvrhi::ShaderType Type,
										const FShaderMacroList& Macros)
	{
		std::string Key = VirtualPath ? VirtualPath : "";
		Key += '|';
		Key += EntryName ? EntryName : "";
		Key += '|';
		Key += ToString(Type);

		for (const donut::engine::ShaderMacro& Macro : Macros)
		{
			Key += '|';
			Key += Macro.name;
			Key += '=';
			Key += Macro.definition;
		}

		return Key;
	}

	nvrhi::ShaderHandle FShaderLibrary::GetShader(const char* VirtualPath, const char* EntryName,
												  nvrhi::ShaderType Type, const FShaderMacroList& Macros)
	{
		if (!ShaderFactory || !VirtualPath || !EntryName)
		{
			LastError = "shader library is not initialized or the request is incomplete";
			return nullptr;
		}

		const std::string Key = MakeKey(VirtualPath, EntryName, Type, Macros);

		const auto Cached = ShaderCache.find(Key);
		if (Cached != ShaderCache.end())
			return Cached->second;

		const auto Signature = ReflectShaderInterface(*ShaderFactory, VirtualPath, EntryName, Macros);
		if (ReloadBaseline)
		{
			auto Old = ReloadBaseline->find(Key);
			if (Old != ReloadBaseline->end() && (Signature.empty() || Old->second.empty() || Signature != Old->second))
			{
				LastError =
					std::string("shader interface changed or reflection unavailable; rebuild the application: ") +
					VirtualPath;
				return nullptr;
			}
		}
		nvrhi::ShaderHandle Shader = ShaderFactory->CreateShader(VirtualPath, EntryName, &Macros, Type);
		if (!Shader)
		{
			LastError = std::string("failed to load ") + VirtualPath + " (" + EntryName + ", " + ToString(Type) + ")";
			donut::log::error("Prism: %s", LastError.c_str());
			return nullptr;
		}

		ShaderCache.emplace(Key, Shader);
		Interfaces[Key] = Signature;
		return Shader;
	}

	nvrhi::ShaderLibraryHandle FShaderLibrary::GetShaderLibrary(const char* VirtualPath, const FShaderMacroList& Macros)
	{
		if (!ShaderFactory || !VirtualPath)
		{
			LastError = "shader library is not initialized or the request is incomplete";
			return nullptr;
		}

		std::string Key = VirtualPath;
		for (const donut::engine::ShaderMacro& Macro : Macros)
		{
			Key += '|';
			Key += Macro.name;
			Key += '=';
			Key += Macro.definition;
		}

		const auto Cached = LibraryCache.find(Key);
		if (Cached != LibraryCache.end())
			return Cached->second;

		nvrhi::ShaderLibraryHandle Library = ShaderFactory->CreateShaderLibrary(VirtualPath, &Macros);
		if (!Library)
		{
			LastError = std::string("failed to load shader library ") + VirtualPath;
			donut::log::error("Prism: %s", LastError.c_str());
			return nullptr;
		}

		LibraryCache.emplace(Key, Library);
		return Library;
	}

	void FShaderLibrary::ClearCache()
	{
		ShaderCache.clear();
		LibraryCache.clear();

		if (ShaderFactory)
			ShaderFactory->ClearCache();
	}

	void FShaderLibrary::Register(IShaderReloadClient* Client)
	{
		if (Client && std::find(Clients.begin(), Clients.end(), Client) == Clients.end())
			Clients.push_back(Client);
	}

	void FShaderLibrary::Unregister(IShaderReloadClient* Client)
	{
		Clients.erase(std::remove(Clients.begin(), Clients.end(), Client), Clients.end());
	}

	FStatus FShaderLibrary::Reload(std::shared_ptr<donut::engine::ShaderFactory> CandidateFactory)
	{
		FShaderLibrary Candidate(Device, std::move(CandidateFactory));
		Candidate.ReloadBaseline = &Interfaces;
		for (auto* Client : Clients)
		{
			auto Status = Client->PrepareShaders(Candidate);
			if (!Status)
			{
				for (auto* Prepared : Clients)
					Prepared->DiscardShaders();
				LastError = Status.GetMessage();
				return Status;
			}
		}
		for (auto* Client : Clients)
			Client->CommitShaders();
		ShaderFactory = std::move(Candidate.ShaderFactory);
		ShaderCache = std::move(Candidate.ShaderCache);
		Interfaces = std::move(Candidate.Interfaces);
		LibraryCache = std::move(Candidate.LibraryCache);
		++Generation;
		LastError.clear();
		return FStatus::Ok();
	}
} // namespace Prism::Gpu
