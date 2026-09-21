#include "LightAdapter.h"

#include <donut/core/log.h>

#include <cmath>

namespace renderlab::adapter
{
    std::vector<renderlab::LightRecord> CollectLights(const donut::engine::SceneGraph& graph)
    {
        std::vector<renderlab::LightRecord> records;
        const auto& lights = graph.GetLights();
        records.reserve(lights.size());

        for (const std::shared_ptr<donut::engine::Light>& light : lights)
        {
            if (!light)
                continue;

            renderlab::LightRecord record;
            record.name = light->GetName();
            record.stableId = uint64_t(records.size());   // 场景内稳定：同一场景内顺序不变
            record.color = light->color;
            record.position = dm::float3(light->GetPosition());
            record.direction = dm::normalize(dm::float3(light->GetDirection()));

            if (const auto* directional = dynamic_cast<const donut::engine::DirectionalLight*>(light.get()))
            {
                record.type = renderlab::LightType::Directional;
                record.intensity = directional->irradiance;
                record.angularRadiusRadians = dm::radians(directional->angularSize);
                record.radius = 0.f;
            }
            else if (const auto* spot = dynamic_cast<const donut::engine::SpotLight*>(light.get()))
            {
                record.type = renderlab::LightType::Spot;
                record.intensity = spot->intensity;
                record.radius = spot->radius;
                record.coneAngleOuterRadians = dm::radians(spot->outerAngle) * 0.5f;   // Donut 存的是全锥角
                record.coneAngleInnerRadians = dm::radians(spot->innerAngle) * 0.5f;
            }
            else if (const auto* point = dynamic_cast<const donut::engine::PointLight*>(light.get()))
            {
                record.type = renderlab::LightType::Point;
                record.intensity = point->intensity;
                record.radius = point->radius;
            }
            else
            {
                donut::log::warning("RenderLab: unsupported light type for '%s', skipping.", record.name.c_str());
                continue;
            }

            // 有阴影图的光源才声明投影能力；首版由实验代码决定是否真的生成阴影。
            record.castsShadow = light->shadowMap != nullptr;

            records.push_back(std::move(record));
        }

        return records;
    }

    renderlab::GpuLight ToGpuLight(const renderlab::LightRecord& light)
    {
        renderlab::GpuLight gpu = {};

        gpu.positionRadius = dm::float4(light.position, light.radius);
        gpu.directionAngularSize = dm::float4(light.direction, light.angularRadiusRadians);
        gpu.colorIntensity = dm::float4(light.color, light.intensity);
        gpu.coneCosines = dm::float4(
            std::cos(light.coneAngleOuterRadians),
            std::cos(light.coneAngleInnerRadians),
            0.f,
            0.f);

        gpu.type = uint32_t(light.type);
        gpu.stableId = uint32_t(light.stableId);
        gpu.castsShadow = light.castsShadow ? 1u : 0u;

        return gpu;
    }
}
