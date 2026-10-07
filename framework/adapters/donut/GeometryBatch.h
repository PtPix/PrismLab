#pragma once

// NVRHI layer: backend view of drawable geometry, without Donut scene types.
//
// The Donut facilities (framework/adapters/donut) fill this from a scene graph; passes such as a shadow map, a depth
// prepass or a GBuffer fill consume it and bind their own shaders. The first version supports opaque,
// non-deforming triangles only: alpha test, skinning and displacement need explicit extensions, they
// must not be hidden behind this interface.

#include <framework/render/data/Conventions.h>
#include <framework/core/Types.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Prism::Gpu
{
	// 一组顶点/索引缓冲及其属性布局。glTF 场景可能包含多个缓冲组（每个模型一组），
	// 所以批次用下标引用它们，而不是假定全场只有一个缓冲。
	struct FGeometryBuffers
	{
		nvrhi::IBuffer* VertexBuffer = nullptr;
		nvrhi::IBuffer* IndexBuffer = nullptr;

		// 顶点属性在 vertexBuffer 中的字节范围；未使用的属性 byteSize 为 0。
		// 位置：float3，法线/切向：R8G8B8A8_SNORM 打包，UV：float2 —— 与 Donut 的 glTF 布局一致。
		nvrhi::BufferRange PositionRange;
		nvrhi::BufferRange TexCoordRange;
		nvrhi::BufferRange NormalRange;
		nvrhi::BufferRange TangentRange;

		nvrhi::Format IndexFormat = nvrhi::Format::R32_UINT;
		uint32_t VertexStride = 0; // 0 表示按属性范围分别绑定（非交错布局）

		[[nodiscard]] bool IsValid() const
		{
			return VertexBuffer != nullptr && IndexBuffer != nullptr;
		}
	};

	// One drawable triangle range with its object transform (CPU-side semantics: meters, right-handed).
	struct FDrawRecord
	{
		std::string DebugName;

		uint32_t BufferGroupIndex = 0;
		uint32_t MeshIndex = 0;
		uint32_t InstanceIndex = 0;
		uint32_t MaterialIndex = 0;

		// 元素索引，不是字节偏移
		uint32_t FirstIndex = 0;
		uint32_t IndexCount = 0;
		int32_t BaseVertex = 0;

		dm::affine3 ObjectToWorld = dm::affine3::identity();
		dm::affine3 PrevObjectToWorld = dm::affine3::identity();
		dm::box3 WorldBounds;

		[[nodiscard]] uint32_t TriangleCount() const
		{
			return IndexCount / 3;
		}
	};

	struct FGeometryBatch
	{
		std::vector<FGeometryBuffers> BufferGroups;
		std::vector<FDrawRecord> Draws;
		dm::box3 WorldBounds;

		[[nodiscard]] bool IsValid() const
		{
			return !BufferGroups.empty() && !Draws.empty();
		}

		[[nodiscard]] uint64_t GetTriangleCount() const
		{
			uint64_t Triangles = 0;
			for (const FDrawRecord& Draw : Draws)
				Triangles += Draw.TriangleCount();

			return Triangles;
		}
	};

	// 世界包围盒：dm::box3 已经提供仿射变换后的保守包围盒
	inline dm::box3 TransformBounds(const dm::box3& Bounds, const dm::affine3& Transform)
	{
		if (Bounds.isempty())
			return Bounds;

		return Bounds * Transform;
	}
} // namespace Prism::Gpu
