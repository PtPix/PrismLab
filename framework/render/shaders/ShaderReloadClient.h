#pragma once
#include <framework/core/Status.h>

namespace Prism::Gpu
{
	class FShaderLibrary;
	class IShaderReloadClient
	{
	  public:
		virtual ~IShaderReloadClient() = default;
		virtual FStatus PrepareShaders(FShaderLibrary& Candidate) = 0;
		virtual void CommitShaders() = 0;
		virtual void DiscardShaders() = 0;
	};
} // namespace Prism::Gpu
