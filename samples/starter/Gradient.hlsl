#include "Prism/Common/Platform.hlsli"
cbuffer Constants : register(b0) { float2 size; float time; float padding; }
float4 main_ps(float4 position : SV_Position) : SV_Target0
{
    float2 uv = position.xy / size;
    float2 p = (uv - .5) * float2(size.x / size.y, 1);
    float wave = .5 + .5 * sin(p.x * 5 + p.y * 3 - time * .35);
    float3 color = lerp(float3(.04, .10, .20), float3(.13, .62, .69), wave);
    float glow = exp(-8 * dot(p - float2(.3, -.1), p - float2(.3, -.1)));
    color += glow * float3(.45, .23, .12);
    color *= 1 - .3 * dot(uv - .5, uv - .5);
    return float4(color, 1);
}
