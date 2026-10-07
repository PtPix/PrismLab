#pragma once

// Host 层：指标收集与导出。
//
// 宿主每帧收集帧统计（CPU 帧时间、GPU 总时间、每个 Pass 的时间戳），实验通过
// ExperimentContext::metrics 上报自己的数值（间接光均值、NaN 计数、reservoir 复用率……）。
//
// 导出格式：CSV，逐帧一行，末尾以 '#' 开头写上下文与汇总（mean/min/max），
// 既能直接看，也能被工具按 '#' 过滤后解析。性能记录需要的信息（GPU、分辨率、构建模式、
// 采样参数、预热与测量区间）由 --bench 与运行日志共同给出。

#include <framework/core/Types.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Prism::Host
{
	class FMetrics
	{
	  public:
		void SetContext(std::string InExperimentName, std::string InSceneDescription, std::string InRendererDescription,
						FExtent2D InRenderSize, FExtent2D InOutputSize);

		// 每帧一次：BeginFrame 清空本帧临时值，EndFrame 并入统计。
		void BeginFrame(uint64_t InFrameIndex);
		void EndFrame();

		// 覆盖式写入（同一帧多次调用以最后一次为准）
		void Set(const char* Name, double Value);

		// 累加（同一帧多次调用求和，例如"本帧所有 Pass 的样本数"）
		void Add(const char* Name, double Value);

		struct FSeries
		{
			std::string Name;
			double Last = 0.0;
			double Mean = 0.0;
			double Min = 0.0;
			double Max = 0.0;
			uint64_t Samples = 0;
		};

		[[nodiscard]] const std::vector<FSeries>& GetSeries() const
		{
			return MetricSeries;
		}
		[[nodiscard]] uint64_t GetMeasuredFrameCount() const
		{
			return FrameCount;
		}

		// 从下一次 BeginFrame 起丢弃已有统计（bench 的预热结束点）。
		void Reset();

		void SetEnabled(bool bInEnabled)
		{
			bEnabled = bInEnabled;
		}
		[[nodiscard]] bool IsEnabled() const
		{
			return bEnabled;
		}

		bool WriteCsv(const std::filesystem::path& Path) const;

	  private:
		struct FFrameRow
		{
			uint64_t FrameIndex = 0;
			std::vector<double> Values;
		};

		int FindOrAdd(const char* Name);

		bool bEnabled = true;
		uint64_t CurrentFrameIndex = 0;
		uint64_t FrameCount = 0;
		bool bFrameOpen = false;

		std::string ExperimentName = "(unknown)";
		std::string SceneDescription = "(unknown)";
		std::string RendererDescription = "(unknown)";
		FExtent2D RenderSize;
		FExtent2D OutputSize;

		std::vector<FSeries> MetricSeries;
		std::vector<double> Pending;
		std::vector<uint8_t> Touched;
		std::vector<FFrameRow> Frames;
	};
} // namespace Prism::Host
