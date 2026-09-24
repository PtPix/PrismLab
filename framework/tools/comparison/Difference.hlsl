#include <Prism/Common/Platform.hlsli>
cbuffer Constants : register(b0) { float gain; float3 padding; }
Texture2D<float4> imageA : register(t0);
Texture2D<float4> imageB : register(t1);
float4 main_ps(float4 position : SV_Position) : SV_Target
{
    int3 texel = int3(int2(position.xy), 0);
    return float4(abs(imageA.Load(texel).rgb - imageB.Load(texel).rgb) * gain, 1);
}
