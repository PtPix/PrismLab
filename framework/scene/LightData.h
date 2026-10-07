#pragma once

// Framework types: light records and their GPU layout.
//
// The intensity field must not silently mean candela, illuminance, radiance and an artistmatic
// multiplier at the same time. Each type documents its own photometric meaning below.

#include "framework/core/Types.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace Prism
{
	enum class ELightType : uint32_t
	{
		Directional = 0, // directional light (sun)
		Point = 1,		 // punctual point light
		Spot = 2,		 // punctual spot light
		Count
	};

	inline const char* ToString(ELightType Type)
	{
		switch (Type)
		{
			case ELightType::Directional:
				return "directional";
			case ELightType::Point:
				return "point";
			case ELightType::Spot:
				return "spot";
			default:
				return "unknown";
		}
	}

	struct FLightRecord
	{
		ELightType Type = ELightType::Directional;
		std::string Name;

		// 光源的稳定标识：可见性纹理、历史缓冲和调试视图用它归属某个光源。
		uint64_t StableId = 0;

		// 世界空间，单位米
		dm::float3 Position = dm::float3(0.f);

		// 单位向量，指向被照亮的表面
		dm::float3 Direction = dm::float3(0.f, -1.f, 0.f);

		// 线性颜色（可大于 1）
		dm::float3 Color = dm::float3(1.f);

		// 方向光：垂直于光线方向的照度；点光/聚光：坎德拉
		float Intensity = 1.f;

		// 方向光：角半径（弧度），软阴影半影来源
		float AngularRadiusRadians = 0.f;

		// 点光/聚光：衰减半径，单位米
		float Radius = 1.f;

		// 聚光：外角与内角（弧度），内角 = 0 表示硬边锥
		float ConeAngleOuterRadians = dm::radians(45.f);
		float ConeAngleInnerRadians = 0.f;

		bool bCastsShadow = false;
	};

	// 上传给 GPU 的固定布局。不要按 LightRecord 的内存布局上传：它含 std::string。
	struct FGpuLight
	{
		dm::float4 PositionRadius;		 // xyz = 世界位置(米)；w = 衰减半径(米)，方向光为 0
		dm::float4 DirectionAngularSize; // xyz = 单位方向；w = 方向光角半径(弧度)，其他类型为 0
		dm::float4 ColorIntensity;		 // rgb = 线性颜色；a = 强度（单位随类型而定）
		dm::float4 ConeCosines;			 // x = cos(外角/2)，y = cos(内角/2)，zw = 保留
		uint32_t Type = 0;				 // LightType
		uint32_t StableId = 0;
		uint32_t bCastsShadow = 0;
		uint32_t Reserved = 0;
	};

	static_assert(sizeof(FGpuLight) == 80, "GpuLight layout changed; update the matching HLSL struct");
	static_assert(offsetof(FGpuLight, DirectionAngularSize) == 16, "GpuLight alignment changed");
	static_assert(offsetof(FGpuLight, ConeCosines) == 48, "GpuLight alignment changed");
	static_assert(offsetof(FGpuLight, Type) == 64, "GpuLight alignment changed");

	// 光源强度到统一单位的概念性说明，供文档与 UI 使用。
	inline const char* GetIntensityUnitDescription(ELightType Type)
	{
		switch (Type)
		{
			case ELightType::Directional:
				return "illuminance perpendicular to the light (linear color scale)";
			case ELightType::Point:
				return "candela";
			case ELightType::Spot:
				return "candela";
			default:
				return "unknown";
		}
	}
} // namespace Prism
