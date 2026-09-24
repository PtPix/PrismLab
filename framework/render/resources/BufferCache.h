#pragma once
#include "ResourceId.h"

// Persistent buffers keyed by ResourceId, with optional per-pixel sizing.

#include "Formats.h"

#include <framework/core/Status.h>
#include <framework/core/Types.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <string>
#include <vector>

namespace prism::gpu
{
    enum class BufferUsage : uint32_t
    {
        None = 0,
        ShaderResource = 1u << 0,
        UnorderedAccess = 1u << 1,
        Constant = 1u << 2,     // 常量缓冲（cbuffer）
        IndirectArgs = 1u << 3, // 间接绘制/派发参数
    };

    constexpr BufferUsage operator|(BufferUsage a, BufferUsage b) { return BufferUsage(uint32_t(a) | uint32_t(b)); }
    constexpr BufferUsage operator&(BufferUsage a, BufferUsage b) { return BufferUsage(uint32_t(a) & uint32_t(b)); }
    constexpr bool HasAny(BufferUsage value, BufferUsage test) { return (uint32_t(value) & uint32_t(test)) != 0; }

    struct BufferRequest
    {
        ResourceId id = AllocateResourceId();
        std::string name;
        BufferUsage usage = BufferUsage::ShaderResource;

        // 结构化缓冲：元素步长（0 表示按 raw / 常量缓冲处理）
        uint64_t structStride = 0;

        // 元素数量的三种来源，优先级：byteSize > elementsPerPixel > elementCount
        uint64_t elementCount = 0;
        uint32_t elementsPerPixel = 0;   // 按渲染分辨率像素数 × 该系数（每像素一个 reservoir 时为 1）
        uint64_t byteSize = 0;

        // 每帧由 CPU 写入。配合 BufferUsage::Constant 时创建为 volatile 常量缓冲，
        // 每帧可写入 maxVersions 次；其他用法下普通缓冲本身就支持 writeBuffer，无需该标志。
        bool cpuWritable = false;

        // volatile 常量缓冲的每帧版本数；NVRHI 要求非零，仅在 cpuWritable + Constant 时有效。
        uint32_t maxVersions = 16;

        [[nodiscard]] uint64_t ResolveByteSize(const Extent2D& renderSize) const;
        [[nodiscard]] uint32_t ResolveStride() const { return uint32_t(structStride); }

        // 用法组合是否自相矛盾；返回的错误直接说明该改哪个字段。
        [[nodiscard]] Status Validate() const;
    };

    class BufferCache
    {
    public:
        explicit BufferCache(nvrhi::IDevice* device);

        // 渲染分辨率变化时调用：按分辨率计算的缓冲会被释放，下一帧按新尺寸重建。
        void SetRenderSize(const Extent2D& renderSize);
        [[nodiscard]] const Extent2D& GetRenderSize() const { return m_RenderSize; }

        // Persistent cache keyed by request ID; names are diagnostic labels.
        nvrhi::IBuffer* GetOrCreate(const BufferRequest& request);
        nvrhi::IBuffer* Find(ResourceId id);

        // 释放全部缓冲；调用前必须保证 GPU 已空闲。
        void Clear();

        struct EntryInfo
        {
            std::string name;
            uint64_t byteSize = 0;
            uint32_t structStride = 0;
        };

        [[nodiscard]] std::vector<EntryInfo> GetEntries() const;

    private:
        struct Entry
        {
            BufferRequest request;
            nvrhi::BufferHandle buffer;
            uint64_t byteSize = 0;
        };

        Entry* FindEntry(ResourceId id);

        nvrhi::IDevice* m_Device = nullptr;
        Extent2D m_RenderSize{ 1, 1 };
        std::vector<Entry> m_Entries;
    };
}
