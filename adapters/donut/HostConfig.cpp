#include "HostConfig.h"

#include <donut/app/ApplicationBase.h>
#include <donut/core/json.h>
#include <donut/core/log.h>
#include <donut/core/vfs/VFS.h>

namespace
{
    // Search for the config file upwards from the executable directory:
    // binaries land in build/<preset>/bin while the config lives at the repository root.
    std::filesystem::path FindFileUpwards(
        const std::filesystem::path& startDirectory,
        const std::filesystem::path& relativeFilePath,
        int maxDepth)
    {
        std::filesystem::path directory = startDirectory;

        for (int depth = 0; depth <= maxDepth && !directory.empty(); ++depth)
        {
            const std::filesystem::path candidate = directory / relativeFilePath;
            if (std::filesystem::exists(candidate))
                return candidate;

            const std::filesystem::path parent = directory.parent_path();
            if (parent == directory)
                break;

            directory = parent;
        }

        return std::filesystem::path();
    }
}

namespace renderlab::adapter
{
    HostConfig LoadHostConfig(const std::filesystem::path& explicitPath)
    {
        HostConfig config;

        std::filesystem::path configPath = explicitPath;
        if (configPath.empty())
        {
            configPath = FindFileUpwards(
                donut::app::GetDirectoryWithExecutable(),
                std::filesystem::path("configs") / "host" / "camera_default.json",
                6);
        }

        if (configPath.empty())
        {
            donut::log::warning("RenderLab: configs/host/camera_default.json not found, using built-in defaults.");
            return config;
        }

        if (!std::filesystem::exists(configPath))
        {
            donut::log::warning("RenderLab: config file does not exist: %s, using built-in defaults.", configPath.string().c_str());
            return config;
        }

        donut::vfs::NativeFileSystem fileSystem;
        Json::Value root;
        if (!donut::json::LoadFromFile(fileSystem, configPath, root))
        {
            donut::log::warning("RenderLab: failed to parse config file: %s, using built-in defaults.", configPath.string().c_str());
            return config;
        }

        config.sourcePath = configPath;
        config.loadedFromFile = true;

        if (root.isMember("camera"))
        {
            const Json::Value& camera = root["camera"];
            camera["position"] >> config.camera.position;
            camera["target"] >> config.camera.target;
            camera["fovDegrees"] >> config.camera.fovDegrees;
            camera["zNear"] >> config.camera.zNear;
            camera["zFar"] >> config.camera.zFar;
            camera["moveSpeed"] >> config.camera.moveSpeed;
        }

        if (root.isMember("window"))
        {
            const Json::Value& window = root["window"];
            window["width"] >> config.window.width;
            window["height"] >> config.window.height;
            window["vsync"] >> config.window.vsync;
        }

        if (root.isMember("lighting"))
        {
            const Json::Value& lighting = root["lighting"];
            lighting["sunDirection"] >> config.lighting.sunDirection;
            lighting["sunIrradiance"] >> config.lighting.sunIrradiance;
            lighting["ambientIntensity"] >> config.lighting.ambientIntensity;
        }

        if (root.isMember("scene"))
        {
            const Json::Value& scene = root["scene"];
            scene["source"] >> config.scene.source;
            scene["asset"] >> config.scene.asset;
        }

        if (root.isMember("render"))
        {
            const Json::Value& render = root["render"];
            render["renderScale"] >> config.render.renderScale;
            render["enableGpuTiming"] >> config.render.enableGpuTiming;
        }

        return config;
    }

    std::filesystem::path ResolveAssetPath(const std::string& path)
    {
        if (path.empty())
            return std::filesystem::path();

        const std::filesystem::path candidate(path);
        if (candidate.is_absolute())
            return candidate;

        std::error_code error;
        if (std::filesystem::exists(candidate, error))
            return std::filesystem::absolute(candidate, error);

        // 运行目录通常是 build/<preset>/bin，而资产在仓库里：向上查找。
        const std::filesystem::path found = FindFileUpwards(
            donut::app::GetDirectoryWithExecutable(),
            candidate,
            6);

        if (!found.empty())
            return found;

        return candidate;
    }

    bool LoadLabSettings(const HostConfig& config, const char* labName, Json::Value& outSettings)
    {
        if (!config.loadedFromFile || !labName)
            return false;

        donut::vfs::NativeFileSystem fileSystem;
        Json::Value root;
        if (!donut::json::LoadFromFile(fileSystem, config.sourcePath, root))
            return false;

        if (!root.isMember("labs"))
            return false;

        const Json::Value& labs = root["labs"];
        if (!labs.isMember(labName))
            return false;

        outSettings = labs[labName];
        return true;
    }
}
