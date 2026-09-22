#include "Metrics.h"

#include <donut/core/log.h>

#include <algorithm>
#include <cstdio>

namespace prism::host
{
    namespace
    {
        std::string EscapeCsv(const std::string& value)
        {
            if (value.find_first_of(",\"\n") == std::string::npos)
                return value;

            std::string escaped = "\"";
            for (const char character : value)
            {
                if (character == '"')
                    escaped += '"';

                escaped += character;
            }

            escaped += '"';
            return escaped;
        }
    }

    void Metrics::SetContext(
        std::string experimentName,
        std::string sceneDescription,
        std::string rendererDescription,
        Extent2D renderSize,
        Extent2D outputSize)
    {
        m_ExperimentName = std::move(experimentName);
        m_SceneDescription = std::move(sceneDescription);
        m_RendererDescription = std::move(rendererDescription);
        m_RenderSize = renderSize;
        m_OutputSize = outputSize;
    }

    int Metrics::FindOrAdd(const char* name)
    {
        if (!name)
            return -1;

        for (size_t index = 0; index < m_Series.size(); ++index)
        {
            if (m_Series[index].name == name)
                return int(index);
        }

        Series series;
        series.name = name;
        m_Series.push_back(std::move(series));
        m_Pending.push_back(0.0);
        m_Touched.push_back(0);

        return int(m_Series.size()) - 1;
    }

    void Metrics::BeginFrame(uint64_t frameIndex)
    {
        m_FrameIndex = frameIndex;
        m_FrameOpen = true;

        std::fill(m_Pending.begin(), m_Pending.end(), 0.0);
        std::fill(m_Touched.begin(), m_Touched.end(), 0);
    }

    void Metrics::Set(const char* name, double value)
    {
        if (!m_Enabled || !m_FrameOpen)
            return;

        const int index = FindOrAdd(name);
        if (index < 0)
            return;

        m_Pending[size_t(index)] = value;
        m_Touched[size_t(index)] = 1;
    }

    void Metrics::Add(const char* name, double value)
    {
        if (!m_Enabled || !m_FrameOpen)
            return;

        const int index = FindOrAdd(name);
        if (index < 0)
            return;

        m_Pending[size_t(index)] += value;
        m_Touched[size_t(index)] = 1;
    }

    void Metrics::EndFrame()
    {
        if (!m_FrameOpen)
            return;

        m_FrameOpen = false;

        if (!m_Enabled)
            return;

        FrameRow row;
        row.frameIndex = m_FrameIndex;
        row.values = m_Pending;

        for (size_t index = 0; index < m_Series.size(); ++index)
        {
            if (!m_Touched[index])
                continue;

            Series& series = m_Series[index];
            series.last = m_Pending[index];

            if (series.samples == 0)
            {
                series.min = series.last;
                series.max = series.last;
                series.mean = series.last;
            }
            else
            {
                series.min = std::min(series.min, series.last);
                series.max = std::max(series.max, series.last);
                series.mean += (series.last - series.mean) / double(series.samples + 1);
            }

            ++series.samples;
        }

        m_Frames.push_back(std::move(row));
        ++m_FrameCount;
    }

    void Metrics::Reset()
    {
        m_FrameCount = 0;
        m_Frames.clear();

        for (Series& series : m_Series)
            series = Series{ series.name };
    }

    bool Metrics::WriteCsv(const std::filesystem::path& path) const
    {
        FILE* file = nullptr;
        if (_wfopen_s(&file, path.c_str(), L"w") != 0 || !file)
        {
            donut::log::error("Prism: cannot write metrics to %s", path.string().c_str());
            return false;
        }

        // 表头
        fprintf(file, "frame");
        for (const Series& series : m_Series)
            fprintf(file, ",%s", series.name.c_str());
        fprintf(file, "\n");

        // 逐帧数据
        for (const FrameRow& row : m_Frames)
        {
            fprintf(file, "%llu", (unsigned long long)row.frameIndex);
            for (size_t index = 0; index < m_Series.size(); ++index)
            {
                const double value = (index < row.values.size()) ? row.values[index] : 0.0;
                fprintf(file, ",%.6f", value);
            }
            fprintf(file, "\n");
        }

        // 上下文与汇总（以 '#' 开头，便于解析时过滤）
        fprintf(file, "# experiment,%s\n", EscapeCsv(m_ExperimentName).c_str());
        fprintf(file, "# scene,%s\n", EscapeCsv(m_SceneDescription).c_str());
        fprintf(file, "# renderer,%s\n", EscapeCsv(m_RendererDescription).c_str());
        fprintf(file, "# render_size,%ux%u\n", m_RenderSize.width, m_RenderSize.height);
        fprintf(file, "# output_size,%ux%u\n", m_OutputSize.width, m_OutputSize.height);
        fprintf(file, "# measured_frames,%llu\n", (unsigned long long)m_FrameCount);

        for (const Series& series : m_Series)
        {
            if (series.samples == 0)
                continue;

            fprintf(file, "# summary %s,mean=%.6f,min=%.6f,max=%.6f,samples=%llu\n",
                series.name.c_str(), series.mean, series.min, series.max,
                (unsigned long long)series.samples);
        }

        fclose(file);

        donut::log::info("Prism: metrics written to %s (%llu measured frames).",
            path.string().c_str(), (unsigned long long)m_FrameCount);
        return true;
    }
}
