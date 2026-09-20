#include "RenderTargetPool.h"

#include <donut/core/log.h>

#include <algorithm>

namespace renderlab::gpu
{
    RenderTargetPool::RenderTargetPool(nvrhi::IDevice* device)
        : m_Device(device)
    {
    }

    void RenderTargetPool::SetRenderSize(const Extent2D& renderSize)
    {
        if (m_RenderSize == renderSize)
            return;

        m_RenderSize = renderSize;

        // 尺寸变化必须重建：这些纹理只在本帧及未来的帧里被借用，调用方已经等待 GPU 空闲。
        for (Entry& entry : m_Entries)
            entry.texture = nullptr;

        m_Framebuffers.clear();
    }

    RenderTargetPool::Entry* RenderTargetPool::FindEntry(const char* name)
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

    Extent2D RenderTargetPool::ResolveSize(const TextureRequest& request) const
    {
        if (request.explicitSize.IsValid())
            return request.explicitSize;

        return m_RenderSize.Scaled(request.resolutionScale);
    }

    nvrhi::ITexture* RenderTargetPool::GetOrCreate(const TextureRequest& request)
    {
        if (request.name.empty())
        {
            donut::log::error("RenderTargetPool: texture requests must be named.");
            return nullptr;
        }

        const Extent2D size = ResolveSize(request);

        Entry* entry = FindEntry(request.name.c_str());
        if (!entry)
        {
            Entry created;
            created.request = request;
            created.size = size;
            m_Entries.push_back(std::move(created));
            entry = &m_Entries.back();
        }
        else
        {
            const bool sizeChanged = entry->size != size;
            const bool formatChanged = entry->request.format != request.format;
            const bool usageChanged = entry->request.usage != request.usage;
            const bool layoutChanged = entry->request.arraySize != request.arraySize || entry->request.mipLevels != request.mipLevels;

            if (sizeChanged || formatChanged || usageChanged || layoutChanged)
            {
                entry->texture = nullptr;
                entry->size = size;
                m_Framebuffers.clear();
            }

            entry->request = request;
        }

        if (entry->texture)
            return entry->texture;

        const nvrhi::Format format = ToNvrhiFormat(request.format);
        if (format == nvrhi::Format::UNKNOWN)
        {
            donut::log::error("RenderTargetPool: unsupported pixel format for '%s'.", request.name.c_str());
            return nullptr;
        }

        nvrhi::TextureDesc desc;
        desc.width = std::max(1u, size.width);
        desc.height = std::max(1u, size.height);
        desc.arraySize = std::max(1u, request.arraySize);
        desc.mipLevels = std::max(1u, request.mipLevels);
        desc.format = format;
        desc.debugName = request.name;
        desc.isShaderResource = HasAny(request.usage, TextureUsage::ShaderResource);
        desc.isRenderTarget = HasAny(request.usage, TextureUsage::RenderTarget) || HasAny(request.usage, TextureUsage::DepthStencil);
        desc.isUAV = HasAny(request.usage, TextureUsage::UnorderedAccess);

        if (request.hasClearValue && IsDepthFormat(request.format))
        {
            desc.useClearValue = true;
            desc.clearValue = nvrhi::Color(request.clearDepth, 0.f, 0.f, 0.f);
        }
        else if (request.hasClearValue)
        {
            desc.useClearValue = true;
            desc.clearValue = nvrhi::Color(request.clearColor.x, request.clearColor.y, request.clearColor.z, request.clearColor.w);
        }

        desc.enableAutomaticStateTracking(GetInitialState(request.usage, request.format));

        entry->texture = m_Device->createTexture(desc);
        entry->size = size;

        if (!entry->texture)
            donut::log::error("RenderTargetPool: failed to create texture '%s'.", request.name.c_str());

        return entry->texture;
    }

    nvrhi::ITexture* RenderTargetPool::Find(const char* name)
    {
        Entry* entry = FindEntry(name);
        return entry ? entry->texture.Get() : nullptr;
    }

    nvrhi::IFramebuffer* RenderTargetPool::GetFramebuffer(nvrhi::ITexture* color, nvrhi::ITexture* depth)
    {
        std::vector<nvrhi::ITexture*> colors;
        if (color)
            colors.push_back(color);

        return GetFramebuffer(colors, depth);
    }

    nvrhi::IFramebuffer* RenderTargetPool::GetFramebuffer(const std::vector<nvrhi::ITexture*>& colors, nvrhi::ITexture* depth)
    {
        if (colors.empty() && !depth)
            return nullptr;

        FramebufferKey key;
        key.attachments = colors;
        key.attachments.push_back(depth);

        for (const auto& cached : m_Framebuffers)
        {
            if (cached.first == key)
                return cached.second.Get();
        }

        nvrhi::FramebufferDesc desc;
        for (nvrhi::ITexture* color : colors)
        {
            if (color)
                desc.addColorAttachment(color);
        }

        if (depth)
            desc.setDepthAttachment(depth);

        nvrhi::FramebufferHandle framebuffer = m_Device->createFramebuffer(desc);
        if (!framebuffer)
            return nullptr;

        m_Framebuffers.emplace_back(key, framebuffer);
        return framebuffer.Get();
    }

    void RenderTargetPool::Clear()
    {
        m_Framebuffers.clear();
        m_Entries.clear();
    }

    std::vector<RenderTargetPool::EntryInfo> RenderTargetPool::GetEntries() const
    {
        std::vector<EntryInfo> infos;
        infos.reserve(m_Entries.size());

        for (const Entry& entry : m_Entries)
        {
            infos.push_back(EntryInfo{
                entry.request.name,
                entry.request.format,
                entry.request.usage,
                entry.texture ? entry.size : Extent2D{} });
        }

        return infos;
    }
}
