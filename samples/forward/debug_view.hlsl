// ForwardExperiment debug view: visualizes what the shared scene pipeline wrote into the depth buffer.
//
// This is the smallest useful example of an experiment-written pass: it owns its constant layout
// (debug_view_cb.h), binds the pool's depth texture and renders with Donut's fullscreen VS.

// 平台约定（矩阵行主序、深度约定、颜色空间）：必须最先包含
#include "Prism/Common/Platform.hlsli"

#include "debug_view_cb.h"

ConstantBuffer<DebugViewConstants> g_Debug : register(b0);

Texture2D<float> g_SceneDepth : register(t0);
SamplerState g_PointClamp : register(s0);

float LinearizeDepth(float deviceDepth)
{
    if (g_Debug.depthConvention == 1)
        deviceDepth = 1.f - deviceDepth;

    const float zNear = g_Debug.zNear;
    const float zFar = g_Debug.zFar;
    return zNear * zFar / (zFar - deviceDepth * (zFar - zNear));
}

float3 ReconstructWorldPosition(float2 uv, float deviceDepth)
{
    // 行向量约定：clip = world * worldToClip，因此 world = clip * clipToWorld
    float4 clip = float4(uv.x * 2.f - 1.f, (1.f - uv.y) * 2.f - 1.f, deviceDepth, 1.f);
    float4 world = mul(clip, g_Debug.clipToWorld);
    return world.xyz / world.w;
}

bool IsBackground(float deviceDepth)
{
    return (g_Debug.depthConvention == 1) ? (deviceDepth <= 0.0000001f) : (deviceDepth >= 0.9999999f);
}

float4 main_ps(float4 position : SV_Position) : SV_Target0
{
    const float2 uv = position.xy * g_Debug.inverseSize;
    const float deviceDepth = g_SceneDepth.SampleLevel(g_PointClamp, uv, 0);

    if (IsBackground(deviceDepth))
        return float4(1.f, 0.f, 1.f, 1.f);   // 背景：品红（与目标清空色区分，便于诊断）

    if (g_Debug.mode == 0)
    {
        return float4(deviceDepth.xxx, 1.f);
    }

    if (g_Debug.mode == 1)
    {
        const float linearDepth = LinearizeDepth(deviceDepth) * g_Debug.depthScale;
        const float t = saturate(linearDepth / max(g_Debug.zFar, 1e-3f));
        return float4(t, t * 0.5f, 1.f - t, 1.f);
    }

    if (g_Debug.mode == 2)
    {
        const float3 worldPosition = ReconstructWorldPosition(uv, deviceDepth);
        const float3 scaled = worldPosition / max(g_Debug.zFar * 0.25f, 1e-3f);
        return float4(frac(abs(scaled)), 1.f);
    }

    // mode 3: normal reconstructed from depth derivatives, a quick check of the matrix convention
    const float2 texel = g_Debug.inverseSize;
    const float3 center = ReconstructWorldPosition(uv, deviceDepth);
    const float3 right = ReconstructWorldPosition(uv + float2(texel.x, 0.f), g_SceneDepth.SampleLevel(g_PointClamp, uv + float2(texel.x, 0.f), 0));
    const float3 down = ReconstructWorldPosition(uv + float2(0.f, texel.y), g_SceneDepth.SampleLevel(g_PointClamp, uv + float2(0.f, texel.y), 0));

    float3 normal = normalize(cross(down - center, right - center));
    if (!all(isfinite(normal)))
        normal = float3(0.f, 0.f, 1.f);

    // 世界空间法线编码在 [-1,1]：这是契约里 NormalSpace::World 的默认约定
    return float4(normal * 0.5f + 0.5f, 1.f);
}
