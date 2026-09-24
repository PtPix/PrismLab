#pragma once

// Persistent resources have stable IDs. Copies share identity; names are labels.

#include "BufferCache.h"
#include "Formats.h"
#include "TextureCache.h"

#include <framework/render/data/Conventions.h>
#include <framework/core/Types.h>

#include <nvrhi/nvrhi.h>

#include <string>
#include <vector>

namespace prism::gpu
{
    class TextureSlot
    {
    public:
        TextureSlot(
            const char* name,
            PixelFormat format,
            TextureUsage usage,
            float resolutionScale = 1.f,
            Extent2D explicitSize = Extent2D{},
            bool hasClearValue = true,
            dm::float4 clearColor = dm::float4(0.f),
            float clearDepth = kDepthClearValue)
        {
            m_Request.name = name;
            m_Request.format = format;
            m_Request.usage = usage;
            m_Request.resolutionScale = resolutionScale;
            m_Request.explicitSize = explicitSize;
            m_Request.hasClearValue = hasClearValue;
            m_Request.clearColor = clearColor;
            m_Request.clearDepth = clearDepth;
        }

        [[nodiscard]] const char* GetName() const { return m_Request.name.c_str(); }
        [[nodiscard]] const TextureRequest& GetRequest() const { return m_Request; }

        // Change allocation policy while retaining the resource ID.
        TextureRequest& MutableRequest() { return m_Request; }

    private:
        TextureRequest m_Request;
    };

    class BufferSlot
    {
    public:
        BufferSlot(
            const char* name,
            uint64_t structStride,
            BufferUsage usage,
            uint32_t elementsPerPixel = 0,
            uint64_t elementCount = 0,
            uint64_t byteSize = 0,
            bool cpuWritable = false)
        {
            m_Request.name = name;
            m_Request.structStride = structStride;
            m_Request.usage = usage;
            m_Request.elementsPerPixel = elementsPerPixel;
            m_Request.elementCount = elementCount;
            m_Request.byteSize = byteSize;
            m_Request.cpuWritable = cpuWritable;
        }

        [[nodiscard]] const char* GetName() const { return m_Request.name.c_str(); }
        [[nodiscard]] const BufferRequest& GetRequest() const { return m_Request; }
        BufferRequest& MutableRequest() { return m_Request; }

    private:
        BufferRequest m_Request;
    };

    class ResourceTable
    {
    public:
        ResourceTable(TextureCache& textures, BufferCache& buffers)
            : m_Textures(textures)
            , m_Buffers(buffers)
        {
        }

        nvrhi::ITexture* Get(const TextureSlot& slot) { return m_Textures.GetOrCreate(slot.GetRequest()); }
        nvrhi::IBuffer* Get(const BufferSlot& slot) { return m_Buffers.GetOrCreate(slot.GetRequest()); }

        nvrhi::IFramebuffer* Framebuffer(const TextureSlot& color, const TextureSlot* depth = nullptr)
        {
            return m_Textures.GetFramebuffer(
                Get(color),
                depth ? Get(*depth) : nullptr);
        }

        // 宿主在渲染分辨率或输出分辨率变化时调用（内部会重建按分辨率计算的资源）。
        void SetRenderSize(const Extent2D& renderSize)
        {
            m_Textures.SetRenderSize(renderSize);
            m_Buffers.SetRenderSize(renderSize);
        }

        // 释放全部资源；调用前必须保证 GPU 已空闲。
        void Clear()
        {
            m_Textures.Clear();
            m_Buffers.Clear();
        }

        [[nodiscard]] TextureCache& Textures() { return m_Textures; }
        [[nodiscard]] BufferCache& Buffers() { return m_Buffers; }

        // 调试/UI 用的清单：纹理与缓冲的名称、格式、尺寸，便于确认"谁申请了多大的资源"。
        struct EntryInfo
        {
            std::string name;
            std::string kind;     // "texture" / "buffer"
            std::string detail;   // 尺寸与格式，或字节数
        };

        [[nodiscard]] std::vector<EntryInfo> GetEntries() const;

    private:
        TextureCache& m_Textures;
        BufferCache& m_Buffers;
    };
}
