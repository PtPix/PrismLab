#pragma once

// Host 层：实验登记的"可显示中间结果"。
//
// 实验在每帧的 Record 阶段调用 Publish 登记纹理（可以登记多张）；宿主面板里下拉选择，
// 选中的纹理由公共调试 Pass（Gpu::FDebugViewPass）替换实验输出显示到交换链。
//
// 登记顺序必须每帧稳定（同一个实验里 Publish 的调用顺序固定），否则下拉选择会跳到别的纹理上。

#include <framework/tools/inspection/DebugView.h>

#include <string>
#include <vector>

namespace Prism::Host
{
	struct FDebugViewEntry
	{
		std::string Group; // 例如 "ForwardExperiment"
		std::string Name;  // 例如 "Depth"
		nvrhi::ITexture* Texture = nullptr;
		Gpu::FDebugViewSettings Settings;

		[[nodiscard]] std::string GetExperimentel() const
		{
			return Group + " / " + Name;
		}
	};

	class FDebugViewRegistry
	{
	  public:
		// 每帧由宿主调用一次：清空上一帧登记（纹理是每帧重新解析的，不跨帧保留句柄）。
		void BeginFrame()
		{
			Entries.clear();
		}

		void Publish(std::string Group, std::string Name, nvrhi::ITexture* Texture,
					 Gpu::FDebugViewSettings Settings = {})
		{
			if (!Texture)
				return;

			FDebugViewEntry Entry;
			Entry.Group = std::move(Group);
			Entry.Name = std::move(Name);
			Entry.Texture = Texture;
			Entry.Settings = Settings;
			Entries.push_back(std::move(Entry));
		}

		[[nodiscard]] const std::vector<FDebugViewEntry>& GetEntries() const
		{
			return Entries;
		}

		// 0 = 显示实验自己的输出；1..N = 对应条目。
		[[nodiscard]] int GetSelectedIndex() const
		{
			return SelectedIndex;
		}

		void SetSelectedIndex(int Index)
		{
			SelectedIndex = (Index < 0) ? 0 : Index;
		}

		[[nodiscard]] const FDebugViewEntry* GetSelected() const
		{
			if (SelectedIndex <= 0)
				return nullptr;

			const size_t Index = size_t(SelectedIndex - 1);
			if (Index >= Entries.size())
				return nullptr;

			return &Entries[Index];
		}

		// 面板里编辑选中条目的显示参数（通道、缩放、偏移）。
		[[nodiscard]] Gpu::FDebugViewSettings GetSelectedSettings() const
		{
			const FDebugViewEntry* Entry = GetSelected();
			return Entry ? Entry->Settings : Gpu::FDebugViewSettings{};
		}

		void SetSelectedSettings(const Gpu::FDebugViewSettings& Settings)
		{
			FDebugViewEntry* Entry = GetSelectedMutable();
			if (Entry)
				Entry->Settings = Settings;
		}

	  private:
		FDebugViewEntry* GetSelectedMutable()
		{
			if (SelectedIndex <= 0)
				return nullptr;

			const size_t Index = size_t(SelectedIndex - 1);
			if (Index >= Entries.size())
				return nullptr;

			return &Entries[Index];
		}

		std::vector<FDebugViewEntry> Entries;
		int SelectedIndex = 0;
	};
} // namespace Prism::Host
