#include "ReplayController.h"
#include <algorithm>
#include <cmath>

namespace prism::host
{
    void ReplayController::Restart()
    {
        if (m_Mode == Mode::Recording) m_Track.samples.clear();
        m_Frame = {}; m_First = true; m_Reset = true; m_Step = false;
    }
    void ReplayController::Record() { m_Track.samples.clear(); m_Mode = Mode::Recording; m_Paused = false; Restart(); }
    bool ReplayController::Play()
    { if (m_Track.samples.empty()) return false; m_Mode = Mode::Playback; m_Paused = false; Restart(); return true; }
    void ReplayController::Stop() { m_Mode = Mode::Live; m_Paused = false; Restart(); }
    void ReplayController::SetFixedStep(bool enabled, double delta)
    {
        if (m_Mode != Mode::Live || !std::isfinite(delta) || delta <= 0 || delta > 1) return;
        m_Fixed = enabled; m_Track.fixedDelta = delta; Restart();
    }
    const ReplayController::Frame& ReplayController::BeginFrame(float realDelta)
    {
        const bool advance = !m_First && (!m_Paused || m_Step);
        m_Frame.delta = 0;
        if (advance)
        {
            if (m_Mode == Mode::Playback && m_Frame.tick + 1 >= m_Track.samples.size()) m_Paused = true;
            else
            {
                ++m_Frame.tick;
                m_Frame.delta = float((m_Fixed || m_Mode != Mode::Live) ? m_Track.fixedDelta : std::max(0.f, realDelta));
                m_Frame.time = (m_Fixed || m_Mode != Mode::Live) ? double(m_Frame.tick) * m_Track.fixedDelta : m_Frame.time + m_Frame.delta;
            }
        }
        m_Frame.seed = m_Track.seed; m_Step = false; m_First = false;
        if (const auto* sample = PlaybackSample(); sample && restoreParameters)
            if (!captureParameters || captureParameters() != sample->parameters)
                restoreParameters(sample->parameters);
        return m_Frame;
    }
    void ReplayController::EndFrame(const CameraPose& camera)
    {
        if (m_Mode == Mode::Recording && m_Frame.tick == m_Track.samples.size())
            m_Track.samples.push_back({camera, captureParameters ? captureParameters() : Json::Value()});
    }
    const ReplaySample* ReplayController::PlaybackSample() const
    { return m_Mode == Mode::Playback && m_Frame.tick < m_Track.samples.size() ? &m_Track.samples[size_t(m_Frame.tick)] : nullptr; }
    Status ReplayController::Load(const std::filesystem::path& path)
    {
        if (m_Mode == Mode::Recording) return Status::Error(ErrorCode::InvalidArgument, "stop recording before loading");
        auto status = m_Track.Load(path); if (status) Stop(); return status;
    }
}
