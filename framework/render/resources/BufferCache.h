#pragma once
#include "ResourceId.h"

// Persistent buffers keyed by ResourceId, with optional per-pixel sizing.

#include "Formats.h"

#include <framework/core/Status.h>
#include <framework/core/Types.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Prism::Gpu
{
	enum class EBufferUsage : uint32_t
	{
		None = 0,
		ShaderResource = 1u << 0,
		UnorderedAccess = 1u << 1,
		Constant = 1u << 2,		// 常量缓冲（cbuffer）
		IndirectArgs = 1u << 3, // 间接绘制/派发参数
	};

	constexpr EBufferUsage operator|(EBufferUsage A, EBufferUsage B)
	{
		return EBufferUsage(uint32_t(A) | uint32_t(B));
	}
	constexpr EBufferUsage operator&(EBufferUsage A, EBufferUsage B)
	{
		return EBufferUsage(uint32_t(A) & uint32_t(B));
	}
	constexpr bool HasAny(EBufferUsage Value, EBufferUsage Test)
	{
		return (uint32_t(Value) & uint32_t(Test)) != 0;
	}

	struct FBufferRequest
	{
		FResourceId Id = AllocateResourceId();
		std::string Name;
		EBufferUsage Usage = EBufferUsage::ShaderResource;

		// 结构化缓冲：元素步长（0 表示按 raw / 常量缓冲处理）
		uint64_t StructStride = 0;

		// 元素数量的三种来源，优先级：byteSize > elementsPerPixel > elementCount
		uint64_t ElementCount = 0;
		uint32_t ElementsPerPixel = 0; // 按渲染分辨率像素数 × 该系数（每像素一个 reservoir 时为 1）
		uint64_t ByteSize = 0;

		// 每帧由 CPU 写入。配合 BufferUsage::Constant 时创建为 volatile 常量缓冲，
		// 每帧可写入 maxVersions 次；其他用法下普通缓冲本身就支持 writeBuffer，无需该标志。
		bool bCpuWritable = false;

		// volatile 常量缓冲的每帧版本数；NVRHI 要求非零，仅在 cpuWritable + Constant 时有效。
		uint32_t MaxVersions = 16;

		[[nodiscard]] uint64_t ResolveByteSize(const FExtent2D& RenderSize) const;
		[[nodiscard]] uint32_t ResolveStride() const
		{
			return uint32_t(StructStride);
		}

		// 用法组合是否自相矛盾；返回的错误直接说明该改哪个字段。
		[[nodiscard]] FStatus Validate() const;
	};

	class FBufferCache
	{
	  public:
		explicit FBufferCache(nvrhi::IDevice* Device);

		// 渲染分辨率变化时调用：按分辨率计算的缓冲会被释放，下一帧按新尺寸重建。
		void SetRenderSize(const FExtent2D& RenderSize);
		[[nodiscard]] const FExtent2D& GetRenderSize() const
		{
			return RenderSize;
		}

		// Persistent cache keyed by request ID; names are diagnostic labels.
		nvrhi::IBuffer* GetOrCreate(const FBufferRequest& Request);
		nvrhi::IBuffer* Find(FResourceId Id);

		// 释放全部缓冲；调用前必须保证 GPU 已空闲。
		void Clear();

		struct FEntryInfo
		{
			std::string Name;
			uint64_t ByteSize = 0;
			uint32_t StructStride = 0;
		};

		[[nodiscard]] std::vector<FEntryInfo> GetEntries() const;

	  private:
		struct FEntry
		{
			FBufferRequest Request;
			nvrhi::BufferHandle Buffer;
			uint64_t ByteSize = 0;
		};

		FEntry* FindEntry(FResourceId Id);

		nvrhi::IDevice* Device = nullptr;
		FExtent2D RenderSize{1, 1};
		std::vector<FEntry> Entries;
	};
} // namespace Prism::Gpu
