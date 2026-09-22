#include "BufferPool.h"

#include <donut/core/log.h>

#include <algorithm>

namespace prism::gpu
{
    uint64_t BufferRequest::ResolveByteSize(const Extent2D& renderSize) const
    {
        if (byteSize > 0)
            return byteSize;

        uint64_t count = elementCount;
        if (elementsPerPixel > 0)
            count = uint64_t(renderSize.width) * uint64_t(renderSize.height) * uint64_t(elementsPerPixel);

        if (count == 0)
            return 0;

        const uint64_t stride = (structStride > 0) ? structStride : 4;
        return count * stride;
    }

    BufferPool::BufferPool(nvrhi::IDevice* device)
        : m_Device(device)
    {
    }

    void BufferPool::SetRenderSize(const Extent2D& renderSize)
    {
        if (m_RenderSize == renderSize)
            return;

        m_RenderSize = renderSize;

        // 只有按分辨率计算的缓冲需要重建，固定尺寸的保留。
        for (Entry& entry : m_Entries)
        {
            if (entry.request.elementsPerPixel > 0)
                entry.buffer = nullptr;
        }
    }

    BufferPool::Entry* BufferPool::FindEntry(const char* name)
    {
        if (!name)
            return nullptr;

        for (Entry& entry : m_Entries)
        {
            if (entry.request.name == name)
                return &entry;
        }

        return nullptr;
    }

    nvrhi::IBuffer* BufferPool::GetOrCreate(const BufferRequest& request)
    {
        if (request.name.empty())
        {
            donut::log::error("BufferPool: buffer requests must be named.");
            return nullptr;
        }

        const uint64_t byteSize = request.ResolveByteSize(m_RenderSize);
        if (byteSize == 0)
        {
            donut::log::error("BufferPool: '%s' resolves to zero bytes.", request.name.c_str());
            return nullptr;
        }

        Entry* entry = FindEntry(request.name.c_str());
        if (!entry)
        {
            Entry created;
            created.request = request;
            created.byteSize = byteSize;
            m_Entries.push_back(std::move(created));
            entry = &m_Entries.back();
        }
        else
        {
            const bool byteSizeChanged = entry->byteSize != byteSize;
            const bool layoutChanged =
                entry->request.usage != request.usage ||
                entry->request.structStride != request.structStride;

            if (byteSizeChanged || layoutChanged)
                entry->buffer = nullptr;

            entry->request = request;
            entry->byteSize = byteSize;
        }

        if (entry->buffer)
            return entry->buffer;

        nvrhi::BufferDesc desc;
        desc.byteSize = byteSize;
        desc.structStride = uint32_t(request.structStride);
        desc.debugName = request.name;
        desc.canHaveUAVs = HasAny(request.usage, BufferUsage::UnorderedAccess);
        desc.canHaveRawViews = (request.structStride == 0);
        desc.isConstantBuffer = HasAny(request.usage, BufferUsage::Constant);
        desc.isDrawIndirectArgs = HasAny(request.usage, BufferUsage::IndirectArgs);
        desc.isVolatile = request.cpuWritable && desc.isConstantBuffer;
        desc.initialState = nvrhi::ResourceStates::Common;

        entry->buffer = m_Device->createBuffer(desc);
        if (!entry->buffer)
            donut::log::error("BufferPool: failed to create buffer '%s' (%llu bytes).",
                request.name.c_str(), (unsigned long long)byteSize);

        return entry->buffer;
    }

    nvrhi::IBuffer* BufferPool::Find(const char* name)
    {
        Entry* entry = FindEntry(name);
        return entry ? entry->buffer.Get() : nullptr;
    }

    void BufferPool::Clear()
    {
        m_Entries.clear();
    }

    std::vector<BufferPool::EntryInfo> BufferPool::GetEntries() const
    {
        std::vector<EntryInfo> infos;
        infos.reserve(m_Entries.size());

        for (const Entry& entry : m_Entries)
        {
            infos.push_back(EntryInfo{
                entry.request.name,
                entry.buffer ? entry.byteSize : 0,
                uint32_t(entry.request.structStride) });
        }

        return infos;
    }
}
