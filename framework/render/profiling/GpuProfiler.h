#pragma once

// NVRHI layer: per-pass GPU timing.
//
// The profiler owns one timer query per (scope, frame in flight): a query is reused only after its
// result has been read. Nested scopes are inclusive; Frame is measured separately.

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Prism::Gpu
{
	class FGpuProfiler
	{
	  public:
		static constexpr uint32_t KFramesInFlight = 3;

		FGpuProfiler(nvrhi::IDevice* InDevice, uint32_t InMaxScopes = 24);

		// 在帧开始时调用一次：收集上一批结果，并把当前槽位准备成可写状态。
		void BeginFrame(nvrhi::ICommandList* Commands);
		void EndFrame();

		void BeginScope(nvrhi::ICommandList* Commands, const char* Name);
		void EndScope(nvrhi::ICommandList* Commands);

		struct FScopeTiming
		{
			std::string Name;
			float Milliseconds = 0.f;		  // 上一次可用结果
			float SmoothedMilliseconds = 0.f; // 指数平滑，便于观察
			bool bValid = false;
			uint64_t FrameIndex = 0;
		};

		[[nodiscard]] const std::vector<FScopeTiming>& GetTimings() const
		{
			return Timings;
		}
		[[nodiscard]] float GetTotalMilliseconds() const;

		void SetEnabled(bool bInEnabled);
		[[nodiscard]] bool IsEnabled() const
		{
			return bEnabled;
		}

		void Reset();

	  private:
		struct FScope
		{
			std::string Name;
			std::vector<nvrhi::TimerQueryHandle> Queries;
			bool bActive = false;
			bool bPending[KFramesInFlight]{};
			uint64_t Frames[KFramesInFlight]{};
			uint64_t LastRecordedFrame = ~uint64_t(0);
		};

		FScope* FindOrCreateScope(const char* Name);

		nvrhi::IDevice* Device = nullptr;
		uint32_t MaxScopes = 0;
		uint32_t FrameSlot = 0;
		uint64_t FrameCount = 0;
		bool bEnabled = true;
		bool bFrameOpen = false;

		std::vector<FScope> Scopes;
		std::vector<FScopeTiming> Timings;
		std::vector<int> Stack;
		uint64_t MinResultFrame = 0;
	};

	// RAII 版本：记录 GPU 时间，同时写入调试标记（PIX / Nsight 中可见同名区间）。
	class FScopedGpuScope
	{
	  public:
		FScopedGpuScope(FGpuProfiler& Profiler, nvrhi::ICommandList* Commands, const char* Name)
			: Profiler(Profiler), Commands(Commands)
		{
			if (Commands)
				Commands->beginMarker(Name);

			Profiler.BeginScope(Commands, Name);
		}

		~FScopedGpuScope()
		{
			Profiler.EndScope(Commands);

			if (Commands)
				Commands->endMarker();
		}

		FScopedGpuScope(const FScopedGpuScope&) = delete;
		FScopedGpuScope& operator=(const FScopedGpuScope&) = delete;

	  private:
		FGpuProfiler& Profiler;
		nvrhi::ICommandList* Commands = nullptr;
	};
} // namespace Prism::Gpu
