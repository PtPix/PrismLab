#pragma once
#include "ResourceId.h"

// Persistent textures and framebuffer caches, keyed by ResourceId.
// Requests retain identity across copies. Labels are only for diagnostics.
// Callers synchronize before clearing/replacing resources still in use.

#include "Formats.h"

#include <framework/render/data/Conventions.h>
#include <framework/core/Types.h>

#include <nvrhi/nvrhi.h>

#include <string>
#include <vector>

namespace Prism::Gpu
{
	struct FTextureRequest
	{
		FResourceId Id = AllocateResourceId();
		std::string Name;
		EPixelFormat Format = EPixelFormat::RgbA16Float;
		ETextureUsage Usage = ETextureUsage::ShaderResource | ETextureUsage::RenderTarget;

		// 相对宿主渲染分辨率的比例；explicitSize 有效时忽略
		float ResolutionScale = 1.f;

		// 固定尺寸（例如阴影图、历史缓冲），为 0 时跟随 resolutionScale
		FExtent2D ExplicitSize;

		uint32_t ArraySize = 1;
		uint32_t MipLevels = 1;

		dm::float4 ClearColor = dm::float4(0.f, 0.f, 0.f, 0.f);
		float ClearDepth = KDepthClearValue;
		bool bHasClearValue = true;
	};

	class FTextureCache
	{
	  public:
		explicit FTextureCache(nvrhi::IDevice* Device);

		// 宿主渲染分辨率变化时调用：所有跟随分辨率的纹理被释放，下一帧按新尺寸重建。
		void SetRenderSize(const FExtent2D& RenderSize);
		[[nodiscard]] const FExtent2D& GetRenderSize() const
		{
			return RenderSize;
		}

		// Persistent cache keyed by request ID; names are diagnostic labels.
		nvrhi::ITexture* GetOrCreate(const FTextureRequest& Request);

		// 只查找，不存在时返回 nullptr；不隐式创建，便于发现拼写错误。
		nvrhi::ITexture* Find(FResourceId Id);

		nvrhi::IFramebuffer* GetFramebuffer(nvrhi::ITexture* Color, nvrhi::ITexture* Depth = nullptr);
		nvrhi::IFramebuffer* GetFramebuffer(const std::vector<nvrhi::ITexture*>& Colors,
											nvrhi::ITexture* Depth = nullptr);

		// 释放所有纹理与 framebuffer；调用前必须保证 GPU 已空闲。
		void Clear();

		struct FEntryInfo
		{
			std::string Name;
			EPixelFormat Format = EPixelFormat::Unknown;
			ETextureUsage Usage = ETextureUsage::None;
			FExtent2D Size;
		};

		[[nodiscard]] std::vector<FEntryInfo> GetEntries() const;
		[[nodiscard]] size_t GetTextureCount() const
		{
			return Entries.size();
		}

	  private:
		struct FEntry
		{
			FTextureRequest Request;
			nvrhi::TextureHandle Texture;
			FExtent2D Size;
		};

		FEntry* FindEntry(FResourceId Id);
		FExtent2D ResolveSize(const FTextureRequest& Request) const;

		nvrhi::IDevice* Device = nullptr;
		FExtent2D RenderSize{1, 1};
		std::vector<FEntry> Entries;

		struct FFramebufferKey
		{
			std::vector<nvrhi::ITexture*> Attachments;

			bool operator==(const FFramebufferKey& Other) const
			{
				return Attachments == Other.Attachments;
			}
			struct FHash
			{
				size_t operator()(const FFramebufferKey& Key) const
				{
					size_t Hash = 1469598103934665603ull;
					for (const nvrhi::ITexture* Texture : Key.Attachments)
					{
						Hash ^= size_t(Texture);
						Hash *= 1099511628211ull;
					}
					return Hash;
				}
			};
		};

		std::vector<std::pair<FFramebufferKey, nvrhi::FramebufferHandle>> Framebuffers;
	};
} // namespace Prism::Gpu
