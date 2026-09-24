#include "BufferCache.h"

#include <donut/core/log.h>

#include <algorithm>

namespace prism::gpu
{
    Status BufferRequest::Validate() const
    {
        const bool constant = HasAny(usage, BufferUsage::Constant);

        // NVRHI: 只有常量缓冲能是 volatile，且 maxVersions 必须非零；volatile 不能同时是 UAV。
        if (cpuWritable && constant)
        {
            if (maxVersions == 0)
                return Status::Error(ErrorCode::InvalidArgument, "a volatile constant buffer needs maxVersions > 0");

            if (HasAny(usage, BufferUsage::UnorderedAccess | BufferUsage::IndirectArgs))
                return Status::Error(ErrorCode::Unsupported,
                    "a volatile constant buffer cannot also be a UAV or indirect argument buffer");
        }

        if (constant && structStride > 0)
            return Status::Error(ErrorCode::InvalidArgument, "a constant buffer cannot have a struct stride");

        return Status::Ok();
    }

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

    BufferCache::BufferCache(nvrhi::IDevice* device)
        : m_Device(device)
    {
    }

    void BufferCache::SetRenderSize(const Extent2D& renderSize)
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

    BufferCache::Entry* BufferCache::FindEntry(ResourceId id)
    {
        if (!id)
            return nullptr;

        for (Entry& entry : m_Entries)
        {
            if (entry.request.id == id)
                return &entry;
        }

        return nullptr;
    }

    nvrhi::IBuffer* BufferCache::GetOrCreate(const BufferRequest& request)
    {
        if (request.name.empty())
        {
            donut::log::error("BufferCache: buffer requests must be named.");
            return nullptr;
        }

        if (const Status valid = request.Validate(); !valid)
        {
            donut::log::error("BufferCache: '%s' has an invalid usage combination: %s",
                request.name.c_str(), valid.ToStringWithCode().c_str());
            return nullptr;
        }

        const uint64_t byteSize = request.ResolveByteSize(m_RenderSize);
        if (byteSize == 0)
        {
            donut::log::error("BufferCache: '%s' resolves to zero bytes.", request.name.c_str());
            return nullptr;
        }

        Entry* entry = FindEntry(request.id);
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

        // 结构化视图由非零 structStride 表达；stride 为 0 时着色器可见性只能通过 raw view 获得。
        const bool shaderResource = HasAny(request.usage, BufferUsage::ShaderResource);
        const bool unorderedAccess = HasAny(request.usage, BufferUsage::UnorderedAccess);
        const bool structured = request.structStride > 0;

        nvrhi::BufferDesc desc;
        desc.byteSize = byteSize;
        desc.structStride = uint32_t(request.structStride);
        desc.debugName = request.name;
        desc.canHaveUAVs = unorderedAccess;
        desc.canHaveRawViews = !structured && (shaderResource || unorderedAccess);
        desc.isConstantBuffer = HasAny(request.usage, BufferUsage::Constant);
        desc.isDrawIndirectArgs = HasAny(request.usage, BufferUsage::IndirectArgs);
        desc.isVolatile = request.cpuWritable && desc.isConstantBuffer;
        desc.maxVersions = desc.isVolatile ? request.maxVersions : 0;
        desc.initialState = nvrhi::ResourceStates::Common;

        entry->buffer = m_Device->createBuffer(desc);
        if (!entry->buffer)
            donut::log::error("BufferCache: failed to create buffer '%s' (%llu bytes).",
                request.name.c_str(), (unsigned long long)byteSize);

        return entry->buffer;
    }

    nvrhi::IBuffer* BufferCache::Find(ResourceId id)
    {
        Entry* entry = FindEntry(id);
        return entry ? entry->buffer.Get() : nullptr;
    }

    void BufferCache::Clear()
    {
        m_Entries.clear();
    }

    std::vector<BufferCache::EntryInfo> BufferCache::GetEntries() const
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
