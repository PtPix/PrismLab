#pragma once

// Framework types: the shadow feature contract.
//
// This is the first feature interface, written before its implementation. It defines what a shadow
// feature consumes and produces; the NVRHI execution layer (framework/nvrhi) and the
// algorithm shaders (algorithms/shaders/Shadows) implement it.
//
//   caster geometry + light projection -> ShadowMapPass -> ShadowMap
//                                                            |
//   camera depth + normals + matrices -----------------------+
//                                                            v
//                                                     ShadowResolvePass
//                                                            |
//                                                            v
//                                                      VisibilityTexture
//                                                            |
//                                                            v
//                                                  direct light for one light

#include "Conventions.h"
#include "LightData.h"
#include "Types.h"

#include <cstdint>

namespace renderlab
{
    enum class ShadowFilter : uint32_t
    {
        Hard = 0,
        PCF,
        PCSS,
        Count
    };

    enum class LightProjection : uint32_t
    {
        Perspective = 0,    // 聚光/点光
        Orthographic,       // 方向光
        Count
    };

    inline const char* ToString(ShadowFilter filter)
    {
        switch (filter)
        {
        case ShadowFilter::Hard: return "hard";
        case ShadowFilter::PCF:  return "PCF";
        case ShadowFilter::PCSS: return "PCSS";
        default:                 return "unknown";
        }
    }

    struct ShadowSettings
    {
        ShadowFilter filter = ShadowFilter::PCF;

        uint32_t blockerSamples = 16;   // PCSS 遮挡物搜索
        uint32_t filterSamples = 32;    // PCF/PCSS 过滤

        // 比较深度的偏移，不是米：光栅阶段的 depth/slope bias 与这里的接收阶段偏移分开配置
        float receiverBiasNdc = 0.0001f;

        // 接收点的世界空间法线偏移（米）
        float normalBiasMeters = 0.f;

        float pcfRadiusTexels = 2.f;

        // PCSS 半影估计：光源尺寸与遮挡物距离的关系
        float penumbraMinMeters = 0.01f;
        float penumbraMaxMeters = 0.5f;

        // 首版约定：阴影图之外的可见性为 1（限定投影范围的策略）
        float outsideShadowMapVisibility = 1.f;
    };

    struct ShadowView
    {
        dm::float4x4 worldToLightClip = dm::float4x4::identity();
        dm::float4x4 lightClipToWorld = dm::float4x4::identity();

        LightProjection projection = LightProjection::Orthographic;
        DepthConvention depthConvention = DepthConvention::ForwardZ0To1;

        float nearPlaneMeters = 0.1f;
        float farPlaneMeters = 100.f;

        // 仅透视聚光灯的 PCSS 路径使用
        float spotEmitterRadiusMeters = 0.f;

        // 仅方向光路径使用：不能把方向光的角尺寸当成聚光灯的米制半径
        float directionalAngularRadiusRadians = 0.f;

        uint64_t lightId = 0;

        [[nodiscard]] float LinearizeDepth(float deviceDepth) const
        {
            return renderlab::LinearizeDepth(deviceDepth, nearPlaneMeters, farPlaneMeters, depthConvention);
        }
    };

    struct ShadowDebugView
    {
        // 调试视图明确标出投影覆盖范围，避免把"图外可见"误解为"世界无遮挡"
        bool showShadowMap = false;
        bool showPcssBlockerDistance = false;
        bool showPenumbraRadius = false;
    };
}
