#include "TextureCache.h"

#include <donut/core/log.h>

#include <algorithm>

namespace Prism::Gpu
{
	FTextureCache::FTextureCache(nvrhi::IDevice* Device) : Device(Device)
	{
	}

	void FTextureCache::SetRenderSize(const FExtent2D& InRenderSize)
	{
		if (RenderSize == InRenderSize)
			return;

		RenderSize = InRenderSize;

		// 尺寸变化必须重建：这些纹理只在本帧及未来的帧里被借用，调用方已经等待 GPU 空闲。
		for (FEntry& Entry : Entries)
			if (!Entry.Request.ExplicitSize.IsValid())
				Entry.Texture = nullptr;

		Framebuffers.clear();
	}

	FTextureCache::FEntry* FTextureCache::FindEntry(FResourceId Id)
	{
		if (!Id)
			return nullptr;

		for (FEntry& Entry : Entries)
		{
			if (Entry.Request.Id == Id)
				return &Entry;
		}

		return nullptr;
	}

	FExtent2D FTextureCache::ResolveSize(const FTextureRequest& Request) const
	{
		if (Request.ExplicitSize.IsValid())
			return Request.ExplicitSize;

		return RenderSize.Scaled(Request.ResolutionScale);
	}

	nvrhi::ITexture* FTextureCache::GetOrCreate(const FTextureRequest& Request)
	{
		if (Request.Name.empty())
		{
			donut::log::error("TextureCache: texture requests must be named.");
			return nullptr;
		}

		const FExtent2D Size = ResolveSize(Request);

		FEntry* Entry = FindEntry(Request.Id);
		if (!Entry)
		{
			FEntry Created;
			Created.Request = Request;
			Created.Size = Size;
			Entries.push_back(std::move(Created));
			Entry = &Entries.back();
		}
		else
		{
			const bool bSizeChanged = Entry->Size != Size;
			const bool bFormatChanged = Entry->Request.Format != Request.Format;
			const bool bUsageChanged = Entry->Request.Usage != Request.Usage;
			const bool bLayoutChanged =
				Entry->Request.ArraySize != Request.ArraySize || Entry->Request.MipLevels != Request.MipLevels;

			if (bSizeChanged || bFormatChanged || bUsageChanged || bLayoutChanged)
			{
				Entry->Texture = nullptr;
				Entry->Size = Size;
				Framebuffers.clear();
			}

			Entry->Request = Request;
		}

		if (Entry->Texture)
			return Entry->Texture;

		const nvrhi::Format Format = ToNvrhiFormat(Request.Format);
		if (Format == nvrhi::Format::UNKNOWN)
		{
			donut::log::error("TextureCache: unsupported pixel format for '%s'.", Request.Name.c_str());
			return nullptr;
		}

		nvrhi::TextureDesc Desc;
		Desc.width = std::max(1u, Size.Width);
		Desc.height = std::max(1u, Size.Height);
		Desc.arraySize = std::max(1u, Request.ArraySize);
		Desc.mipLevels = std::max(1u, Request.MipLevels);
		Desc.format = Format;
		Desc.debugName = Request.Name;
		Desc.isShaderResource = HasAny(Request.Usage, ETextureUsage::ShaderResource);
		Desc.isRenderTarget =
			HasAny(Request.Usage, ETextureUsage::RenderTarget) || HasAny(Request.Usage, ETextureUsage::DepthStencil);
		Desc.isUAV = HasAny(Request.Usage, ETextureUsage::UnorderedAccess);

		if (Request.bHasClearValue && Desc.isRenderTarget && IsDepthFormat(Request.Format))
		{
			Desc.useClearValue = true;
			Desc.clearValue = nvrhi::Color(Request.ClearDepth, 0.f, 0.f, 0.f);
		}
		else if (Request.bHasClearValue && Desc.isRenderTarget)
		{
			Desc.useClearValue = true;
			Desc.clearValue =
				nvrhi::Color(Request.ClearColor.x, Request.ClearColor.y, Request.ClearColor.z, Request.ClearColor.w);
		}

		Desc.enableAutomaticStateTracking(GetInitialState(Request.Usage, Request.Format));

		Entry->Texture = Device->createTexture(Desc);
		Entry->Size = Size;

		if (!Entry->Texture)
			donut::log::error("TextureCache: failed to create texture '%s'.", Request.Name.c_str());

		return Entry->Texture;
	}

	nvrhi::ITexture* FTextureCache::Find(FResourceId Id)
	{
		FEntry* Entry = FindEntry(Id);
		return Entry ? Entry->Texture.Get() : nullptr;
	}

	nvrhi::IFramebuffer* FTextureCache::GetFramebuffer(nvrhi::ITexture* Color, nvrhi::ITexture* Depth)
	{
		std::vector<nvrhi::ITexture*> Colors;
		if (Color)
			Colors.push_back(Color);

		return GetFramebuffer(Colors, Depth);
	}

	nvrhi::IFramebuffer* FTextureCache::GetFramebuffer(const std::vector<nvrhi::ITexture*>& Colors,
													   nvrhi::ITexture* Depth)
	{
		if (Colors.empty() && !Depth)
			return nullptr;

		FFramebufferKey Key;
		Key.Attachments = Colors;
		Key.Attachments.push_back(Depth);

		for (const auto& Cached : Framebuffers)
		{
			if (Cached.first == Key)
				return Cached.second.Get();
		}

		nvrhi::FramebufferDesc Desc;
		for (nvrhi::ITexture* Color : Colors)
		{
			if (Color)
				Desc.addColorAttachment(Color);
		}

		if (Depth)
			Desc.setDepthAttachment(Depth);

		nvrhi::FramebufferHandle Framebuffer = Device->createFramebuffer(Desc);
		if (!Framebuffer)
			return nullptr;

		Framebuffers.emplace_back(Key, Framebuffer);
		return Framebuffer.Get();
	}

	void FTextureCache::Clear()
	{
		Framebuffers.clear();
		Entries.clear();
	}

	std::vector<FTextureCache::FEntryInfo> FTextureCache::GetEntries() const
	{
		std::vector<FEntryInfo> Infos;
		Infos.reserve(Entries.size());

		for (const FEntry& Entry : Entries)
		{
			Infos.push_back(FEntryInfo{Entry.Request.Name, Entry.Request.Format, Entry.Request.Usage,
									   Entry.Texture ? Entry.Size : FExtent2D{}});
		}

		return Infos;
	}
} // namespace Prism::Gpu
