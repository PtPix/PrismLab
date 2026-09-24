#pragma once

#include <framework/render/shaders/ShaderLibrary.h>
#include <framework/render/shaders/ShaderReloadClient.h>
#include <framework/core/Status.h>
#include <framework/core/Types.h>
#include <donut/engine/BindingCache.h>
#include <nvrhi/utils.h>
#include <type_traits>

namespace prism::gpu
{
    struct ShaderEntry
    {
        std::string path;
        std::string entry;
        nvrhi::ShaderType stage = nvrhi::ShaderType::None;
        ShaderMacroList defines;

        nvrhi::ShaderHandle Load(ShaderLibrary& shaders) const
        { return shaders.GetShader(path.c_str(), entry.c_str(), stage, defines); }
    };

    class PassConstants
    {
    public:
        bool Initialize(nvrhi::IDevice* device, uint32_t bytes, const char* name, uint32_t versions = 16)
        {
            m_Size = bytes;
            m_Buffer = device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(bytes, name, versions));
            return m_Buffer != nullptr;
        }
        template<class T> void Write(nvrhi::ICommandList* commands, const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>, "Constants must be trivially copyable");
            assert(m_Buffer && sizeof(T) == m_Size);
            commands->writeBuffer(m_Buffer, &value, sizeof(T));
        }
        nvrhi::IBuffer* Get() const { return m_Buffer; }
    private:
        nvrhi::BufferHandle m_Buffer;
        size_t m_Size = 0;
    };

    // Non-movable: the shader library holds a registration until destruction.
    class ShaderPass : public ShaderReloadClient
    {
    public:
        ShaderPass() = default;
        ShaderPass(const ShaderPass&) = delete;
        ShaderPass& operator=(const ShaderPass&) = delete;
        ~ShaderPass() override;
        nvrhi::BindingSetHandle Bindings(const nvrhi::BindingSetDesc& desc, nvrhi::IBindingLayout* layout);
        void ClearBindings();
    protected:
        void Attach(nvrhi::IDevice* device, ShaderLibrary& shaders);
        nvrhi::IDevice* m_Device = nullptr;
        ShaderLibrary* m_Shaders = nullptr;
    private:
        std::unique_ptr<donut::engine::BindingCache> m_Bindings;
    };
}
