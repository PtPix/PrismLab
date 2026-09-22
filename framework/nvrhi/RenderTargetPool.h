#pragma once

// NVRHI layer: named transient render targets.
//
// Experiments declare the textures they need (name, format, usage, size relative to the render resolution);
// the pool creates them, hands out framebuffers and rebuilds everything when the render size changes.
// This is what keeps window resizing, format plumbing and framebuffer caching out of experiment code.
//
// Ownership rules: the pool owns the textures for the frame; a pass only
// borrows them for the duration of the GPU work it records. Textures are released in Clear(), which
// the host calls after waiting for GPU idle.

#include "Formats.h"

#include <framework/types/Conventions.h>
#include <framework/types/Types.h>

#include <nvrhi/nvrhi.h>

#include <string>
#include <vector>

namespace prism::gpu
{
    struct TextureRequest
    {
        std::string name;
        PixelFormat format = PixelFormat::RGBA16_FLOAT;
        TextureUsage usage = TextureUsage::ShaderResource | TextureUsage::RenderTarget;

        // 相对宿主渲染分辨率的比例；explicitSize 有效时忽略
        float resolutionScale = 1.f;

        // 固定尺寸（例如阴影图、历史缓冲），为 0 时跟随 resolutionScale
        Extent2D explicitSize;

        uint32_t arraySize = 1;
        uint32_t mipLevels = 1;

        dm::float4 clearColor = dm::float4(0.f, 0.f, 0.f, 0.f);
        float clearDepth = kDepthClearValue;
        bool hasClearValue = true;
    };

    class RenderTargetPool
    {
    public:
        explicit RenderTargetPool(nvrhi::IDevice* device);

        // 宿主渲染分辨率变化时调用：所有跟随分辨率的纹理被释放，下一帧按新尺寸重建。
        void SetRenderSize(const Extent2D& renderSize);
        [[nodiscard]] const Extent2D& GetRenderSize() const { return m_RenderSize; }

        // 按 name 取得（必要时创建）纹理。尺寸、格式或用法变化时旧纹理被替换。
        nvrhi::ITexture* GetOrCreate(const TextureRequest& request);

        // 只查找，不存在时返回 nullptr；不隐式创建，便于发现拼写错误。
        nvrhi::ITexture* Find(const char* name);

        nvrhi::IFramebuffer* GetFramebuffer(nvrhi::ITexture* color, nvrhi::ITexture* depth = nullptr);
        nvrhi::IFramebuffer* GetFramebuffer(const std::vector<nvrhi::ITexture*>& colors, nvrhi::ITexture* depth = nullptr);

        // 释放所有纹理与 framebuffer；调用前必须保证 GPU 已空闲。
        void Clear();

        struct EntryInfo
        {
            std::string name;
            PixelFormat format = PixelFormat::Unknown;
            TextureUsage usage = TextureUsage::None;
            Extent2D size;
        };

        [[nodiscard]] std::vector<EntryInfo> GetEntries() const;
        [[nodiscard]] size_t GetTextureCount() const { return m_Entries.size(); }

    private:
        struct Entry
        {
            TextureRequest request;
            nvrhi::TextureHandle texture;
            Extent2D size;
        };

        Entry* FindEntry(const char* name);
        Extent2D ResolveSize(const TextureRequest& request) const;

        nvrhi::IDevice* m_Device = nullptr;
        Extent2D m_RenderSize{ 1, 1 };
        std::vector<Entry> m_Entries;

        struct FramebufferKey
        {
            std::vector<nvrhi::ITexture*> attachments;

            bool operator==(const FramebufferKey& other) const { return attachments == other.attachments; }
            struct Hash
            {
                size_t operator()(const FramebufferKey& key) const
                {
                    size_t hash = 1469598103934665603ull;
                    for (const nvrhi::ITexture* texture : key.attachments)
                    {
                        hash ^= size_t(texture);
                        hash *= 1099511628211ull;
                    }
                    return hash;
                }
            };
        };

        std::vector<std::pair<FramebufferKey, nvrhi::FramebufferHandle>> m_Framebuffers;
    };
}
