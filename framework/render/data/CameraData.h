#pragma once

// Framework types: camera and view semantics.
//
// The matrices follow Conventions.h: world space is right-handed, view space is left-handed, and the
// projection is D3D style with z in [0, 1]. "previous" matrices are what a temporal feature needs to
// reproject the last frame; they are only valid when hasPrevious is true.

#include "Conventions.h"
#include "framework/core/Types.h"

namespace Prism
{
	struct FViewMatrices
	{
		dm::affine3 WorldToView = dm::affine3::identity();
		dm::affine3 ViewToWorld = dm::affine3::identity();
		dm::float4x4 ViewToClip = dm::float4x4::identity();
		dm::float4x4 ClipToView = dm::float4x4::identity();
		dm::float4x4 WorldToClip = dm::float4x4::identity();
		dm::float4x4 ClipToWorld = dm::float4x4::identity();

		// 组合顺序遵循 Donut 与 HLSL 的行向量约定：clip = world * worldToView * viewToClip，
		// 因此 worldToClip = worldToView * viewToClip（dm 的矩阵乘法即普通行主序乘积）。
		static FViewMatrices Build(const dm::affine3& WorldToView, const dm::float4x4& ViewToClip)
		{
			FViewMatrices Matrices;
			Matrices.WorldToView = WorldToView;
			Matrices.ViewToWorld = dm::inverse(WorldToView);
			Matrices.ViewToClip = ViewToClip;
			Matrices.ClipToView = dm::inverse(ViewToClip);
			Matrices.WorldToClip = dm::affineToHomogeneous(WorldToView) * ViewToClip;
			Matrices.ClipToWorld = dm::inverse(Matrices.WorldToClip);
			return Matrices;
		}
	};

	struct FCameraData
	{
		// 相机世界状态（右手系，Y 轴向上，单位米）
		dm::float3 Position = dm::float3(0.f);
		dm::float3 Forward = dm::float3(0.f, 0.f, 1.f);
		dm::float3 Up = dm::float3(0.f, 1.f, 0.f);
		dm::float3 Right = dm::float3(1.f, 0.f, 0.f);

		float VerticalFovRadians = dm::radians(60.f);
		float AspectRatio = 16.f / 9.f;
		float ZNearMeters = 0.05f;
		float ZFarMeters = 100.f;

		EDepthConvention DepthConvention = EDepthConvention::ForwardZ0To1;

		FViewMatrices Current;
		FViewMatrices Previous;
		bool bHasPrevious = false;

		dm::float2 Jitter = dm::float2(0.f);
		dm::float2 PreviousJitter = dm::float2(0.f);

		// 设备深度 -> 相机前方线性距离（米）
		[[nodiscard]] float LinearizeDepth(float DeviceDepth) const
		{
			return Prism::LinearizeDepth(DeviceDepth, ZNearMeters, ZFarMeters, DepthConvention);
		}

		// uv 原点在左上；返回值可能落在 [0,1] 之外，表示视锥外
		[[nodiscard]] dm::float2 WorldToUnjitteredUv(const dm::float3& WorldPosition) const
		{
			dm::float4 Clip = dm::float4(WorldPosition, 1.f) * Current.WorldToClip;
			if (std::fabs(Clip.w) < 1e-8f)
				return dm::float2(-1.f);

			const dm::float2 Ndc = dm::float2(Clip.x / Clip.w, Clip.y / Clip.w);
			return dm::float2(Ndc.x * 0.5f + 0.5f, 0.5f - Ndc.y * 0.5f);
		}

		// 与 WorldToUnjitteredUv 相反：由无抖动 uv 与设备深度重建世界位置
		[[nodiscard]] dm::float3 ReconstructWorldPosition(const dm::float2& Uv, float DeviceDepth) const
		{
			const dm::float4 Clip = dm::float4(Uv.x * 2.f - 1.f, (1.f - Uv.y) * 2.f - 1.f, DeviceDepth, 1.f);

			const dm::float4 World = Clip * Current.ClipToWorld;
			if (std::fabs(World.w) < 1e-8f)
				return Position;

			return dm::float3(World.x / World.w, World.y / World.w, World.z / World.w);
		}
	};
} // namespace Prism
