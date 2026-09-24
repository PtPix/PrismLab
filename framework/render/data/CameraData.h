#pragma once

// Framework types: camera and view semantics.
//
// The matrices follow Conventions.h: world space is right-handed, view space is left-handed, and the
// projection is D3D style with z in [0, 1]. "previous" matrices are what a temporal feature needs to
// reproject the last frame; they are only valid when hasPrevious is true.

#include "Conventions.h"
#include "framework/core/Types.h"

namespace prism
{
    struct ViewMatrices
    {
        dm::affine3 worldToView = dm::affine3::identity();
        dm::affine3 viewToWorld = dm::affine3::identity();
        dm::float4x4 viewToClip = dm::float4x4::identity();
        dm::float4x4 clipToView = dm::float4x4::identity();
        dm::float4x4 worldToClip = dm::float4x4::identity();
        dm::float4x4 clipToWorld = dm::float4x4::identity();

        // 组合顺序遵循 Donut 与 HLSL 的行向量约定：clip = world * worldToView * viewToClip，
        // 因此 worldToClip = worldToView * viewToClip（dm 的矩阵乘法即普通行主序乘积）。
        static ViewMatrices Build(const dm::affine3& worldToView, const dm::float4x4& viewToClip)
        {
            ViewMatrices matrices;
            matrices.worldToView = worldToView;
            matrices.viewToWorld = dm::inverse(worldToView);
            matrices.viewToClip = viewToClip;
            matrices.clipToView = dm::inverse(viewToClip);
            matrices.worldToClip = dm::affineToHomogeneous(worldToView) * viewToClip;
            matrices.clipToWorld = dm::inverse(matrices.worldToClip);
            return matrices;
        }
    };

    struct CameraData
    {
        // 相机世界状态（右手系，Y 轴向上，单位米）
        dm::float3 position = dm::float3(0.f);
        dm::float3 forward = dm::float3(0.f, 0.f, 1.f);
        dm::float3 up = dm::float3(0.f, 1.f, 0.f);
        dm::float3 right = dm::float3(1.f, 0.f, 0.f);

        float verticalFovRadians = dm::radians(60.f);
        float aspectRatio = 16.f / 9.f;
        float zNearMeters = 0.05f;
        float zFarMeters = 100.f;

        DepthConvention depthConvention = DepthConvention::ForwardZ0To1;

        ViewMatrices current;
        ViewMatrices previous;
        bool hasPrevious = false;

        dm::float2 jitter = dm::float2(0.f);
        dm::float2 previousJitter = dm::float2(0.f);

        // 设备深度 -> 相机前方线性距离（米）
        [[nodiscard]] float LinearizeDepth(float deviceDepth) const
        {
            return prism::LinearizeDepth(deviceDepth, zNearMeters, zFarMeters, depthConvention);
        }

        // uv 原点在左上；返回值可能落在 [0,1] 之外，表示视锥外
        [[nodiscard]] dm::float2 WorldToUnjitteredUv(const dm::float3& worldPosition) const
        {
            dm::float4 clip = dm::float4(worldPosition, 1.f) * current.worldToClip;
            if (std::fabs(clip.w) < 1e-8f)
                return dm::float2(-1.f);

            const dm::float2 ndc = dm::float2(clip.x / clip.w, clip.y / clip.w);
            return dm::float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);
        }

        // 与 WorldToUnjitteredUv 相反：由无抖动 uv 与设备深度重建世界位置
        [[nodiscard]] dm::float3 ReconstructWorldPosition(const dm::float2& uv, float deviceDepth) const
        {
            const dm::float4 clip = dm::float4(
                uv.x * 2.f - 1.f,
                (1.f - uv.y) * 2.f - 1.f,
                deviceDepth,
                1.f);

            const dm::float4 world = clip * current.clipToWorld;
            if (std::fabs(world.w) < 1e-8f)
                return position;

            return dm::float3(world.x / world.w, world.y / world.w, world.z / world.w);
        }
    };
}
