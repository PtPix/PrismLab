#pragma once

// Host 层：实验登记的"可显示中间结果"。
//
// 实验在每帧的 Record 阶段调用 Publish 登记纹理（可以登记多张）；宿主面板里下拉选择，
// 选中的纹理由公共调试 Pass（gpu::DebugViewPass）替换实验输出显示到交换链。
//
// 登记顺序必须每帧稳定（同一个实验里 Publish 的调用顺序固定），否则下拉选择会跳到别的纹理上。

#include <framework/nvrhi/DebugView.h>

#include <string>
#include <vector>

namespace renderlab::host
{
    struct DebugViewEntry
    {
        std::string group;      // 例如 "ForwardLab"
        std::string name;       // 例如 "Depth"
        nvrhi::ITexture* texture = nullptr;
        gpu::DebugViewSettings settings;

        [[nodiscard]] std::string GetLabel() const { return group + " / " + name; }
    };

    class DebugViewRegistry
    {
    public:
        // 每帧由宿主调用一次：清空上一帧登记（纹理是每帧重新解析的，不跨帧保留句柄）。
        void BeginFrame() { m_Entries.clear(); }

        void Publish(std::string group, std::string name, nvrhi::ITexture* texture, gpu::DebugViewSettings settings = {})
        {
            if (!texture)
                return;

            DebugViewEntry entry;
            entry.group = std::move(group);
            entry.name = std::move(name);
            entry.texture = texture;
            entry.settings = settings;
            m_Entries.push_back(std::move(entry));
        }

        [[nodiscard]] const std::vector<DebugViewEntry>& GetEntries() const { return m_Entries; }

        // 0 = 显示实验自己的输出；1..N = 对应条目。
        [[nodiscard]] int GetSelectedIndex() const { return m_SelectedIndex; }

        void SetSelectedIndex(int index) { m_SelectedIndex = (index < 0) ? 0 : index; }

        [[nodiscard]] const DebugViewEntry* GetSelected() const
        {
            if (m_SelectedIndex <= 0)
                return nullptr;

            const size_t index = size_t(m_SelectedIndex - 1);
            if (index >= m_Entries.size())
                return nullptr;

            return &m_Entries[index];
        }

        // 面板里编辑选中条目的显示参数（通道、缩放、偏移）。
        [[nodiscard]] gpu::DebugViewSettings GetSelectedSettings() const
        {
            const DebugViewEntry* entry = GetSelected();
            return entry ? entry->settings : gpu::DebugViewSettings{};
        }

        void SetSelectedSettings(const gpu::DebugViewSettings& settings)
        {
            DebugViewEntry* entry = GetSelectedMutable();
            if (entry)
                entry->settings = settings;
        }

    private:
        DebugViewEntry* GetSelectedMutable()
        {
            if (m_SelectedIndex <= 0)
                return nullptr;

            const size_t index = size_t(m_SelectedIndex - 1);
            if (index >= m_Entries.size())
                return nullptr;

            return &m_Entries[index];
        }

        std::vector<DebugViewEntry> m_Entries;
        int m_SelectedIndex = 0;
    };
}
