#pragma once

// Framework types: per-frame and per-view data handed to every feature.
//
// A feature that keeps temporal history must key that history by viewId and must clear it when any
// of the reset reasons below applies. The host sets the flags; the feature decides what it can keep.

#include "Types.h"

#include <string>

namespace renderlab
{
    enum class HistoryResetReason : uint32_t
    {
        None = 0,
        FirstFrame,         // 第一帧，没有历史
        CameraCut,          // 相机不连续（切换视角、瞬移、场景加载完成）
        ResolutionChange,   // 渲染分辨率或输出分辨率改变
        SettingsChange,     // 影响算法的参数改变
        SceneChange,        // 场景内容改变
        ResourceRecreated,  // 历史资源被重建（例如设备丢失、显存紧张）
        Manual,             // 实验代码主动请求
        Count
    };

    inline const char* ToString(HistoryResetReason reason)
    {
        switch (reason)
        {
        case HistoryResetReason::None:              return "none";
        case HistoryResetReason::FirstFrame:        return "first frame";
        case HistoryResetReason::CameraCut:         return "camera cut";
        case HistoryResetReason::ResolutionChange:  return "resolution change";
        case HistoryResetReason::SettingsChange:    return "settings change";
        case HistoryResetReason::SceneChange:       return "scene change";
        case HistoryResetReason::ResourceRecreated: return "resource recreated";
        case HistoryResetReason::Manual:            return "manual";
        default:                                    return "unknown";
        }
    }

    // Single bit per reason, shifted past the None entry.
    constexpr uint32_t HistoryResetBit(HistoryResetReason reason)
    {
        return (reason == HistoryResetReason::None || reason == HistoryResetReason::Count)
            ? 0u
            : (1u << (uint32_t(reason) - 1u));
    }

    struct FrameInfo
    {
        uint64_t frameIndex = 0;
        float deltaTimeSeconds = 0.f;
        float timeSeconds = 0.f;

        ViewId viewId = kPrimaryViewId;

        // 场景数据的渲染分辨率；输出（交换链）分辨率可以不同，例如时域上采样。
        Extent2D renderSize;
        Extent2D outputSize;

        // 亚像素抖动，单位是渲染目标像素，作用于裁剪空间 xy。
        dm::float2 jitter = dm::float2(0.f);
        dm::float2 previousJitter = dm::float2(0.f);

        uint32_t historyResetFlags = 0;

        [[nodiscard]] bool IsFirstFrame() const { return frameIndex == 0; }
        [[nodiscard]] bool NeedsHistoryReset() const { return historyResetFlags != 0; }
        [[nodiscard]] bool NeedsHistoryReset(HistoryResetReason reason) const
        {
            return (historyResetFlags & HistoryResetBit(reason)) != 0;
        }

        void RequestHistoryReset(HistoryResetReason reason) { historyResetFlags |= HistoryResetBit(reason); }
        void ClearHistoryResetRequest() { historyResetFlags = 0; }

        [[nodiscard]] std::string DescribeHistoryReset() const
        {
            if (!NeedsHistoryReset())
                return "none";

            std::string description;
            for (uint32_t index = 1; index < uint32_t(HistoryResetReason::Count); ++index)
            {
                const auto reason = HistoryResetReason(index);
                if (!NeedsHistoryReset(reason))
                    continue;

                if (!description.empty())
                    description += ", ";

                description += ToString(reason);
            }

            return description;
        }
    };
}
