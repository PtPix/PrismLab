#pragma once

// Host layer: command line parsing shared by all experiments.
//
//   --config <path>      使用指定配置文件
//   --scene <source>     覆盖配置中的场景来源（procedural | gltf）
//   --asset <path>       覆盖场景资产路径
//   --width/--height <n> 覆盖窗口尺寸
//   --capture <path>     截图并退出（默认在 --capture-frame 指定的帧）
//   --capture-frame <n>  截图帧序号，默认 2
//   --write-reference <path>  把该帧输出写成浮点参考图（.f32）后退出
//   --reference <path>   与该帧输出和浮点参考图比较，差异超限时进程返回非零
//   --tolerance <v>      参考图比较容差，默认 0.01
//   --bench[=N]          预热后测量 N 帧（默认 120），写出指标 CSV 并退出
//   --bench-warmup[=N]   --bench 的预热帧数，默认 30
//   --metrics <path>     指标 CSV 路径（配合 --bench，退出时写出）
//   --no-vsync           关闭垂直同步
//   --no-timing          关闭 GPU 计时
//   --help               打印用法

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Prism::Host
{
	struct FCommandLine
	{
		std::filesystem::path ConfigPath;
		std::string SceneSource;
		std::string SceneAsset;

		std::filesystem::path CapturePath;
		uint32_t CaptureFrame = 2;

		// 浮点参考图：写出来给后续比较，或与已有参考比较（失败时进程返回非零）
		std::filesystem::path ReferencePath;
		std::filesystem::path WriteReferencePath;
		float Tolerance = 0.01f;

		// 性能测量：预热后测量固定帧数，写出指标 CSV 后退出
		uint32_t BenchFrames = 0;
		uint32_t BenchWarmup = 30;
		std::filesystem::path MetricsPath;

		// 公共调试视图：0 = 显示实验输出，n = 显示第 n 个登记的中间结果
		// （配合 --capture 可以给中间结果截图，不需要手点面板）
		int DebugView = 0;

		uint32_t Width = 0;
		uint32_t Height = 0;

		bool bDisableVsync = false;
		bool bDisableGpuTiming = false;
		bool bShowHelp = false;

		// 捕获/参考比较/性能测量的帧数上限（0 表示不做）
		[[nodiscard]] bool WantsHeadlessRun() const
		{
			return BenchFrames > 0 || !CapturePath.empty() || !ReferencePath.empty() ||
				   !WriteReferencePath.empty();
		}

		// 未识别的参数：留给实验自己解析
		std::vector<std::string> ExtraArguments;
	};

	FCommandLine ParseCommandLine(int Argc, char** Argv);
	std::string GetCommandLineUsage();
} // namespace Prism::Host
