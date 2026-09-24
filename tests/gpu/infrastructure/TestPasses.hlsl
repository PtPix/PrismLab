RWTexture2D<float4> result : register(u0);
#ifdef TEST_WIDE
[numthreads(8, 4, 1)]
#else
[numthreads(4, 4, 1)]
#endif
void main_cs(uint3 id : SV_DispatchThreadID)
{
    uint w, h; result.GetDimensions(w, h);
    if (id.x < w && id.y < h)
        result[id.xy] = float4(float(id.x) / w, float(id.y) / h, 0.5, 1);
}
float4 main_vs(uint id : SV_VertexID) : SV_Position
{
    float2 p = id == 0 ? float2(-1, -1) : id == 1 ? float2(-1, 3) : float2(3, -1);
    return float4(p, 0, 1);
}
float4 main_ps() : SV_Target0 { return float4(0.25, 0.5, 0.75, 1); }
