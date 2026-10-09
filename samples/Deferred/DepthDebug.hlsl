#include "Prism/Common/Platform.hlsli"
#include "DepthDebugCb.h"

ConstantBuffer<FDepthDebugConstants> g_Debug : register(b0);
Texture2D<float> g_Depth : register(t0);
SamplerState g_PointClamp : register(s0);

float4 main_ps(float4 position : SV_Position) : SV_Target0
{
    const float2 uv = position.xy * g_Debug.InverseSize;
    const float depth = g_Depth.SampleLevel(g_PointClamp, uv, 0);
    const bool reverseZ = g_Debug.DepthConvention == PRISM_DEPTH_CONVENTION_REVERSED_Z;
    if (reverseZ ? depth <= 0.0000001f : depth >= 0.9999999f)
    {
        return float4(0.12f, 0.04f, 0.18f, 1.f);
    }

    if (g_Debug.Mode == 0)
    {
        return float4(depth.xxx, 1.f);
    }

    const float linearDepth = reverseZ
        ? g_Debug.ZNear * g_Debug.ZFar / (g_Debug.ZNear + depth * (g_Debug.ZFar - g_Debug.ZNear))
        : g_Debug.ZNear * g_Debug.ZFar / (g_Debug.ZFar - depth * (g_Debug.ZFar - g_Debug.ZNear));
    const float value = saturate(linearDepth / g_Debug.ZFar);
    return float4(value.xxx, 1.f);
}
