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

namespace prism::gpu
{
    namespace
    {
        bool WriteType(std::ostream& out, ID3D12ShaderReflectionType* type)
        {
            D3D12_SHADER_TYPE_DESC d{};
            if (!type || FAILED(type->GetDesc(&d))) return false;
            out << '[' << d.Class << ',' << d.Type << ',' << d.Rows << ',' << d.Columns
                << ',' << d.Elements << ',' << d.Offset << ',' << d.Members;
            for (UINT i = 0; i < d.Members; ++i)
                if (!WriteType(out, type->GetMemberTypeByIndex(i))) return false;
            out << ']';
            return true;
        }
    }
    std::string ReflectShaderInterface(donut::engine::ShaderFactory& factory, const char* path,
        const char* entry, const ShaderMacroList& defines)
    {
        using Microsoft::WRL::ComPtr;
        struct DxcModule
        {
            HMODULE handle = LoadLibraryA(PRISM_DXCOMPILER_PATH);
            ~DxcModule() { if (handle) FreeLibrary(handle); }
        };
        static DxcModule module;
        if (!module.handle) return {};
        auto create = reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(module.handle, "DxcCreateInstance"));
        if (!create) return {};
        auto blob = factory.GetBytecode(path, entry);
        if (!blob) return {};
        std::vector<ShaderMake::ShaderConstant> constants;
        for (const auto& d : defines) constants.push_back({d.name.c_str(), d.definition.c_str()});
        const void* bytes = nullptr; size_t size = 0;
        if (!ShaderMake::FindPermutationInBlob(blob->data(), blob->size(), constants.data(), uint32_t(constants.size()), &bytes, &size)) return {};
        ComPtr<IDxcUtils> utils;
        if (FAILED(create(CLSID_DxcUtils, IID_PPV_ARGS(&utils)))) return {};
        DxcBuffer buffer{bytes, size, 0};
        ComPtr<ID3D12ShaderReflection> reflection;
        if (FAILED(utils->CreateReflection(&buffer, IID_PPV_ARGS(&reflection)))) return {};
        D3D12_SHADER_DESC desc{}; if (FAILED(reflection->GetDesc(&desc))) return {};
        std::vector<std::string> records;
        for (UINT i = 0; i < desc.BoundResources; ++i)
        {
            D3D12_SHADER_INPUT_BIND_DESC b{}; if (FAILED(reflection->GetResourceBindingDesc(i, &b))) return {};
            std::ostringstream row;
            row << "R:" << b.Space << ':' << b.BindPoint << ':' << b.BindCount << ':' << b.Type << ':' << b.Dimension << ':' << b.ReturnType;
            if (b.Type == D3D_SIT_CBUFFER)
            {
                auto* cb = reflection->GetConstantBufferByName(b.Name);
                D3D12_SHADER_BUFFER_DESC c{}; if (FAILED(cb->GetDesc(&c))) return {};
                row << ':' << c.Size;
                for (UINT v = 0; v < c.Variables; ++v)
                {
                    auto* variable = cb->GetVariableByIndex(v);
                    D3D12_SHADER_VARIABLE_DESC vd{};
                    if (FAILED(variable->GetDesc(&vd))) return {};
                    row << ':' << vd.StartOffset << ',' << vd.Size;
                    if (!WriteType(row, variable->GetType())) return {};
                }
            }
            records.push_back(row.str());
        }
        for (int direction = 0; direction < 2; ++direction)
        {
            const UINT count = direction ? desc.OutputParameters : desc.InputParameters;
            for (UINT i = 0; i < count; ++i)
            {
                D3D12_SIGNATURE_PARAMETER_DESC p{};
                const HRESULT hr = direction ? reflection->GetOutputParameterDesc(i, &p) : reflection->GetInputParameterDesc(i, &p);
                if (FAILED(hr)) return {};
                std::ostringstream row; row << "S:" << direction << ':' << p.SemanticName << ':' << p.SemanticIndex << ':' << p.Register
                    << ':' << p.SystemValueType << ':' << p.ComponentType << ':' << unsigned(p.Mask);
                records.push_back(row.str());
            }
        }
        UINT x = 0, y = 0, z = 0; reflection->GetThreadGroupSize(&x, &y, &z);
        std::sort(records.begin(), records.end());
        std::ostringstream result; result << "Threads:" << x << ',' << y << ',' << z << '\n';
        for (const auto& row : records) result << row << '\n';
        return result.str();
    }
}
