#pragma once
#include <framework/tools/comparison/ComparisonPass.h>
#include <string>
#include <vector>

namespace prism::host
{
    class ComparisonController
    {
    public:
        struct Source { std::string id; gpu::ComparisonImage image; };
        gpu::ComparisonSettings settings;
        std::string sourceA = "Output", sourceB = "Output";
        bool useFrozenB = false;
        Status Initialize(nvrhi::IDevice* device, gpu::ShaderLibrary& shaders, donut::engine::CommonRenderPasses& common)
        { return m_Pass.Initialize(device, shaders, common); }
        void BeginFrame() { m_Sources.clear(); }
        void Publish(std::string id, gpu::ComparisonImage image);
        void RequestFreeze() { m_Freeze = true; }
        void ClearFrozen() { m_Pass.ClearFrozen(); useFrozenB = false; }
        gpu::ComparisonImage Record(nvrhi::ICommandList* commands, gpu::ComparisonImage fallback);
        const std::vector<Source>& Sources() const { return m_Sources; }
        const std::string& Message() const { return m_Message; }
    private:
        gpu::ComparisonImage Find(const std::string& id) const;
        gpu::ComparisonPass m_Pass;
        std::vector<Source> m_Sources;
        bool m_Freeze = false;
        std::string m_Message;
    };
}
