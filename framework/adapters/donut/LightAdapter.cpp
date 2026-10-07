#include "LightAdapter.h"

#include <donut/core/log.h>

#include <cmath>

namespace Prism::Adapter
{
	std::vector<Prism::FLightRecord> CollectLights(const donut::engine::SceneGraph& Graph)
	{
		std::vector<Prism::FLightRecord> Records;
		const auto& Lights = Graph.GetLights();
		Records.reserve(Lights.size());

		for (const std::shared_ptr<donut::engine::Light>& Light : Lights)
		{
			if (!Light)
				continue;

			Prism::FLightRecord Record;
			Record.Name = Light->GetName();
			Record.StableId = uint64_t(Records.size()); // 场景内稳定：同一场景内顺序不变
			Record.Color = Light->color;
			Record.Position = dm::float3(Light->GetPosition());
			Record.Direction = dm::normalize(dm::float3(Light->GetDirection()));

			if (const auto* Directional = dynamic_cast<const donut::engine::DirectionalLight*>(Light.get()))
			{
				Record.Type = Prism::ELightType::Directional;
				Record.Intensity = Directional->irradiance;
				Record.AngularRadiusRadians = dm::radians(Directional->angularSize);
				Record.Radius = 0.f;
			}
			else if (const auto* Spot = dynamic_cast<const donut::engine::SpotLight*>(Light.get()))
			{
				Record.Type = Prism::ELightType::Spot;
				Record.Intensity = Spot->intensity;
				Record.Radius = Spot->radius;
				Record.ConeAngleOuterRadians = dm::radians(Spot->outerAngle) * 0.5f; // Donut 存的是全锥角
				Record.ConeAngleInnerRadians = dm::radians(Spot->innerAngle) * 0.5f;
			}
			else if (const auto* Point = dynamic_cast<const donut::engine::PointLight*>(Light.get()))
			{
				Record.Type = Prism::ELightType::Point;
				Record.Intensity = Point->intensity;
				Record.Radius = Point->radius;
			}
			else
			{
				donut::log::warning("Prism: unsupported light type for '%s', skipping.", Record.Name.c_str());
				continue;
			}

			// 有阴影图的光源才声明投影能力；首版由实验代码决定是否真的生成阴影。
			Record.bCastsShadow = Light->shadowMap != nullptr;

			Records.push_back(std::move(Record));
		}

		return Records;
	}

	Prism::FGpuLight ToGpuLight(const Prism::FLightRecord& Light)
	{
		Prism::FGpuLight Gpu = {};

		Gpu.PositionRadius = dm::float4(Light.Position, Light.Radius);
		Gpu.DirectionAngularSize = dm::float4(Light.Direction, Light.AngularRadiusRadians);
		Gpu.ColorIntensity = dm::float4(Light.Color, Light.Intensity);
		Gpu.ConeCosines =
			dm::float4(std::cos(Light.ConeAngleOuterRadians), std::cos(Light.ConeAngleInnerRadians), 0.f, 0.f);

		Gpu.Type = uint32_t(Light.Type);
		Gpu.StableId = uint32_t(Light.StableId);
		Gpu.bCastsShadow = Light.bCastsShadow ? 1u : 0u;

		return Gpu;
	}
} // namespace Prism::Adapter
