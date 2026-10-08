#pragma once

#include <framework/render/shaders/ShaderLibrary.h>
#include <framework/render/shaders/ShaderReloadClient.h>
#include <framework/core/Status.h>
#include <framework/core/Types.h>
#include <framework/render/data/Conventions.h>
#include <donut/engine/BindingCache.h>
#include <nvrhi/utils.h>
#include <type_traits>

namespace Prism::Gpu
{
	constexpr nvrhi::ComparisonFunc GetDepthCompare(EDepthConvention Convention)
	{
		return Convention == EDepthConvention::ReversedZ0To1
			? nvrhi::ComparisonFunc::Greater : nvrhi::ComparisonFunc::Less;
	}

	struct FShaderEntry
	{
		std::string Path;
		std::string Entry;
		nvrhi::ShaderType Stage = nvrhi::ShaderType::None;
		FShaderMacroList Defines;

		nvrhi::ShaderHandle Load(FShaderLibrary& Shaders) const
		{
			return Shaders.GetShader(Path.c_str(), Entry.c_str(), Stage, Defines);
		}
	};

	class FPassConstants
	{
	  public:
		bool Initialize(nvrhi::IDevice* Device, uint32_t Bytes, const char* Name, uint32_t Versions = 16)
		{
			Size = Bytes;
			Buffer = Device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(Bytes, Name, Versions));
			return Buffer != nullptr;
		}
		template <class InT> void Write(nvrhi::ICommandList* Commands, const InT& Value)
		{
			static_assert(std::is_trivially_copyable_v<InT>, "Constants must be trivially copyable");
			assert(Buffer && sizeof(InT) == Size);
			Commands->writeBuffer(Buffer, &Value, sizeof(InT));
		}
		nvrhi::IBuffer* Get() const
		{
			return Buffer;
		}

	  private:
		nvrhi::BufferHandle Buffer;
		size_t Size = 0;
	};

	// Non-movable: the shader library holds a registration until destruction.
	class FShaderPass : public IShaderReloadClient
	{
	  public:
		FShaderPass() = default;
		FShaderPass(const FShaderPass&) = delete;
		FShaderPass& operator=(const FShaderPass&) = delete;
		~FShaderPass() override;
		nvrhi::BindingSetHandle GetOrCreateBindingSet(const nvrhi::BindingSetDesc& Desc, nvrhi::IBindingLayout* Layout);
		void ClearBindings();

	  protected:
		void Attach(nvrhi::IDevice* InDevice, FShaderLibrary& InShaderLibrary);
		nvrhi::IDevice* Device = nullptr;
		FShaderLibrary* ShaderLibrary = nullptr;

	  private:
		std::unique_ptr<donut::engine::BindingCache> BindingCache;
	};
} // namespace Prism::Gpu
