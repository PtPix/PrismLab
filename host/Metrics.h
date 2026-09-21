#pragma once

// Host 层：指标收集与导出。
//
// 宿主每帧收集帧统计（CPU 帧时间、GPU 总时间、每个 Pass 的时间戳），实验通过
// LabContext::metrics 上报自己的数值（间接光均值、NaN 计数、reservoir 复用率……）。
//
// 导出格式：CSV，逐帧一行，末尾以 '#' 开头写上下文与汇总（mean/min/max），
// 既能直接看，也能被工具按 '#' 过滤后解析。性能记录需要的信息（GPU、分辨率、构建模式、
// 采样参数、预热与测量区间）由 --bench 与运行日志共同给出（见 docs/architecture.md）。

#include <renderlab/contracts/Types.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace renderlab::host
{
    class Metrics
    {
    public:
        void SetContext(
            std::string labName,
            std::string sceneDescription,
            std::string rendererDescription,
            Extent2D renderSize,
            Extent2D outputSize);

        // 每帧一次：BeginFrame 清空本帧临时值，EndFrame 并入统计。
        void BeginFrame(uint64_t frameIndex);
        void EndFrame();

        // 覆盖式写入（同一帧多次调用以最后一次为准）
        void Set(const char* name, double value);

        // 累加（同一帧多次调用求和，例如"本帧所有 Pass 的样本数"）
        void Add(const char* name, double value);

        struct Series
        {
            std::string name;
            double last = 0.0;
            double mean = 0.0;
            double min = 0.0;
            double max = 0.0;
            uint64_t samples = 0;
        };

        [[nodiscard]] const std::vector<Series>& GetSeries() const { return m_Series; }
        [[nodiscard]] uint64_t GetMeasuredFrameCount() const { return m_FrameCount; }

        // 从下一次 BeginFrame 起丢弃已有统计（bench 的预热结束点）。
        void Reset();

        void SetEnabled(bool enabled) { m_Enabled = enabled; }
        [[nodiscard]] bool IsEnabled() const { return m_Enabled; }

        bool WriteCsv(const std::filesystem::path& path) const;

    private:
        struct FrameRow
        {
            uint64_t frameIndex = 0;
            std::vector<double> values;
        };

        int FindOrAdd(const char* name);

        bool m_Enabled = true;
        uint64_t m_FrameIndex = 0;
        uint64_t m_FrameCount = 0;
        bool m_FrameOpen = false;

        std::string m_LabName = "(unknown)";
        std::string m_SceneDescription = "(unknown)";
        std::string m_RendererDescription = "(unknown)";
        Extent2D m_RenderSize;
        Extent2D m_OutputSize;

        std::vector<Series> m_Series;
        std::vector<double> m_Pending;
        std::vector<uint8_t> m_Touched;
        std::vector<FrameRow> m_Frames;
    };
}
