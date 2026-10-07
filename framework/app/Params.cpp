#include "Params.h"

#include <donut/core/json.h>
#include <donut/core/log.h>

#include <imgui.h>

#include <cstring>

namespace Prism::Host
{
	namespace
	{
		// 字段字节数：用于 hash 与 JSON 类型检查
		size_t FieldSize(EParamKind Kind)
		{
			switch (Kind)
			{
				case EParamKind::Bool:
					return sizeof(bool);
				case EParamKind::Int:
					return sizeof(int);
				case EParamKind::Float:
					return sizeof(float);
				default:
					return 0;
			}
		}

		const char* ToString(EParamKind Kind)
		{
			switch (Kind)
			{
				case EParamKind::Bool:
					return "bool";
				case EParamKind::Int:
					return "int";
				case EParamKind::Float:
					return "float";
				default:
					return "unknown";
			}
		}
	} // namespace

	void* FParamTable::FieldAddress(const FParamDesc& Descriptor) const
	{
		if (!Instance)
			return nullptr;

		return static_cast<uint8_t*>(Instance) + Descriptor.Offset;
	}

	void FParamTable::BuildUI()
	{
		if (!Instance || Descriptors.empty())
		{
			ImGui::TextDisabled("(no parameters)");
			return;
		}

		for (const FParamDesc& Descriptor : Descriptors)
		{
			void* Field = FieldAddress(Descriptor);
			if (!Field)
				continue;

			const bool bHistoryInvalidating = HasAny(Descriptor.Flags, EParamFlags::HistoryInvalidating);
			const bool bReadOnly = HasAny(Descriptor.Flags, EParamFlags::ReadOnly);

			ImGui::PushID(Descriptor.Name);
			if (bReadOnly)
				ImGui::BeginDisabled();

			bool bChanged = false;

			switch (Descriptor.Kind)
			{
				case EParamKind::Bool:
					bChanged = ImGui::Checkbox(Descriptor.Label, reinterpret_cast<bool*>(Field));
					break;

				case EParamKind::Int:
					bChanged = ImGui::SliderInt(Descriptor.Label, reinterpret_cast<int*>(Field),
												int(Descriptor.MinValue), int(Descriptor.MaxValue));
					break;

				case EParamKind::Float:
					bChanged = ImGui::SliderFloat(Descriptor.Label, reinterpret_cast<float*>(Field),
												  Descriptor.MinValue, Descriptor.MaxValue, "%.3f");
					break;

				default:
					break;
			}

			if (bReadOnly)
				ImGui::EndDisabled();

			if (Descriptor.Help && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", Descriptor.Help);

			if (bHistoryInvalidating)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("(resets history)");
			}

			ImGui::PopID();

			if (bChanged)
				bEdited = true;
		}
	}

	void FParamTable::LoadJson(const Json::Value& Settings)
	{
		if (!Instance)
			return;

		for (const FParamDesc& Descriptor : Descriptors)
		{
			if (!Settings.isMember(Descriptor.Name))
				continue;

			const Json::Value& Value = Settings[Descriptor.Name];
			void* Field = FieldAddress(Descriptor);

			switch (Descriptor.Kind)
			{
				case EParamKind::Bool:
				{
					if (!Value.isBool())
					{
						donut::log::warning("Prism: parameter '%s' expects %s in the config, ignoring.",
											Descriptor.Name, ToString(Descriptor.Kind));
						break;
					}

					const bool bPrevious = *reinterpret_cast<bool*>(Field);
					Value >> *reinterpret_cast<bool*>(Field);
					bEdited |= (bPrevious != *reinterpret_cast<bool*>(Field));
					break;
				}

				case EParamKind::Int:
				{
					if (!Value.isInt())
					{
						donut::log::warning("Prism: parameter '%s' expects %s in the config, ignoring.",
											Descriptor.Name, ToString(Descriptor.Kind));
						break;
					}

					const int Previous = *reinterpret_cast<int*>(Field);
					Value >> *reinterpret_cast<int*>(Field);
					bEdited |= (Previous != *reinterpret_cast<int*>(Field));
					break;
				}

				case EParamKind::Float:
				{
					if (!Value.isNumeric())
					{
						donut::log::warning("Prism: parameter '%s' expects %s in the config, ignoring.",
											Descriptor.Name, ToString(Descriptor.Kind));
						break;
					}

					const float Previous = *reinterpret_cast<float*>(Field);
					Value >> *reinterpret_cast<float*>(Field);
					bEdited |= (Previous != *reinterpret_cast<float*>(Field));
					break;
				}

				default:
					break;
			}
		}
	}

	void FParamTable::SaveJson(Json::Value& Settings) const
	{
		if (!Instance)
			return;

		for (const FParamDesc& Descriptor : Descriptors)
		{
			void* Field = FieldAddress(Descriptor);
			if (!Field)
				continue;

			Json::Value& Value = Settings[Descriptor.Name];

			switch (Descriptor.Kind)
			{
				case EParamKind::Bool:
					Value = *reinterpret_cast<bool*>(Field);
					break;
				case EParamKind::Int:
					Value = *reinterpret_cast<int*>(Field);
					break;
				case EParamKind::Float:
					Value = *reinterpret_cast<float*>(Field);
					break;
				default:
					break;
			}
		}
	}

	uint64_t FParamTable::ComputeHash() const
	{
		// FNV-1a：只覆盖标记为 HistoryInvalidating 的字段；字节级比较，float 的 -0 与 0 视为不同。
		uint64_t Hash = 1469598103934665603ull;

		for (const FParamDesc& Descriptor : Descriptors)
		{
			if (!HasAny(Descriptor.Flags, EParamFlags::HistoryInvalidating))
				continue;

			const uint8_t* Bytes = static_cast<const uint8_t*>(FieldAddress(Descriptor));
			if (!Bytes)
				continue;

			const size_t Size = FieldSize(Descriptor.Kind);
			for (size_t Index = 0; Index < Size; ++Index)
			{
				Hash ^= Bytes[Index];
				Hash *= 1099511628211ull;
			}
		}

		return Hash;
	}
} // namespace Prism::Host
