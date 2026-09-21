#pragma once

// NVRHI 层：命名缓冲（结构化 / 常量 / 间接参数）。
//
// 与 RenderTargetPool 同一套思路：按名字声明需求，尺寸变化时自动重建。GI / ReSTIR 的 reservoir、
// 光源表、间接绘制参数都需要结构化缓冲，且元素数常常按渲染分辨率变化（每像素一个 reservoir）。

#include "Formats.h"

#include <renderlab/contracts/Types.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <string>
#include <vector>

namespace renderlab::gpu
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
        std::string name;
        BufferUsage usage = BufferUsage::ShaderResource;

        // 结构化缓冲：元素步长（0 表示按 raw / 常量缓冲处理）
        uint64_t structStride = 0;

        // 元素数量的三种来源，优先级：byteSize > elementsPerPixel > elementCount
        uint64_t elementCount = 0;
        uint32_t elementsPerPixel = 0;   // 按渲染分辨率像素数 × 该系数（每像素一个 reservoir 时为 1）
        uint64_t byteSize = 0;

        // 每帧由 CPU 写入（例如常量、计数器）：创建为 volatile 常量缓冲或多版本缓冲
        bool cpuWritable = false;

        [[nodiscard]] uint64_t ResolveByteSize(const Extent2D& renderSize) const;
        [[nodiscard]] uint32_t ResolveStride() const { return uint32_t(structStride); }
    };

    class BufferPool
    {
    public:
        explicit BufferPool(nvrhi::IDevice* device);

        // 渲染分辨率变化时调用：按分辨率计算的缓冲会被释放，下一帧按新尺寸重建。
        void SetRenderSize(const Extent2D& renderSize);
        [[nodiscard]] const Extent2D& GetRenderSize() const { return m_RenderSize; }

        // 按 name 取得（必要时创建）缓冲；尺寸或用法变化时旧缓冲被替换。
        nvrhi::IBuffer* GetOrCreate(const BufferRequest& request);
        nvrhi::IBuffer* Find(const char* name);

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

        Entry* FindEntry(const char* name);

        nvrhi::IDevice* m_Device = nullptr;
        Extent2D m_RenderSize{ 1, 1 };
        std::vector<Entry> m_Entries;
    };
}
