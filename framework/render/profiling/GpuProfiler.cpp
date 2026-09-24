#include "GpuProfiler.h"

#include <donut/core/log.h>

#include <algorithm>

namespace prism::gpu
{
    GpuProfiler::GpuProfiler(nvrhi::IDevice* device, uint32_t maxScopes)
        : m_Device(device)
        , m_MaxScopes(maxScopes)
    {
    }

    GpuProfiler::Scope* GpuProfiler::FindOrCreateScope(const char* name)
    {
        if (!name)
            return nullptr;

        for (Scope& scope : m_Scopes)
        {
            if (scope.name == name)
                return &scope;
        }

        if (m_Scopes.size() >= m_MaxScopes)
        {
            donut::log::warning("GpuProfiler: scope '%s' exceeds the limit of %u scopes.", name, m_MaxScopes);
            return nullptr;
        }

        Scope scope;
        scope.name = name;
        scope.queries.reserve(kFramesInFlight);

        for (uint32_t i = 0; i < kFramesInFlight; ++i)
            scope.queries.push_back(m_Device->createTimerQuery());

        m_Scopes.push_back(std::move(scope));

        ScopeTiming timing;
        timing.name = name;
        m_Timings.push_back(std::move(timing));

        return &m_Scopes.back();
    }

    void GpuProfiler::BeginFrame(nvrhi::ICommandList* commands)
    {
        (void)commands;
        m_FrameSlot = uint32_t(m_FrameCount % kFramesInFlight);
        for (size_t i = 0; i < m_Scopes.size(); ++i)
        {
            auto& scope = m_Scopes[i];
            for (uint32_t slot = 0; slot < kFramesInFlight; ++slot)
            {
                if (!scope.pending[slot] || !m_Device->pollTimerQuery(scope.queries[slot])) continue;
                const float ms = m_Device->getTimerQueryTime(scope.queries[slot]) * 1000.f;
                m_Device->resetTimerQuery(scope.queries[slot]); scope.pending[slot] = false;
                auto& timing = m_Timings[i];
                if (scope.frames[slot] < m_MinResultFrame || (timing.valid && scope.frames[slot] <= timing.frameIndex)) continue;
                timing.milliseconds = ms;
                timing.smoothedMilliseconds = timing.valid ? timing.smoothedMilliseconds * 0.9f + ms * 0.1f : ms;
                timing.frameIndex = scope.frames[slot]; timing.valid = true;
            }
        }
        m_FrameOpen = true;
    }

    void GpuProfiler::EndFrame()
    {
        assert(m_Stack.empty());
        m_FrameOpen = false; ++m_FrameCount;
    }

    void GpuProfiler::BeginScope(nvrhi::ICommandList* commands, const char* name)
    {
        m_Stack.push_back(-1);
        if (!m_Enabled || !m_FrameOpen || !commands) return;
        auto* scope = FindOrCreateScope(name);
        if (!scope || scope->active || scope->pending[m_FrameSlot] || scope->lastRecordedFrame == m_FrameCount) return;
        m_Stack.back() = int(scope - m_Scopes.data());
        commands->beginTimerQuery(scope->queries[m_FrameSlot]);
        scope->active = true; scope->lastRecordedFrame = m_FrameCount;
        scope->frames[m_FrameSlot] = m_FrameCount;
    }

    void GpuProfiler::EndScope(nvrhi::ICommandList* commands)
    {
        if (m_Stack.empty()) return;
        int index = m_Stack.back(); m_Stack.pop_back();
        if (index < 0 || !commands) return;
        auto& scope = m_Scopes[size_t(index)];
        commands->endTimerQuery(scope.queries[m_FrameSlot]);
        scope.active = false; scope.pending[m_FrameSlot] = true;
    }

    float GpuProfiler::GetTotalMilliseconds() const
    {
        for (const auto& timing : m_Timings)
            if (timing.name == "Frame" && timing.valid) return timing.milliseconds;
        return 0.f;
    }

    void GpuProfiler::SetEnabled(bool enabled)
    {
        m_Enabled = enabled;
    }

    void GpuProfiler::Reset()
    {
        m_MinResultFrame = m_FrameCount;
        for (auto& timing : m_Timings) timing.valid = false;
    }
}
