// 公共调试视图：把任意中间纹理画到目标，供 UI 下拉选择使用。
//
// 自带顶点着色器（不依赖 Donut 的全屏 VS）：输出 4 个顶点，配合 TriangleStrip。
// 输出已经是显示空间的数据（不是线性辐射），显示链应按 ColorSpace::DisplayEncoded 处理。

#include "RenderLab/Common/Platform.hlsli"
#include "DebugView_cb.h"

ConstantBuffer<DebugViewConstants> g_Debug : register(b0);

Texture2D<float4> g_Source : register(t0);
SamplerState g_PointClamp : register(s0);

void main_vs(in uint vertexId : SV_VertexID,
             out float4 position : SV_Position,
             out float2 uv : UV)
{
    const uint u = vertexId & 1;
    const uint v = (vertexId >> 1) & 1;

    position = float4(float(u) * 2.f - 1.f, 1.f - float(v) * 2.f, 0.f, 1.f);
    uv = float2(u, v);
}

// 简单热度色：黑 -> 红 -> 白
float3 FalseColor(float t)
{
    t = saturate(t);
    return saturate(float3(t * 2.f, t * 2.f - 1.f, t * 2.f - 1.5f));
}

float4 main_ps(float4 position : SV_Position) : SV_Target0
{
    const float2 uv = position.xy * g_Debug.inverseSize;
    const float4 value = g_Source.SampleLevel(g_PointClamp, uv, 0);

    float3 result = value.rgb;

    switch (g_Debug.mode)
    {
    case 1: result = value.rrr; break;
    case 2: result = value.ggg; break;
    case 3: result = value.bbb; break;
    case 4: result = value.aaa; break;
    case 5: result = dot(value.rgb, float3(0.2126f, 0.7152f, 0.0722f)).xxx; break;
    case 6: result = FalseColor(dot(value.rgb, float3(0.2126f, 0.7152f, 0.0722f))); break;
    case 7: result = (1.f - value.r).xxx; break;
    default: break;
    }

    result = saturate(result * g_Debug.scale + g_Debug.bias);
    return float4(result, 1.f);
}
