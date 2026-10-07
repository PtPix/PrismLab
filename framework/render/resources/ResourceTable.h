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

namespace Prism::Gpu
{
	class FTextureSlot
	{
	  public:
		FTextureSlot(const char* Name, EPixelFormat Format, ETextureUsage Usage, float ResolutionScale = 1.f,
					 FExtent2D ExplicitSize = FExtent2D{}, bool bHasClearValue = true,
					 dm::float4 ClearColor = dm::float4(0.f), float ClearDepth = KDepthClearValue)
		{
			Request.Name = Name;
			Request.Format = Format;
			Request.Usage = Usage;
			Request.ResolutionScale = ResolutionScale;
			Request.ExplicitSize = ExplicitSize;
			Request.bHasClearValue = bHasClearValue;
			Request.ClearColor = ClearColor;
			Request.ClearDepth = ClearDepth;
		}

		[[nodiscard]] const char* GetName() const
		{
			return Request.Name.c_str();
		}
		[[nodiscard]] const FTextureRequest& GetRequest() const
		{
			return Request;
		}

		// Change allocation policy while retaining the resource ID.
		FTextureRequest& MutableRequest()
		{
			return Request;
		}

	  private:
		FTextureRequest Request;
	};

	class FBufferSlot
	{
	  public:
		FBufferSlot(const char* Name, uint64_t StructStride, EBufferUsage Usage, uint32_t ElementsPerPixel = 0,
					uint64_t ElementCount = 0, uint64_t ByteSize = 0, bool bCpuWritable = false,
					uint32_t MaxVersions = 16)
		{
			Request.Name = Name;
			Request.StructStride = StructStride;
			Request.Usage = Usage;
			Request.ElementsPerPixel = ElementsPerPixel;
			Request.ElementCount = ElementCount;
			Request.ByteSize = ByteSize;
			Request.bCpuWritable = bCpuWritable;
			Request.MaxVersions = MaxVersions;
		}

		[[nodiscard]] const char* GetName() const
		{
			return Request.Name.c_str();
		}
		[[nodiscard]] const FBufferRequest& GetRequest() const
		{
			return Request;
		}
		FBufferRequest& MutableRequest()
		{
			return Request;
		}

	  private:
		FBufferRequest Request;
	};

	class FResourceTable
	{
	  public:
		FResourceTable(FTextureCache& InTextureCache, FBufferCache& InBufferCache)
			: TextureCache(InTextureCache), BufferCache(InBufferCache)
		{
		}

		nvrhi::ITexture* Get(const FTextureSlot& Slot)
		{
			return TextureCache.GetOrCreate(Slot.GetRequest());
		}
		nvrhi::IBuffer* Get(const FBufferSlot& Slot)
		{
			return BufferCache.GetOrCreate(Slot.GetRequest());
		}

		nvrhi::IFramebuffer* Framebuffer(const FTextureSlot& Color, const FTextureSlot* Depth = nullptr)
		{
			return TextureCache.GetFramebuffer(Get(Color), Depth ? Get(*Depth) : nullptr);
		}

		// 宿主在渲染分辨率或输出分辨率变化时调用（内部会重建按分辨率计算的资源）。
		void SetRenderSize(const FExtent2D& RenderSize)
		{
			TextureCache.SetRenderSize(RenderSize);
			BufferCache.SetRenderSize(RenderSize);
		}

		// 释放全部资源；调用前必须保证 GPU 已空闲。
		void Clear()
		{
			TextureCache.Clear();
			BufferCache.Clear();
		}

		[[nodiscard]] FTextureCache& GetTextureCache()
		{
			return TextureCache;
		}
		[[nodiscard]] FBufferCache& GetBufferCache()
		{
			return BufferCache;
		}

		// 调试/UI 用的清单：纹理与缓冲的名称、格式、尺寸，便于确认"谁申请了多大的资源"。
		struct FEntryInfo
		{
			std::string Name;
			std::string Kind;	// "texture" / "buffer"
			std::string Detail; // 尺寸与格式，或字节数
		};

		[[nodiscard]] std::vector<FEntryInfo> GetEntries() const;

	  private:
		FTextureCache& TextureCache;
		FBufferCache& BufferCache;
	};
} // namespace Prism::Gpu
