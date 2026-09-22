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

        if (!m_Enabled)
            return;

        m_FrameSlot = uint32_t(m_FrameCount % kFramesInFlight);

        // 只有当该槽位的查询已经解析完成时，才把结果记为上一次的时间并复用查询。
        if (m_FrameCount >= kFramesInFlight)
        {
            for (size_t index = 0; index < m_Scopes.size(); ++index)
            {
                Scope& scope = m_Scopes[index];
                nvrhi::ITimerQuery* query = scope.queries[m_FrameSlot].Get();

                if (!m_Device->pollTimerQuery(query))
                    continue;

                const float milliseconds = m_Device->getTimerQueryTime(query) * 1000.f;
                m_Device->resetTimerQuery(query);

                ScopeTiming& timing = m_Timings[index];
                timing.milliseconds = milliseconds;
                timing.smoothedMilliseconds = timing.valid
                    ? (timing.smoothedMilliseconds * 0.9f + milliseconds * 0.1f)
                    : milliseconds;
                timing.valid = true;
            }
        }

        m_FrameOpen = true;
    }

    void GpuProfiler::EndFrame()
    {
        m_FrameOpen = false;
        ++m_FrameCount;
    }

    void GpuProfiler::BeginScope(nvrhi::ICommandList* commands, const char* name)
    {
        if (!m_Enabled || !m_FrameOpen || !commands)
            return;

        Scope* scope = FindOrCreateScope(name);
        if (!scope || scope->active)
            return;

        commands->beginTimerQuery(scope->queries[m_FrameSlot].Get());
        scope->active = true;
    }

    void GpuProfiler::EndScope(nvrhi::ICommandList* commands)
    {
        if (!m_Enabled || !m_FrameOpen || !commands)
            return;

        for (Scope& scope : m_Scopes)
        {
            if (!scope.active)
                continue;

            commands->endTimerQuery(scope.queries[m_FrameSlot].Get());
            scope.active = false;
        }
    }

    float GpuProfiler::GetTotalMilliseconds() const
    {
        float total = 0.f;
        for (const ScopeTiming& timing : m_Timings)
        {
            if (timing.valid)
                total += timing.milliseconds;
        }

        return total;
    }

    void GpuProfiler::SetEnabled(bool enabled)
    {
        m_Enabled = enabled;
    }

    void GpuProfiler::Reset()
    {
        m_Scopes.clear();
        m_Timings.clear();
        m_FrameCount = 0;
        m_FrameSlot = 0;
    }
}
