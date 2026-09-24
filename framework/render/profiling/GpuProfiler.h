#pragma once

// NVRHI layer: per-pass GPU timing.
//
// The profiler owns one timer query per (scope, frame in flight): a query is reused only after its
// result has been read. Nested scopes are inclusive; Frame is measured separately.

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <string>
#include <vector>

namespace prism::gpu
{
    class GpuProfiler
    {
    public:
        static constexpr uint32_t kFramesInFlight = 3;

        GpuProfiler(nvrhi::IDevice* device, uint32_t maxScopes = 24);

        // 在帧开始时调用一次：收集上一批结果，并把当前槽位准备成可写状态。
        void BeginFrame(nvrhi::ICommandList* commands);
        void EndFrame();

        void BeginScope(nvrhi::ICommandList* commands, const char* name);
        void EndScope(nvrhi::ICommandList* commands);

        struct ScopeTiming
        {
            std::string name;
            float milliseconds = 0.f;           // 上一次可用结果
            float smoothedMilliseconds = 0.f;   // 指数平滑，便于观察
            bool valid = false;
            uint64_t frameIndex = 0;
        };

        [[nodiscard]] const std::vector<ScopeTiming>& GetTimings() const { return m_Timings; }
        [[nodiscard]] float GetTotalMilliseconds() const;

        void SetEnabled(bool enabled);
        [[nodiscard]] bool IsEnabled() const { return m_Enabled; }

        void Reset();

    private:
        struct Scope
        {
            std::string name;
            std::vector<nvrhi::TimerQueryHandle> queries;
            bool active = false;
            bool pending[kFramesInFlight]{};
            uint64_t frames[kFramesInFlight]{};
            uint64_t lastRecordedFrame = ~uint64_t(0);
        };

        Scope* FindOrCreateScope(const char* name);

        nvrhi::IDevice* m_Device = nullptr;
        uint32_t m_MaxScopes = 0;
        uint32_t m_FrameSlot = 0;
        uint64_t m_FrameCount = 0;
        bool m_Enabled = true;
        bool m_FrameOpen = false;

        std::vector<Scope> m_Scopes;
        std::vector<ScopeTiming> m_Timings;
        std::vector<int> m_Stack;
        uint64_t m_MinResultFrame = 0;
    };

    // RAII 版本：记录 GPU 时间，同时写入调试标记（PIX / Nsight 中可见同名区间）。
    class ScopedGpuScope
    {
    public:
        ScopedGpuScope(GpuProfiler& profiler, nvrhi::ICommandList* commands, const char* name)
            : m_Profiler(profiler)
            , m_Commands(commands)
        {
            if (commands)
                commands->beginMarker(name);

            m_Profiler.BeginScope(commands, name);
        }

        ~ScopedGpuScope()
        {
            m_Profiler.EndScope(m_Commands);

            if (m_Commands)
                m_Commands->endMarker();
        }

        ScopedGpuScope(const ScopedGpuScope&) = delete;
        ScopedGpuScope& operator=(const ScopedGpuScope&) = delete;

    private:
        GpuProfiler& m_Profiler;
        nvrhi::ICommandList* m_Commands = nullptr;
    };
}
