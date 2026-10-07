#include "ShaderInterface.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <donut/core/vfs/VFS.h>
#include <ShaderMake/ShaderBlob.h>
#include <d3d12shader.h>
#include <wrl/client.h>
#include <dxcapi.h>
#include <algorithm>
#include <sstream>

namespace Prism::Gpu
{
	namespace
	{
		bool WriteType(std::ostream& Out, ID3D12ShaderReflectionType* Type)
		{
			D3D12_SHADER_TYPE_DESC D{};
			if (!Type || FAILED(Type->GetDesc(&D)))
				return false;
			Out << '[' << D.Class << ',' << D.Type << ',' << D.Rows << ',' << D.Columns << ',' << D.Elements << ','
				<< D.Offset << ',' << D.Members;
			for (UINT I = 0; I < D.Members; ++I)
				if (!WriteType(Out, Type->GetMemberTypeByIndex(I)))
					return false;
			Out << ']';
			return true;
		}
	} // namespace
	std::string ReflectShaderInterface(donut::engine::ShaderFactory& Factory, const char* Path, const char* Entry,
									   const FShaderMacroList& Defines)
	{
		using Microsoft::WRL::ComPtr;
		struct FDxcModule
		{
			HMODULE Handle = LoadLibraryA(PRISM_DXCOMPILER_PATH);
			~FDxcModule()
			{
				if (Handle)
					FreeLibrary(Handle);
			}
		};
		static FDxcModule Module;
		if (!Module.Handle)
			return {};
		auto Create = reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(Module.Handle, "DxcCreateInstance"));
		if (!Create)
			return {};
		auto Blob = Factory.GetBytecode(Path, Entry);
		if (!Blob)
			return {};
		std::vector<ShaderMake::ShaderConstant> Constants;
		for (const auto& D : Defines)
			Constants.push_back({D.name.c_str(), D.definition.c_str()});
		const void* Bytes = nullptr;
		size_t Size = 0;
		if (!ShaderMake::FindPermutationInBlob(Blob->data(), Blob->size(), Constants.data(), uint32_t(Constants.size()),
											   &Bytes, &Size))
			return {};
		ComPtr<IDxcUtils> Utils;
		if (FAILED(Create(CLSID_DxcUtils, IID_PPV_ARGS(&Utils))))
			return {};
		DxcBuffer Buffer{Bytes, Size, 0};
		ComPtr<ID3D12ShaderReflection> Reflection;
		if (FAILED(Utils->CreateReflection(&Buffer, IID_PPV_ARGS(&Reflection))))
			return {};
		D3D12_SHADER_DESC Desc{};
		if (FAILED(Reflection->GetDesc(&Desc)))
			return {};
		std::vector<std::string> Records;
		for (UINT I = 0; I < Desc.BoundResources; ++I)
		{
			D3D12_SHADER_INPUT_BIND_DESC B{};
			if (FAILED(Reflection->GetResourceBindingDesc(I, &B)))
				return {};
			std::ostringstream Row;
			Row << "R:" << B.Space << ':' << B.BindPoint << ':' << B.BindCount << ':' << B.Type << ':' << B.Dimension
				<< ':' << B.ReturnType;
			if (B.Type == D3D_SIT_CBUFFER)
			{
				auto* Cb = Reflection->GetConstantBufferByName(B.Name);
				D3D12_SHADER_BUFFER_DESC C{};
				if (FAILED(Cb->GetDesc(&C)))
					return {};
				Row << ':' << C.Size;
				for (UINT V = 0; V < C.Variables; ++V)
				{
					auto* Variable = Cb->GetVariableByIndex(V);
					D3D12_SHADER_VARIABLE_DESC Vd{};
					if (FAILED(Variable->GetDesc(&Vd)))
						return {};
					Row << ':' << Vd.StartOffset << ',' << Vd.Size;
					if (!WriteType(Row, Variable->GetType()))
						return {};
				}
			}
			Records.push_back(Row.str());
		}
		for (int Direction = 0; Direction < 2; ++Direction)
		{
			const UINT Count = Direction ? Desc.OutputParameters : Desc.InputParameters;
			for (UINT I = 0; I < Count; ++I)
			{
				D3D12_SIGNATURE_PARAMETER_DESC P{};
				const HRESULT Hr =
					Direction ? Reflection->GetOutputParameterDesc(I, &P) : Reflection->GetInputParameterDesc(I, &P);
				if (FAILED(Hr))
					return {};
				std::ostringstream Row;
				Row << "S:" << Direction << ':' << P.SemanticName << ':' << P.SemanticIndex << ':' << P.Register << ':'
					<< P.SystemValueType << ':' << P.ComponentType << ':' << unsigned(P.Mask);
				Records.push_back(Row.str());
			}
		}
		UINT X = 0, Y = 0, Z = 0;
		Reflection->GetThreadGroupSize(&X, &Y, &Z);
		std::sort(Records.begin(), Records.end());
		std::ostringstream Result;
		Result << "Threads:" << X << ',' << Y << ',' << Z << '\n';
		for (const auto& Row : Records)
			Result << Row << '\n';
		return Result.str();
	}
} // namespace Prism::Gpu
