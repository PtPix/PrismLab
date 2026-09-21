#pragma once

// Donut facilities: the shared experiment configuration (window, camera, lighting, scene, render).
//
// A lab does not read the JSON itself: it asks for its own section ("labs" -> "<lab name>") and keeps
// its parameters in the framework types. The default file is the experiment's own config.json, which
// rl_add_target (CONFIG) copies next to the executable as <executable name>.json; --config overrides it.

#include <donut/core/math/math.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace Json
{
    class Value;
}

namespace renderlab::adapter
{
    struct HostCameraPreset
    {
        dm::float3 position = dm::float3(0.f, 1.6f, -6.f);
        dm::float3 target = dm::float3(0.f, 0.8f, 0.f);
        float fovDegrees = 60.f;
        float zNear = 0.05f;
        float zFar = 100.f;
        float moveSpeed = 2.f;
    };

    struct HostWindowPreset
    {
        uint32_t width = 1280;
        uint32_t height = 720;
        bool vsync = true;
    };

    struct HostLightingPreset
    {
        dm::float3 sunDirection = dm::float3(0.45f, -1.f, 0.35f);
        float sunIrradiance = 2.2f;
        float ambientIntensity = 0.18f;
    };

    struct HostScenePreset
    {
        // "procedural" —— 无需资产的程序化测试场景（算法研究的默认选择）
        // "gltf"       —— 由 Donut 加载的 glTF 场景，路径见 asset
        std::string source = "procedural";
        std::string asset;
    };

    struct HostRenderPreset
    {
        // 场景渲染分辨率相对窗口的比例；1 表示与窗口一致（时域上采样会用到小于 1 的值）
        float renderScale = 1.f;

        bool enableGpuTiming = true;
    };

    struct HostConfig
    {
        HostCameraPreset camera;
        HostWindowPreset window;
        HostLightingPreset lighting;
        HostScenePreset scene;
        HostRenderPreset render;

        std::filesystem::path sourcePath;
        bool loadedFromFile = false;
    };

    // explicitPath wins; otherwise <executable name>.json next to the executable is used; if neither
    // exists the built-in defaults are kept.
    HostConfig LoadHostConfig(
        const std::filesystem::path& explicitPath = std::filesystem::path(),
        const std::filesystem::path& executablePath = std::filesystem::path());

    // 解析资产路径：绝对路径原样使用；相对路径先按当前工作目录，再从可执行文件目录向上查找。
    // 找不到时返回原路径，由加载方给出错误信息。
    std::filesystem::path ResolveAssetPath(const std::string& path);

    // 读取实验自己的参数段：configs 中 "labs": { "<labName>": { ... } }。
    // 返回 false 表示没有该段，实验应继续使用内置默认值。
    bool LoadLabSettings(const HostConfig& config, const char* labName, Json::Value& outSettings);
}
