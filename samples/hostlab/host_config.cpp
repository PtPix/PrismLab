#include "host_config.h"

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

namespace renderlab
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
            donut::log::warning("HostLab: configs/host/camera_default.json not found, using built-in defaults.");
            return config;
        }

        if (!std::filesystem::exists(configPath))
        {
            donut::log::warning("HostLab: config file does not exist: %s, using built-in defaults.", configPath.string().c_str());
            return config;
        }

        donut::vfs::NativeFileSystem fileSystem;
        Json::Value root;
        if (!donut::json::LoadFromFile(fileSystem, configPath, root))
        {
            donut::log::warning("HostLab: failed to parse config file: %s, using built-in defaults.", configPath.string().c_str());
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
        else
        {
            donut::log::warning("HostLab: config is missing the camera section, using built-in camera defaults.");
        }

        if (root.isMember("window"))
        {
            const Json::Value& window = root["window"];
            window["width"] >> config.window.width;
            window["height"] >> config.window.height;
            window["vsync"] >> config.window.vsync;
        }
        else
        {
            donut::log::warning("HostLab: config is missing the window section, using built-in window defaults.");
        }

        if (root.isMember("lighting"))
        {
            const Json::Value& lighting = root["lighting"];
            lighting["sunDirection"] >> config.lighting.sunDirection;
            lighting["sunIrradiance"] >> config.lighting.sunIrradiance;
            lighting["ambientIntensity"] >> config.lighting.ambientIntensity;
        }
        else
        {
            donut::log::warning("HostLab: config is missing the lighting section, using built-in lighting defaults.");
        }

        return config;
    }
}
