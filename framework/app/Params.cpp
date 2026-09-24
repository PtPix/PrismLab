#include "Params.h"

#include <donut/core/json.h>
#include <donut/core/log.h>

#include <imgui.h>

#include <cstring>

namespace prism::host
{
    namespace
    {
        // 字段字节数：用于 hash 与 JSON 类型检查
        size_t FieldSize(ParamKind kind)
        {
            switch (kind)
            {
            case ParamKind::Bool:  return sizeof(bool);
            case ParamKind::Int:   return sizeof(int);
            case ParamKind::Float: return sizeof(float);
            default:               return 0;
            }
        }

        const char* ToString(ParamKind kind)
        {
            switch (kind)
            {
            case ParamKind::Bool:  return "bool";
            case ParamKind::Int:   return "int";
            case ParamKind::Float: return "float";
            default:               return "unknown";
            }
        }
    }

    void* ParamTable::FieldAddress(const ParamDesc& descriptor) const
    {
        if (!m_Instance)
            return nullptr;

        return static_cast<uint8_t*>(m_Instance) + descriptor.offset;
    }

    void ParamTable::BuildUI()
    {
        if (!m_Instance || m_Descriptors.empty())
        {
            ImGui::TextDisabled("(no parameters)");
            return;
        }

        for (const ParamDesc& descriptor : m_Descriptors)
        {
            void* field = FieldAddress(descriptor);
            if (!field)
                continue;

            const bool historyInvalidating = HasAny(descriptor.flags, ParamFlags::HistoryInvalidating);
            const bool readOnly = HasAny(descriptor.flags, ParamFlags::ReadOnly);

            ImGui::PushID(descriptor.name);
            if (readOnly)
                ImGui::BeginDisabled();

            bool changed = false;

            switch (descriptor.kind)
            {
            case ParamKind::Bool:
                changed = ImGui::Checkbox(descriptor.label, reinterpret_cast<bool*>(field));
                break;

            case ParamKind::Int:
                changed = ImGui::SliderInt(descriptor.label, reinterpret_cast<int*>(field),
                    int(descriptor.minValue), int(descriptor.maxValue));
                break;

            case ParamKind::Float:
                changed = ImGui::SliderFloat(descriptor.label, reinterpret_cast<float*>(field),
                    descriptor.minValue, descriptor.maxValue, "%.3f");
                break;

            default:
                break;
            }

            if (readOnly)
                ImGui::EndDisabled();

            if (descriptor.help && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", descriptor.help);

            if (historyInvalidating)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(resets history)");
            }

            ImGui::PopID();

            if (changed)
                m_Edited = true;
        }
    }

    void ParamTable::LoadJson(const Json::Value& settings)
    {
        if (!m_Instance)
            return;

        for (const ParamDesc& descriptor : m_Descriptors)
        {
            if (!settings.isMember(descriptor.name))
                continue;

            const Json::Value& value = settings[descriptor.name];
            void* field = FieldAddress(descriptor);

            switch (descriptor.kind)
            {
            case ParamKind::Bool:
            {
                if (!value.isBool())
                {
                    donut::log::warning("Prism: parameter '%s' expects %s in the config, ignoring.",
                        descriptor.name, ToString(descriptor.kind));
                    break;
                }

                const bool previous = *reinterpret_cast<bool*>(field);
                value >> *reinterpret_cast<bool*>(field);
                m_Edited |= (previous != *reinterpret_cast<bool*>(field));
                break;
            }

            case ParamKind::Int:
            {
                if (!value.isInt())
                {
                    donut::log::warning("Prism: parameter '%s' expects %s in the config, ignoring.",
                        descriptor.name, ToString(descriptor.kind));
                    break;
                }

                const int previous = *reinterpret_cast<int*>(field);
                value >> *reinterpret_cast<int*>(field);
                m_Edited |= (previous != *reinterpret_cast<int*>(field));
                break;
            }

            case ParamKind::Float:
            {
                if (!value.isNumeric())
                {
                    donut::log::warning("Prism: parameter '%s' expects %s in the config, ignoring.",
                        descriptor.name, ToString(descriptor.kind));
                    break;
                }

                const float previous = *reinterpret_cast<float*>(field);
                value >> *reinterpret_cast<float*>(field);
                m_Edited |= (previous != *reinterpret_cast<float*>(field));
                break;
            }

            default:
                break;
            }
        }
    }

    void ParamTable::SaveJson(Json::Value& settings) const
    {
        if (!m_Instance)
            return;

        for (const ParamDesc& descriptor : m_Descriptors)
        {
            void* field = FieldAddress(descriptor);
            if (!field)
                continue;

            Json::Value& value = settings[descriptor.name];

            switch (descriptor.kind)
            {
            case ParamKind::Bool:  value = *reinterpret_cast<bool*>(field); break;
            case ParamKind::Int:   value = *reinterpret_cast<int*>(field); break;
            case ParamKind::Float: value = *reinterpret_cast<float*>(field); break;
            default: break;
            }
        }
    }

    uint64_t ParamTable::ComputeHash() const
    {
        // FNV-1a：只覆盖标记为 HistoryInvalidating 的字段；字节级比较，float 的 -0 与 0 视为不同。
        uint64_t hash = 1469598103934665603ull;

        for (const ParamDesc& descriptor : m_Descriptors)
        {
            if (!HasAny(descriptor.flags, ParamFlags::HistoryInvalidating))
                continue;

            const uint8_t* bytes = static_cast<const uint8_t*>(FieldAddress(descriptor));
            if (!bytes)
                continue;

            const size_t size = FieldSize(descriptor.kind);
            for (size_t index = 0; index < size; ++index)
            {
                hash ^= bytes[index];
                hash *= 1099511628211ull;
            }
        }

        return hash;
    }
}
