#pragma once

// Framework types: surface and GBuffer semantics.
//
// Same names, different meanings: a "normal" target can be world space, view space, octahedral or
// reconstructed. Every GBuffer-like interface in Prism states the meaning explicitly, and the
// algorithm shader headers use the same names.

#include "framework/render/data/PixelFormat.h"
#include "framework/core/Types.h"

namespace Prism
{
	enum class ENormalSpace : uint32_t
	{
		World = 0, // 默认：世界空间单位法线，带符号，编码在 [-1, 1]
		View,
		Count
	};

	enum class ERoughnessEncoding : uint32_t
	{
		// 默认：感知粗糙度 r，GGX alpha = r * r，平方只做一次
		PerceptualLinear = 0,
		Alpha, // 直接存 alpha
		Count
	};

	struct FGBufferSchema
	{
		ENormalSpace NormalSpace = ENormalSpace::World;
		ERoughnessEncoding RoughnessEncoding = ERoughnessEncoding::PerceptualLinear;

		EPixelFormat NormalsRoughness = EPixelFormat::RgbA16Float;	// rgb = 法线, a = 粗糙度
		EPixelFormat BaseColorMetalness = EPixelFormat::RgbA8Unorm; // rgb = 基色(线性), a = 金属度
		EPixelFormat MotionVector = EPixelFormat::RG16Float;		// previousUV - currentUV
		EPixelFormat Depth = EPixelFormat::D32Float;

		// 屏幕分辨率下与世界空间位置的往返误差上限（米），用于契约自检。
		float WorldPositionToleranceMeters = 1e-3f;
	};

	// CPU 侧的表面采样，供参考实现、数值测试和调试视图使用。
	struct FSurfaceGeometry
	{
		dm::float3 WorldPosition = dm::float3(0.f);
		dm::float3 WorldNormal = dm::float3(0.f, 1.f, 0.f);
		dm::float2 UnjitteredUv = dm::float2(0.f);
		float DeviceDepth = 1.f;
		float LinearDepthMeters = 0.f;
		bool bIsBackground = true;
	};

	struct FSurfaceShading
	{
		dm::float3 BaseColor = dm::float3(1.f);
		float PerceptualRoughness = 0.5f;
		float Metalness = 0.f;
		dm::float3 Emissive = dm::float3(0.f);
	};
} // namespace Prism
