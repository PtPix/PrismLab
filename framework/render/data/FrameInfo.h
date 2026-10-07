#pragma once

// Framework types: per-frame and per-view data handed to every feature.
//
// A feature that keeps temporal history must key that history by viewId and must clear it when any
// of the reset reasons below applies. The host sets the flags; the feature decides what it can keep.

#include "framework/core/Types.h"

#include <string>

namespace Prism
{
	enum class EHistoryResetReason : uint32_t
	{
		None = 0,
		FirstFrame,		   // 第一帧，没有历史
		CameraCut,		   // 相机不连续（切换视角、瞬移、场景加载完成）
		ResolutionChange,  // 渲染分辨率或输出分辨率改变
		SettingsChange,	   // 影响算法的参数改变
		SceneChange,	   // 场景内容改变
		ResourceRecreated, // 历史资源被重建（例如设备丢失、显存紧张）
		Manual,			   // 实验代码主动请求
		Count
	};

	inline const char* ToString(EHistoryResetReason Reason)
	{
		switch (Reason)
		{
			case EHistoryResetReason::None:
				return "none";
			case EHistoryResetReason::FirstFrame:
				return "first frame";
			case EHistoryResetReason::CameraCut:
				return "camera cut";
			case EHistoryResetReason::ResolutionChange:
				return "resolution change";
			case EHistoryResetReason::SettingsChange:
				return "settings change";
			case EHistoryResetReason::SceneChange:
				return "scene change";
			case EHistoryResetReason::ResourceRecreated:
				return "resource recreated";
			case EHistoryResetReason::Manual:
				return "manual";
			default:
				return "unknown";
		}
	}

	// Single bit per reason, shifted past the None entry.
	constexpr uint32_t HistoryResetBit(EHistoryResetReason Reason)
	{
		return (Reason == EHistoryResetReason::None || Reason == EHistoryResetReason::Count)
				   ? 0u
				   : (1u << (uint32_t(Reason) - 1u));
	}

	struct FFrameInfo
	{
		uint64_t FrameIndex = 0;
		uint64_t SubmissionIndex = 0;
		uint32_t RandomSeed = 1;
		float DeltaTimeSeconds = 0.f;
		float TimeSeconds = 0.f;

		FViewId ViewId = KPrimaryViewId;

		// 场景数据的渲染分辨率；输出（交换链）分辨率可以不同，例如时域上采样。
		FExtent2D RenderSize;
		FExtent2D OutputSize;

		// 亚像素抖动，单位是渲染目标像素，作用于裁剪空间 xy。
		dm::float2 Jitter = dm::float2(0.f);
		dm::float2 PreviousJitter = dm::float2(0.f);

		uint32_t HistoryResetFlags = 0;

		[[nodiscard]] bool IsFirstFrame() const
		{
			return FrameIndex == 0;
		}
		[[nodiscard]] bool NeedsHistoryReset() const
		{
			return HistoryResetFlags != 0;
		}
		[[nodiscard]] bool NeedsHistoryReset(EHistoryResetReason Reason) const
		{
			return (HistoryResetFlags & HistoryResetBit(Reason)) != 0;
		}

		void RequestHistoryReset(EHistoryResetReason Reason)
		{
			HistoryResetFlags |= HistoryResetBit(Reason);
		}
		void ClearHistoryResetRequest()
		{
			HistoryResetFlags = 0;
		}

		[[nodiscard]] std::string DescribeHistoryReset() const
		{
			if (!NeedsHistoryReset())
				return "none";

			std::string Description;
			for (uint32_t Index = 1; Index < uint32_t(EHistoryResetReason::Count); ++Index)
			{
				const auto Reason = EHistoryResetReason(Index);
				if (!NeedsHistoryReset(Reason))
					continue;

				if (!Description.empty())
					Description += ", ";

				Description += ToString(Reason);
			}

			return Description;
		}
	};
} // namespace Prism
