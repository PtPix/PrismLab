#include "Metrics.h"

#include <donut/core/log.h>

#include <algorithm>
#include <cstdio>

namespace Prism::Host
{
	namespace
	{
		std::string EscapeCsv(const std::string& Value)
		{
			if (Value.find_first_of(",\"\n") == std::string::npos)
				return Value;

			std::string Escaped = "\"";
			for (const char Character : Value)
			{
				if (Character == '"')
					Escaped += '"';

				Escaped += Character;
			}

			Escaped += '"';
			return Escaped;
		}
	} // namespace

	void FMetrics::SetContext(std::string InExperimentName, std::string InSceneDescription,
							  std::string InRendererDescription, FExtent2D InRenderSize, FExtent2D InOutputSize)
	{
		ExperimentName = std::move(InExperimentName);
		SceneDescription = std::move(InSceneDescription);
		RendererDescription = std::move(InRendererDescription);
		RenderSize = InRenderSize;
		OutputSize = InOutputSize;
	}

	int FMetrics::FindOrAdd(const char* Name)
	{
		if (!Name)
			return -1;

		for (size_t Index = 0; Index < MetricSeries.size(); ++Index)
		{
			if (MetricSeries[Index].Name == Name)
				return int(Index);
		}

		FSeries NewSeries;
		NewSeries.Name = Name;
		MetricSeries.push_back(std::move(NewSeries));
		Pending.push_back(0.0);
		Touched.push_back(0);

		return int(MetricSeries.size()) - 1;
	}

	void FMetrics::BeginFrame(uint64_t InFrameIndex)
	{
		CurrentFrameIndex = InFrameIndex;
		bFrameOpen = true;

		std::fill(Pending.begin(), Pending.end(), 0.0);
		std::fill(Touched.begin(), Touched.end(), 0);
	}

	void FMetrics::Set(const char* Name, double Value)
	{
		if (!bEnabled || !bFrameOpen)
			return;

		const int Index = FindOrAdd(Name);
		if (Index < 0)
			return;

		Pending[size_t(Index)] = Value;
		Touched[size_t(Index)] = 1;
	}

	void FMetrics::Add(const char* Name, double Value)
	{
		if (!bEnabled || !bFrameOpen)
			return;

		const int Index = FindOrAdd(Name);
		if (Index < 0)
			return;

		Pending[size_t(Index)] += Value;
		Touched[size_t(Index)] = 1;
	}

	void FMetrics::EndFrame()
	{
		if (!bFrameOpen)
			return;

		bFrameOpen = false;

		if (!bEnabled)
			return;

		FFrameRow Row;
		Row.FrameIndex = CurrentFrameIndex;
		Row.Values = Pending;

		for (size_t Index = 0; Index < MetricSeries.size(); ++Index)
		{
			if (!Touched[Index])
				continue;

			FSeries& SeriesData = MetricSeries[Index];
			SeriesData.Last = Pending[Index];

			if (SeriesData.Samples == 0)
			{
				SeriesData.Min = SeriesData.Last;
				SeriesData.Max = SeriesData.Last;
				SeriesData.Mean = SeriesData.Last;
			}
			else
			{
				SeriesData.Min = std::min(SeriesData.Min, SeriesData.Last);
				SeriesData.Max = std::max(SeriesData.Max, SeriesData.Last);
				SeriesData.Mean += (SeriesData.Last - SeriesData.Mean) / double(SeriesData.Samples + 1);
			}

			++SeriesData.Samples;
		}

		Frames.push_back(std::move(Row));
		++FrameCount;
	}

	void FMetrics::Reset()
	{
		FrameCount = 0;
		Frames.clear();

		for (FSeries& SeriesData : MetricSeries)
			SeriesData = FSeries{SeriesData.Name};
	}

	bool FMetrics::WriteCsv(const std::filesystem::path& Path) const
	{
		FILE* File = nullptr;
		if (_wfopen_s(&File, Path.c_str(), L"w") != 0 || !File)
		{
			donut::log::error("Prism: cannot write metrics to %s", Path.string().c_str());
			return false;
		}

		// 表头
		fprintf(File, "frame");
		for (const FSeries& SeriesData : MetricSeries)
			fprintf(File, ",%s", SeriesData.Name.c_str());
		fprintf(File, "\n");

		// 逐帧数据
		for (const FFrameRow& Row : Frames)
		{
			fprintf(File, "%llu", (unsigned long long)Row.FrameIndex);
			for (size_t Index = 0; Index < MetricSeries.size(); ++Index)
			{
				const double Value = (Index < Row.Values.size()) ? Row.Values[Index] : 0.0;
				fprintf(File, ",%.6f", Value);
			}
			fprintf(File, "\n");
		}

		// 上下文与汇总（以 '#' 开头，便于解析时过滤）
		fprintf(File, "# experiment,%s\n", EscapeCsv(ExperimentName).c_str());
		fprintf(File, "# scene,%s\n", EscapeCsv(SceneDescription).c_str());
		fprintf(File, "# renderer,%s\n", EscapeCsv(RendererDescription).c_str());
		fprintf(File, "# render_size,%ux%u\n", RenderSize.Width, RenderSize.Height);
		fprintf(File, "# output_size,%ux%u\n", OutputSize.Width, OutputSize.Height);
		fprintf(File, "# measured_frames,%llu\n", (unsigned long long)FrameCount);

		for (const FSeries& SeriesData : MetricSeries)
		{
			if (SeriesData.Samples == 0)
				continue;

			fprintf(File, "# summary %s,mean=%.6f,min=%.6f,max=%.6f,samples=%llu\n", SeriesData.Name.c_str(),
					SeriesData.Mean, SeriesData.Min, SeriesData.Max, (unsigned long long)SeriesData.Samples);
		}

		fclose(File);

		donut::log::info("Prism: metrics written to %s (%llu measured frames).", Path.string().c_str(),
						 (unsigned long long)FrameCount);
		return true;
	}
} // namespace Prism::Host
