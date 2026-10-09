#pragma pack_matrix(row_major)
#include "SurfaceDepthCb.h"

ConstantBuffer<FSurfaceDepthConstants> g_Depth : register(b0);

float4 main_vs(float3 position : POSITION) : SV_Position
{
    return mul(float4(position, 1.f), g_Depth.ObjectToClip);
}
