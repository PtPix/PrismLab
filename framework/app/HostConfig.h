#pragma once

// Donut facilities: the shared experiment configuration (window, camera, lighting, scene, render).
//
// An experiment does not read the JSON itself: it asks for its own section ("experiments" -> "<name>") and keeps
// its parameters in the framework types. The default file is the experiment's own config.json, which
// prism_add_target (CONFIG) copies next to the executable as <executable name>.json; --config overrides it.

#include <framework/scene/ScenePreset.h>
#include <framework/render/data/CameraPreset.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace Json
{
    class Value;
}

namespace prism::host
{
    struct WindowPreset
    {
        uint32_t width = 1280;
        uint32_t height = 720;
        bool vsync = true;
    };
    struct RenderPreset
    {
        // 场景渲染分辨率相对窗口的比例；1 表示与窗口一致（时域上采样会用到小于 1 的值）
        float renderScale = 1.f;

        bool enableGpuTiming = true;
    };

    struct HostConfig
    {
        CameraPreset camera;
        WindowPreset window;
        LightingPreset lighting;
        ScenePreset scene;
        RenderPreset render;

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

    // 读取实验自己的参数段：configs 中 "experiments": { "<experimentName>": { ... } }。
    // 返回 false 表示没有该段，实验应继续使用内置默认值。
    bool LoadExperimentSettings(const HostConfig& config, const char* experimentName, Json::Value& outSettings);
}
