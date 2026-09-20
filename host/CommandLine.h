#pragma once

// Host layer: command line parsing shared by all labs.
//
//   --config <path>      使用指定配置文件
//   --scene <source>     覆盖配置中的场景来源（procedural | gltf）
//   --asset <path>       覆盖场景资产路径
//   --smoke-test[=N]     渲染 N 帧（默认 3）后退出，用于验证构建与初始化
//   --width/--height <n> 覆盖窗口尺寸
//   --capture <path>     截图并退出（默认在 --capture-frame 指定的帧）
//   --capture-frame <n>  截图帧序号，默认 2
//   --no-vsync           关闭垂直同步
//   --no-timing          关闭 GPU 计时
//   --help               打印用法

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace renderlab::host
{
    struct CommandLine
    {
        std::filesystem::path configPath;
        std::string sceneSource;
        std::string sceneAsset;

        uint32_t smokeTestFrames = 0;
        bool smokeTest = false;

        std::filesystem::path capturePath;
        uint32_t captureFrame = 2;

        uint32_t width = 0;
        uint32_t height = 0;

        bool disableVsync = false;
        bool disableGpuTiming = false;
        bool showHelp = false;

        // 未识别的参数：留给实验自己解析
        std::vector<std::string> extraArguments;
    };

    CommandLine ParseCommandLine(int argc, char** argv);
    std::string GetCommandLineUsage();
}
