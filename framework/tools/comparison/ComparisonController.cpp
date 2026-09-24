#include "ComparisonController.h"

namespace prism::host
{
    void ComparisonController::Publish(std::string id, gpu::ComparisonImage image)
    {
        for (auto& source : m_Sources) if (source.id == id) { source.image = image; return; }
        m_Sources.push_back({std::move(id), image});
    }
    gpu::ComparisonImage ComparisonController::Find(const std::string& id) const
    { for (const auto& source : m_Sources) if (source.id == id) return source.image; return {}; }
    gpu::ComparisonImage ComparisonController::Record(nvrhi::ICommandList* commands, gpu::ComparisonImage fallback)
    {
        Publish("Output", fallback); m_Message.clear();
        auto a = Find(sourceA), b = Find(sourceB);
        if (m_Freeze)
        {
            m_Freeze = false; auto status = m_Pass.Freeze(commands, b);
            if (!status) m_Message = status.ToStringWithCode(); else useFrozenB = true;
        }
        if (settings.mode == gpu::ComparisonMode::Off) return fallback;
        if (useFrozenB) b = m_Pass.Frozen();
        auto status = m_Pass.Record(commands, a, b, settings);
        if (!status) { m_Message = status.ToStringWithCode(); return fallback; }
        return {m_Pass.Output(), settings.mode == gpu::ComparisonMode::Difference ? ColorSpace::DisplayEncoded : a.colorSpace};
    }
}
