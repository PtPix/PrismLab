#pragma once

// Framework types: surface and GBuffer semantics.
//
// Same names, different meanings: a "normal" target can be world space, view space, octahedral or
// reconstructed. Every GBuffer-like interface in RenderLab states the meaning explicitly, and the
// shader headers under algorithms/shaders use the same names.

#include "PixelFormat.h"
#include "Types.h"

namespace renderlab
{
    enum class NormalSpace : uint32_t
    {
        World = 0,      // 默认：世界空间单位法线，带符号，编码在 [-1, 1]
        View,
        Count
    };

    enum class RoughnessEncoding : uint32_t
    {
        // 默认：感知粗糙度 r，GGX alpha = r * r，平方只做一次
        PerceptualLinear = 0,
        Alpha,          // 直接存 alpha
        Count
    };

    struct GBufferSchema
    {
        NormalSpace normalSpace = NormalSpace::World;
        RoughnessEncoding roughnessEncoding = RoughnessEncoding::PerceptualLinear;

        PixelFormat normalsRoughness = PixelFormat::RGBA16_FLOAT; // rgb = 法线, a = 粗糙度
        PixelFormat baseColorMetalness = PixelFormat::RGBA8_UNORM; // rgb = 基色(线性), a = 金属度
        PixelFormat motionVector = PixelFormat::RG16_FLOAT;        // previousUV - currentUV
        PixelFormat depth = PixelFormat::D32_FLOAT;

        // 屏幕分辨率下与世界空间位置的往返误差上限（米），用于契约自检。
        float worldPositionToleranceMeters = 1e-3f;
    };

    // CPU 侧的表面采样，供参考实现、数值测试和调试视图使用。
    struct SurfaceGeometry
    {
        dm::float3 worldPosition = dm::float3(0.f);
        dm::float3 worldNormal = dm::float3(0.f, 1.f, 0.f);
        dm::float2 unjitteredUv = dm::float2(0.f);
        float deviceDepth = 1.f;
        float linearDepthMeters = 0.f;
        bool isBackground = true;
    };

    struct SurfaceShading
    {
        dm::float3 baseColor = dm::float3(1.f);
        float perceptualRoughness = 0.5f;
        float metalness = 0.f;
        dm::float3 emissive = dm::float3(0.f);
    };
}
