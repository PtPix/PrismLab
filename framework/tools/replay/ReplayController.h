#pragma once
#include "ReplayTrack.h"
#include <functional>

namespace prism::host
{
    class ReplayController
    {
    public:
        enum class Mode { Live, Recording, Playback };
        struct Frame { uint64_t tick = 0; double time = 0; float delta = 0; uint32_t seed = 1; };
        // Register sample-owned parameters. Scene animation consumes Frame::time independently.
        std::function<Json::Value()> captureParameters;
        std::function<void(const Json::Value&)> restoreParameters;
        void Record();
        bool Play();
        void Stop();
        void Restart();
        void Pause(bool paused) { m_Paused = paused; }
        void Step() { m_Paused = true; m_Step = true; }
        bool IsPaused() const { return m_Paused; }
        Mode GetMode() const { return m_Mode; }
        bool BlocksInput() const { return m_Mode == Mode::Playback || m_Paused; }
        void SetFixedStep(bool enabled, double delta);
        bool IsFixedStep() const { return m_Fixed; }
        double FixedDelta() const { return m_Track.fixedDelta; }
        void SetSeed(uint32_t seed) { if (m_Mode == Mode::Live) { m_Track.seed = seed; m_Reset = true; } }
        const Frame& BeginFrame(float realDelta);
        void EndFrame(const CameraPose& camera);
        const ReplaySample* PlaybackSample() const;
        bool ConsumeReset() { bool value = m_Reset; m_Reset = false; return value; }
        const Frame& GetFrame() const { return m_Frame; }
        const ReplayTrack& Track() const { return m_Track; }
        Status Save(const std::filesystem::path& path) const { return m_Track.Save(path); }
        Status Load(const std::filesystem::path& path);
    private:
        ReplayTrack m_Track;
        Frame m_Frame;
        Mode m_Mode = Mode::Live;
        bool m_Fixed = false, m_Paused = false, m_Step = false, m_First = true, m_Reset = true;
    };
}
