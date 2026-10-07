#pragma once

// Host 层：参数表。
//
// 一个描述符表同时驱动三件事：ImGui 控件、JSON 读写、参数 hash（用于自动请求历史重置）。
// 参数结构体本身保持普通 POD，可读、可直接写进常量缓冲。
//
// Feature code declares an F-prefixed settings struct with PascalCase fields, then describes those
// fields in a K-prefixed FParamDesc array. FParamTable binds the struct and provides JSON loading,
// ImGui controls, and history-invalidating hashes without per-field plumbing.

#include <framework/core/Types.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace Json
{
	class Value;
}

namespace Prism::Host
{
	enum class EParamKind : uint32_t
	{
		Bool = 0,
		Int,
		Float,
	};

	enum class EParamFlags : uint32_t
	{
		None = 0,

		// 该参数变化会使历史失效（参与 ComputeHash，便于自动请求 HistoryReset）
		HistoryInvalidating = 1u << 0,

		// 只读显示（例如统计值），UI 上不可编辑
		ReadOnly = 1u << 1,
	};

	constexpr EParamFlags operator|(EParamFlags A, EParamFlags B)
	{
		return EParamFlags(uint32_t(A) | uint32_t(B));
	}
	constexpr bool HasAny(EParamFlags Value, EParamFlags Test)
	{
		return (uint32_t(Value) & uint32_t(Test)) != 0;
	}

	struct FParamDesc
	{
		const char* Name = nullptr;	 // JSON 键
		const char* Label = nullptr; // UI 标签
		EParamKind Kind = EParamKind::Float;
		uint32_t Offset = 0; // 字段在参数结构体中的字节偏移
		float MinValue = 0.f;
		float MaxValue = 1.f;
		EParamFlags Flags = EParamFlags::None;
		const char* Help = nullptr; // 可选：鼠标悬停提示
	};

	class FParamTable
	{
	  public:
		template <size_t N> FParamTable(const FParamDesc (&Descriptors)[N]) : Descriptors(Descriptors, Descriptors + N)
		{
		}

		FParamTable() = default;

		template <class InSettings> void Bind(InSettings* InInstance)
		{
			Instance = InInstance;
		}

		// ImGui 控件（布尔用复选框，整数/浮点用滑块）。
		void BuildUI();

		// 读配置：缺失的键保留默认值；类型不符时记录 warning 并跳过。
		void LoadJson(const Json::Value& Settings);

		// 写配置：把全部参数按 JSON 键写回。
		void SaveJson(Json::Value& Settings) const;

		// 只统计标了 HistoryInvalidating 的字段；用于"参数改了才重置历史"。
		[[nodiscard]] uint64_t ComputeHash() const;

		// UI 或 JSON 是否改动过（feature 每帧检查一次后 ClearEdited）。
		[[nodiscard]] bool WasEdited() const
		{
			return bEdited;
		}
		void ClearEdited()
		{
			bEdited = false;
		}

		[[nodiscard]] const std::vector<FParamDesc>& GetDescriptors() const
		{
			return Descriptors;
		}
		[[nodiscard]] bool IsBound() const
		{
			return Instance != nullptr;
		}

	  private:
		void* FieldAddress(const FParamDesc& Descriptor) const;

		std::vector<FParamDesc> Descriptors;
		void* Instance = nullptr;
		bool bEdited = false;
	};

// 字段描述符：把设置结构体字段、JSON 键、UI 标签和标志绑在一起。
#define PRISM_PARAM_BOOL(StructType, Member, Label, Flags)                                                             \
	::Prism::Host::FParamDesc                                                                                          \
	{                                                                                                                  \
		#Member, Label, ::Prism::Host::EParamKind::Bool, uint32_t(offsetof(StructType, Member)), 0.f, 1.f, (Flags),    \
			nullptr                                                                                                    \
	}

#define PRISM_PARAM_INT(StructType, Member, Label, MinValue, MaxValue, Flags)                                          \
	::Prism::Host::FParamDesc                                                                                          \
	{                                                                                                                  \
		#Member, Label, ::Prism::Host::EParamKind::Int, uint32_t(offsetof(StructType, Member)), float(MinValue),       \
			float(MaxValue), (Flags), nullptr                                                                          \
	}

#define PRISM_PARAM_FLOAT(StructType, Member, Label, MinValue, MaxValue, Flags)                                        \
	::Prism::Host::FParamDesc                                                                                          \
	{                                                                                                                  \
		#Member, Label, ::Prism::Host::EParamKind::Float, uint32_t(offsetof(StructType, Member)), float(MinValue),     \
			float(MaxValue), (Flags), nullptr                                                                          \
	}
} // namespace Prism::Host
