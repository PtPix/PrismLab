#pragma once

// Host 层：参数表。
//
// 一个描述符表同时驱动三件事：ImGui 控件、JSON 读写、参数 hash（用于自动请求历史重置）。
// 参数结构体本身保持普通 POD，可读、可直接写进常量缓冲。
//
// 声明（放在 feature 自己的头文件里）：
//
//   struct GiSettings { bool enabled = true; int candidateSamples = 4; float temporalWeight = 0.9f; };
//
//   inline const renderlab::host::ParamDesc kGiParams[] = {
//       RL_PARAM_BOOL (GiSettings, enabled,          "Enable GI",       renderlab::host::ParamFlags::None),
//       RL_PARAM_INT  (GiSettings, candidateSamples, "Candidates", 1, 32, renderlab::host::ParamFlags::HistoryInvalidating),
//       RL_PARAM_FLOAT(GiSettings, temporalWeight,   "Temporal w", 0,  1,  renderlab::host::ParamFlags::HistoryInvalidating),
//   };
//
// 使用（feature 的 Initialize / BuildUI / Render）：
//
//   m_Params = host::ParamTable(kGiParams);
//   m_Params.Bind(&m_Settings);
//   Json::Value lab; if (adapter::LoadLabSettings(*context.config, GetName(), lab)) m_Params.LoadJson(lab);
//
//   void BuildUI(...) override { m_Params.BuildUI(); }
//
//   // 参数变化 -> 历史失效（五行的标准写法，feature 自己拥有上一帧的 hash）
//   if (m_Params.WasEdited())
//   {
//       const uint64_t hash = m_Params.ComputeHash();
//       if (hash != m_LastParamHash) { m_LastParamHash = hash; context.callbacks.requestHistoryReset(HistoryResetReason::SettingsChange); }
//       m_Params.ClearEdited();
//   }

#include <framework/types/Types.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace Json
{
    class Value;
}

namespace renderlab::host
{
    enum class ParamKind : uint32_t
    {
        Bool = 0,
        Int,
        Float,
    };

    enum class ParamFlags : uint32_t
    {
        None = 0,

        // 该参数变化会使历史失效（参与 ComputeHash，便于自动请求 HistoryReset）
        HistoryInvalidating = 1u << 0,

        // 只读显示（例如统计值），UI 上不可编辑
        ReadOnly = 1u << 1,
    };

    constexpr ParamFlags operator|(ParamFlags a, ParamFlags b) { return ParamFlags(uint32_t(a) | uint32_t(b)); }
    constexpr bool HasAny(ParamFlags value, ParamFlags test) { return (uint32_t(value) & uint32_t(test)) != 0; }

    struct ParamDesc
    {
        const char* name = nullptr;      // JSON 键
        const char* label = nullptr;     // UI 标签
        ParamKind kind = ParamKind::Float;
        uint32_t offset = 0;             // 字段在参数结构体中的字节偏移
        float minValue = 0.f;
        float maxValue = 1.f;
        ParamFlags flags = ParamFlags::None;
        const char* help = nullptr;      // 可选：鼠标悬停提示
    };

    class ParamTable
    {
    public:
        template <size_t N>
        ParamTable(const ParamDesc (&descriptors)[N])
            : m_Descriptors(descriptors, descriptors + N)
        {
        }

        ParamTable() = default;

        template <class Settings>
        void Bind(Settings* instance)
        {
            m_Instance = instance;
        }

        // ImGui 控件（布尔用复选框，整数/浮点用滑块）。
        void BuildUI();

        // 读配置：缺失的键保留默认值；类型不符时记录 warning 并跳过。
        void LoadJson(const Json::Value& settings);

        // 写配置：把全部参数按 JSON 键写回。
        void SaveJson(Json::Value& settings) const;

        // 只统计标了 HistoryInvalidating 的字段；用于"参数改了才重置历史"。
        [[nodiscard]] uint64_t ComputeHash() const;

        // UI 或 JSON 是否改动过（feature 每帧检查一次后 ClearEdited）。
        [[nodiscard]] bool WasEdited() const { return m_Edited; }
        void ClearEdited() { m_Edited = false; }

        [[nodiscard]] const std::vector<ParamDesc>& GetDescriptors() const { return m_Descriptors; }
        [[nodiscard]] bool IsBound() const { return m_Instance != nullptr; }

    private:
        void* FieldAddress(const ParamDesc& descriptor) const;

        std::vector<ParamDesc> m_Descriptors;
        void* m_Instance = nullptr;
        bool m_Edited = false;
    };

    // 字段描述符：把设置结构体字段、JSON 键、UI 标签和标志绑在一起。
    #define RL_PARAM_BOOL(StructType, member, label, flags)                                              \
        ::renderlab::host::ParamDesc{ #member, label, ::renderlab::host::ParamKind::Bool,                \
            uint32_t(offsetof(StructType, member)), 0.f, 1.f, (flags), nullptr }

    #define RL_PARAM_INT(StructType, member, label, minValue, maxValue, flags)                            \
        ::renderlab::host::ParamDesc{ #member, label, ::renderlab::host::ParamKind::Int,                  \
            uint32_t(offsetof(StructType, member)), float(minValue), float(maxValue), (flags), nullptr }

    #define RL_PARAM_FLOAT(StructType, member, label, minValue, maxValue, flags)                          \
        ::renderlab::host::ParamDesc{ #member, label, ::renderlab::host::ParamKind::Float,                \
            uint32_t(offsetof(StructType, member)), float(minValue), float(maxValue), (flags), nullptr }
}
