#include "BufferCache.h"

#include <donut/core/log.h>

#include <algorithm>

namespace Prism::Gpu
{
	FStatus FBufferRequest::Validate() const
	{
		const bool bConstant = HasAny(Usage, EBufferUsage::Constant);

		// NVRHI: 只有常量缓冲能是 volatile，且 maxVersions 必须非零；volatile 不能同时是 UAV。
		if (bCpuWritable && bConstant)
		{
			if (MaxVersions == 0)
				return FStatus::Error(EErrorCode::InvalidArgument, "a volatile constant buffer needs maxVersions > 0");

			if (HasAny(Usage, EBufferUsage::UnorderedAccess | EBufferUsage::IndirectArgs))
				return FStatus::Error(EErrorCode::Unsupported,
									  "a volatile constant buffer cannot also be a UAV or indirect argument buffer");
		}

		if (bConstant && StructStride > 0)
			return FStatus::Error(EErrorCode::InvalidArgument, "a constant buffer cannot have a struct stride");

		return FStatus::Ok();
	}

	uint64_t FBufferRequest::ResolveByteSize(const FExtent2D& RenderSize) const
	{
		if (ByteSize > 0)
			return ByteSize;

		uint64_t Count = ElementCount;
		if (ElementsPerPixel > 0)
			Count = uint64_t(RenderSize.Width) * uint64_t(RenderSize.Height) * uint64_t(ElementsPerPixel);

		if (Count == 0)
			return 0;

		const uint64_t Stride = (StructStride > 0) ? StructStride : 4;
		return Count * Stride;
	}

	FBufferCache::FBufferCache(nvrhi::IDevice* Device) : Device(Device)
	{
	}

	void FBufferCache::SetRenderSize(const FExtent2D& InRenderSize)
	{
		if (RenderSize == InRenderSize)
			return;

		RenderSize = InRenderSize;

		// 只有按分辨率计算的缓冲需要重建，固定尺寸的保留。
		for (FEntry& Entry : Entries)
		{
			if (Entry.Request.ElementsPerPixel > 0)
				Entry.Buffer = nullptr;
		}
	}

	FBufferCache::FEntry* FBufferCache::FindEntry(FResourceId Id)
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

	nvrhi::IBuffer* FBufferCache::GetOrCreate(const FBufferRequest& Request)
	{
		if (Request.Name.empty())
		{
			donut::log::error("BufferCache: buffer requests must be named.");
			return nullptr;
		}

		if (const FStatus Valid = Request.Validate(); !Valid)
		{
			donut::log::error("BufferCache: '%s' has an invalid usage combination: %s", Request.Name.c_str(),
							  Valid.ToStringWithCode().c_str());
			return nullptr;
		}

		const uint64_t ByteSize = Request.ResolveByteSize(RenderSize);
		if (ByteSize == 0)
		{
			donut::log::error("BufferCache: '%s' resolves to zero bytes.", Request.Name.c_str());
			return nullptr;
		}

		FEntry* Entry = FindEntry(Request.Id);
		if (!Entry)
		{
			FEntry Created;
			Created.Request = Request;
			Created.ByteSize = ByteSize;
			Entries.push_back(std::move(Created));
			Entry = &Entries.back();
		}
		else
		{
			const bool bByteSizeChanged = Entry->ByteSize != ByteSize;
			const bool bLayoutChanged =
				Entry->Request.Usage != Request.Usage || Entry->Request.StructStride != Request.StructStride;

			if (bByteSizeChanged || bLayoutChanged)
				Entry->Buffer = nullptr;

			Entry->Request = Request;
			Entry->ByteSize = ByteSize;
		}

		if (Entry->Buffer)
			return Entry->Buffer;

		// 结构化视图由非零 structStride 表达；stride 为 0 时着色器可见性只能通过 raw view 获得。
		const bool bShaderResource = HasAny(Request.Usage, EBufferUsage::ShaderResource);
		const bool bUnorderedAccess = HasAny(Request.Usage, EBufferUsage::UnorderedAccess);
		const bool bStructured = Request.StructStride > 0;

		nvrhi::BufferDesc Desc;
		Desc.byteSize = ByteSize;
		Desc.structStride = uint32_t(Request.StructStride);
		Desc.debugName = Request.Name;
		Desc.canHaveUAVs = bUnorderedAccess;
		Desc.canHaveRawViews = !bStructured && (bShaderResource || bUnorderedAccess);
		Desc.isConstantBuffer = HasAny(Request.Usage, EBufferUsage::Constant);
		Desc.isDrawIndirectArgs = HasAny(Request.Usage, EBufferUsage::IndirectArgs);
		Desc.isVolatile = Request.bCpuWritable && Desc.isConstantBuffer;
		Desc.maxVersions = Desc.isVolatile ? Request.MaxVersions : 0;
		Desc.initialState = nvrhi::ResourceStates::Common;

		Entry->Buffer = Device->createBuffer(Desc);
		if (!Entry->Buffer)
			donut::log::error("BufferCache: failed to create buffer '%s' (%llu bytes).", Request.Name.c_str(),
							  (unsigned long long)ByteSize);

		return Entry->Buffer;
	}

	nvrhi::IBuffer* FBufferCache::Find(FResourceId Id)
	{
		FEntry* Entry = FindEntry(Id);
		return Entry ? Entry->Buffer.Get() : nullptr;
	}

	void FBufferCache::Clear()
	{
		Entries.clear();
	}

	std::vector<FBufferCache::FEntryInfo> FBufferCache::GetEntries() const
	{
		std::vector<FEntryInfo> Infos;
		Infos.reserve(Entries.size());

		for (const FEntry& Entry : Entries)
		{
			Infos.push_back(FEntryInfo{Entry.Request.Name, Entry.Buffer ? Entry.ByteSize : 0,
									   uint32_t(Entry.Request.StructStride)});
		}

		return Infos;
	}
} // namespace Prism::Gpu
