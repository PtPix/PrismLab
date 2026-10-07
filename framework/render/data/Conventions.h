#pragma once

// Framework types: the project-wide semantic conventions, version 1.
//
// These are Prism's own rules, not Donut's or UE's defaults. The Donut facilities convert host
// data into these conventions; if a host value is passed through unconverted, the interface
// metadata must say so and the verification code must check it.
//
//   World space   right-handed, Y up, meters
//   View space    left-handed (camera looks down +Z), D3D style projection, z in [0, 1]
//   Depth         forward-Z, near -> 0, far -> 1, cleared to 1, comparison Less
//                 (reversed-Z is allowed but must be requested explicitly and verified)
//   UV            origin top-left, range [0, 1]
//   Color         linear HDR RGB before display transform, RGBA16_FLOAT scene color, no implicit gamma
//   Roughness     perceptual roughness r; GGX alpha = r * r, squared exactly once inside the BRDF
//   Visibility    0 = fully occluded, 1 = fully visible, always tied to one light / sample
//   Motion vector previousUnjitteredUV - currentUnjitteredUV
//   Matrices      HLSL uses the row-vector form mul(v, M), exactly like Donut: a CPU dm::float4x4 is
//                 uploaded as-is and worldToClip = worldToView * viewToClip. Clip-space chains are
//                 therefore written clip = world * worldToView * viewToClip, and the inverse is
//                 clipToWorld = inverse(worldToClip). See ViewMatrices::Build.

#include "framework/core/Types.h"

#include <cmath>

namespace Prism
{
	// --- depth ---

	constexpr float KDepthClearValue = 1.0f;

	enum class EDepthConvention : uint32_t
	{
		ForwardZ0To1 = 0, // 默认：近 0 远 1，Less 通过
		ReversedZ0To1,	  // 需要显式登记，并让所有消费方按此解释
	};

	inline const char* ToString(EDepthConvention Convention)
	{
		switch (Convention)
		{
			case EDepthConvention::ForwardZ0To1:
				return "forward-Z [0,1]";
			case EDepthConvention::ReversedZ0To1:
				return "reversed-Z [0,1]";
			default:
				return "unknown depth convention";
		}
	}

	// Device depth -> linear distance in front of the camera (meters).
	// Only valid for the default forward-Z convention; callers must pass the convention they use.
	inline float LinearizeDepth(float DeviceDepth, float ZNearMeters, float ZFarMeters,
								EDepthConvention Convention = EDepthConvention::ForwardZ0To1)
	{
		if (Convention == EDepthConvention::ReversedZ0To1)
			DeviceDepth = 1.f - DeviceDepth;

		const float Denominator = ZFarMeters - DeviceDepth * (ZFarMeters - ZNearMeters);
		if (std::fabs(Denominator) < 1e-8f)
			return ZFarMeters;

		return ZNearMeters * ZFarMeters / Denominator;
	}

	// Linear distance in front of the camera -> device depth (inverse of LinearizeDepth).
	inline float DeviceDepthFromLinear(float LinearDepthMeters, float ZNearMeters, float ZFarMeters,
									   EDepthConvention Convention = EDepthConvention::ForwardZ0To1)
	{
		const float Distance = (std::fabs(LinearDepthMeters) < 1e-8f) ? ZNearMeters : LinearDepthMeters;
		const float DeviceDepth = ZFarMeters * (Distance - ZNearMeters) / (Distance * (ZFarMeters - ZNearMeters));

		if (Convention == EDepthConvention::ReversedZ0To1)
			return 1.f - DeviceDepth;

		return DeviceDepth;
	}

	// --- matrices ---

	// A CPU-side matrix uses the same logical layout as HLSL's mul(M, v): row-major, translation in
	// row 3. dm::float4x4 already stores rows in this order, so uploading the value directly to a
	// constant buffer is correct for both D3D and Vulkan in this codebase. The function exists so
	// that there is exactly one place to change if that assumption is ever revisited.
	inline dm::float4x4 ToShaderMatrix(const dm::float4x4& Matrix)
	{
		return Matrix;
	}

	inline dm::float4x4 ToShaderMatrix(const dm::affine3& Transform)
	{
		return dm::affineToHomogeneous(Transform);
	}

	// --- roughness / GGX ---

	inline float PerceptualRoughnessToAlpha(float PerceptualRoughness)
	{
		const float Clamped = dm::clamp(PerceptualRoughness, 0.f, 1.f);
		return Clamped * Clamped;
	}

	inline float AlphaToPerceptualRoughness(float Alpha)
	{
		const float Clamped = dm::clamp(Alpha, 0.f, 1.f);
		return std::sqrt(Clamped);
	}
} // namespace Prism
