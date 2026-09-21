#pragma once

// Host 层：时域服务的接缝（接口与调用点，不含实现）。
//
// 时域基础设施（历史纹理的分配与交换、采样序列、像素种子、重置规则）由使用者实现
// （路线图 §3.2 帧与视图信息、§5.5 时域功能的额外接口、方向 06）。宿主只定义接缝：
//
//   * Lab::Initialize 里创建实现并把指针写进 LabContext::temporal；
//   * feature 通过 LabContext::temporal 取历史与种子，不自己发明一套；
//   * 宿主在渲染分辨率变化时调用 OnRenderSizeChanged，让实现丢弃按分辨率分配的历史。
//
// 约定：
//   1. 一份历史的身份是 (owner, viewId)。owner 由 feature 自己命名（"GI.Indirect"、"Shadow.History"、
//      "TAA.Color"），viewId 来自 contracts/Types.h；多视图历史必须分开。
//   2. 同一帧内"读取的历史"与"写入的历史"必须是不同纹理（ping-pong 或帧槽），
//      避免 GPU 多帧在途时的读写覆盖。
//   3. PixelSeed 只能依赖 (frameIndex, pixel, stream)：回归测试要求固定种子可复现；
//      统计误差时换 stream 而不是换随机源实现。
//   4. 历史失效由 FrameInfo 的重置原因驱动（首帧、相机跳变、分辨率变化、设置变化、场景切换、
//      资源重建、实验主动请求）。feature 用 WasResetThisFrame 决定首帧行为，而不是自己判断帧号。

#include <renderlab/contracts/FrameInfo.h>
#include <renderlab/contracts/Status.h>
#include <renderlab/contracts/Types.h>

#include <cstdint>

namespace renderlab::host
{
    struct LabContext;   // 定义在 host/Lab.h；实现方需要包含它

    class ITemporalServices
    {
    public:
        virtual ~ITemporalServices() = default;

        // 创建历史资源与内部状态。
        virtual Status Initialize(LabContext& context) = 0;

        // 丢弃某个 owner 的历史（例如实验改了滤波参数、或某个光源被移除）。
        virtual void ResetHistory(const char* owner, renderlab::ViewId viewId, renderlab::HistoryResetReason reason) = 0;

        // 本帧该 owner 的历史是否在开始时被判定为无效（feature 据此走首帧路径）。
        virtual bool WasResetThisFrame(const char* owner, renderlab::ViewId viewId) const = 0;

        // 本帧应使用的采样索引；sampleCount 由 feature 决定（例如 8 或 16 个样本一个周期）。
        virtual uint32_t SampleIndex(renderlab::ViewId viewId, uint32_t sampleCount) = 0;

        // 稳定像素种子：同一 (frame, pixel, stream) 必须给出相同结果。
        virtual uint32_t PixelSeed(dm::uint2 pixel, uint32_t stream) const = 0;

        // 渲染分辨率变化：释放按分辨率分配的历史（例如与屏幕同尺寸的历史纹理）。
        virtual void OnRenderSizeChanged(const Extent2D& renderSize) = 0;

        // 时域相关的调试信息（历史有效性、样本索引、复用的采样数）在宿主面板里显示。
        virtual void BuildUI(LabContext& context) = 0;
    };
}
