#pragma once

// Host layer: command line parsing shared by all experiments.
//
//   --config <path>      使用指定配置文件
//   --scene <source>     覆盖配置中的场景来源（procedural | gltf）
//   --asset <path>       覆盖场景资产路径
//   --smoke-test[=N]     渲染 N 帧（默认 3）后退出，用于验证构建与初始化
//   --width/--height <n> 覆盖窗口尺寸
//   --capture <path>     截图并退出（默认在 --capture-frame 指定的帧）
//   --capture-frame <n>  截图帧序号，默认 2
//   --write-reference <path>  把该帧输出写成浮点参考图（.f32）后退出
//   --reference <path>   与该帧输出和浮点参考图比较，差异超限时进程返回非零
//   --tolerance <v>      参考图比较容差，默认 0.01
//   --bench[=N]          预热后测量 N 帧（默认 120），写出指标 CSV 并退出
//   --bench-warmup[=N]   --bench 的预热帧数，默认 30
//   --metrics <path>     指标 CSV 路径（配合 --bench / --smoke-test，退出时写出）
//   --no-vsync           关闭垂直同步
//   --no-timing          关闭 GPU 计时
//   --help               打印用法

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace prism::host
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

        // 浮点参考图：写出来给后续比较，或与已有参考比较（失败时进程返回非零）
        std::filesystem::path referencePath;
        std::filesystem::path writeReferencePath;
        float tolerance = 0.01f;

        // 性能测量：预热后测量固定帧数，写出指标 CSV 后退出
        uint32_t benchFrames = 0;
        uint32_t benchWarmup = 30;
        std::filesystem::path metricsPath;

        // 公共调试视图：0 = 显示实验输出，n = 显示第 n 个登记的中间结果
        // （配合 --capture 可以给中间结果截图，不需要手点面板）
        int debugView = 0;

        uint32_t width = 0;
        uint32_t height = 0;

        bool disableVsync = false;
        bool disableGpuTiming = false;
        bool showHelp = false;

        // 捕获/参考比较/性能测量的帧数上限（0 表示不做）
        [[nodiscard]] bool WantsHeadlessRun() const
        {
            return smokeTest || benchFrames > 0 || !capturePath.empty() ||
                !referencePath.empty() || !writeReferencePath.empty();
        }

        // 未识别的参数：留给实验自己解析
        std::vector<std::string> extraArguments;
    };

    CommandLine ParseCommandLine(int argc, char** argv);
    std::string GetCommandLineUsage();
}
