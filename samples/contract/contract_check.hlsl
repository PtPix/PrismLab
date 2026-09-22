// Contract check pass (M1): decode the depth buffer on the GPU and write values the CPU can verify.
//
// The CPU then checks, for a sparse set of pixels:
//   * projecting the reconstructed world position returns the same pixel (matrix convention)
//   * the linear depth matches the CPU's LinearizeDepth of the same device depth (depth convention)
//   * the CPU's own reconstruction of the world position matches the GPU's (both sides agree)
//
// Nothing here depends on the scene: the outputs are compared against the CPU's own math.

// 平台约定（矩阵行主序、深度约定、颜色空间）：必须最先包含
#include "Prism/Common/Platform.hlsli"

#include "contract_check_cb.h"

ConstantBuffer<ContractCheckConstants> g_Check : register(b0);

Texture2D<float> g_SceneDepth : register(t0);

RWTexture2D<float4> g_OutputPosition : register(u0);   // rgb = 世界位置(米), a = 线性深度(米)
RWTexture2D<float> g_OutputDepth : register(u1);       // 设备深度原值

float LinearizeDepth(float deviceDepth)
{
    if (g_Check.depthConvention == 1)
        deviceDepth = 1.f - deviceDepth;

    const float zNear = g_Check.zNear;
    const float zFar = g_Check.zFar;
    return zNear * zFar / (zFar - deviceDepth * (zFar - zNear));
}

[numthreads(8, 8, 1)]
void main_cs(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 pixel = dispatchThreadId.xy;
    if (any(pixel >= g_Check.size))
        return;

    const float2 uv = (float2(pixel) + 0.5f) * g_Check.inverseSize;
    const float deviceDepth = g_SceneDepth.Load(int3(pixel, 0));

    // 行向量约定：world = clip * clipToWorld
    const float4 clip = float4(uv.x * 2.f - 1.f, (1.f - uv.y) * 2.f - 1.f, deviceDepth, 1.f);
    const float4 world = mul(clip, g_Check.clipToWorld);

    g_OutputPosition[pixel] = float4(world.xyz / world.w, LinearizeDepth(deviceDepth));
    g_OutputDepth[pixel] = deviceDepth;
}
