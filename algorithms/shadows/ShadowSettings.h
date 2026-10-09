#pragma once

// Shadow-family settings and light projection conventions.

#include <array>
#include <cmath>
#include <cstdint>

namespace Prism
{
	enum class EShadowFilter : uint32_t
	{
		Hard = 0,
		PCF,
		PCSS,
		Count
	};

	enum class ELightProjection : uint32_t
	{
		Perspective = 0, // 聚光/点光
		Orthographic,	 // 方向光
		Count
	};

	inline const char* ToString(EShadowFilter Filter)
	{
		switch (Filter)
		{
			case EShadowFilter::Hard:
				return "hard";
			case EShadowFilter::PCF:
				return "PCF";
			case EShadowFilter::PCSS:
				return "PCSS";
			default:
				return "unknown";
		}
	}

	struct FShadowSettings
	{
		EShadowFilter Filter = EShadowFilter::PCF;

		uint32_t BlockerSamples = 16; // PCSS 遮挡物搜索
		uint32_t FilterSamples = 32;  // PCF/PCSS 过滤

		// 比较深度的偏移，不是米：光栅阶段的 depth/slope bias 与这里的接收阶段偏移分开配置
		float ReceiverBiasNdc = 0.0001f;

		// 接收点的世界空间法线偏移（米）
		float NormalBiasMeters = 0.f;

		float PcfRadiusTexels = 2.f;

		// PCSS 半影估计：光源尺寸与遮挡物距离的关系
		float PenumbraMinMeters = 0.01f;
		float PenumbraMaxMeters = 0.5f;

		// 首版约定：阴影图之外的可见性为 1（限定投影范围的策略）
		float OutsideShadowMapVisibility = 1.f;
	};

	struct FShadowView
	{
		// Row-major 4x4 matrices; the caller converts its math types at the boundary.
		std::array<float, 16> WorldToLightClip = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
												  0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
		std::array<float, 16> LightClipToWorld = WorldToLightClip;

		ELightProjection Projection = ELightProjection::Orthographic;
		bool bReverseZ = false;

		float NearPlaneMeters = 0.1f;
		float FarPlaneMeters = 100.f;

		// 仅透视聚光灯的 PCSS 路径使用
		float SpotEmitterRadiusMeters = 0.f;

		// 仅方向光路径使用：不能把方向光的角尺寸当成聚光灯的米制半径
		float DirectionalAngularRadiusRadians = 0.f;

		uint64_t LightId = 0;

		[[nodiscard]] float LinearizeDepth(float DeviceDepth) const
		{
			const float Range = FarPlaneMeters - NearPlaneMeters;
			const float Denominator =
				bReverseZ ? NearPlaneMeters + DeviceDepth * Range : FarPlaneMeters - DeviceDepth * Range;
			return std::fabs(Denominator) < 1e-8f ? FarPlaneMeters : NearPlaneMeters * FarPlaneMeters / Denominator;
		}
	};

	struct FShadowDebugView
	{
		// 调试视图明确标出投影覆盖范围，避免把"图外可见"误解为"世界无遮挡"
		bool bShowShadowMap = false;
		bool bShowPcssBlockerDistance = false;
		bool bShowPenumbraRadius = false;
	};
} // namespace Prism
