#pragma once

// Host 层：显示链的接缝（接口与调用点，不含实现）。
//
// 显示链负责"线性场景颜色 → 交换链可显示像素"这一段：曝光、HDR 滤波（Bloom 等）、显示变换
// （Tone Mapping）、输出编码。它被所有 feature 共用，所以接口放在宿主这一层，而不是某个实验里。
//
// 实现由使用者提供（路线图方向 11）：
//   * 实现 IDisplayChain，在 Lab::Initialize 里创建并把指针写进 LabContext::displayChain；
//   * 宿主每帧在实验输出之后调用 Record，把结果直接写进 DisplayInput::outputTarget；
//   * 未注册实现、或实现返回错误时，宿主退回"直接把实验输出 blit 到交换链"并记录日志。
//
// 约定（路线图 §7.1 与 §11）：显示链只接受 contracts/ColorSpace.h 里标为 SceneLinear 或
// PreExposed 的输入；DisplayEncoded 的输出不得再当作光照信号使用。

#include <renderlab/contracts/ColorSpace.h>
#include <renderlab/contracts/Status.h>
#include <renderlab/contracts/Types.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>

namespace renderlab::host
{
    struct LabContext;   // 定义在 host/Lab.h；实现方需要包含它

    struct DisplayInput
    {
        // 实验本帧的输出（未做显示变换）
        nvrhi::ITexture* sceneColor = nullptr;
        renderlab::ColorSpace colorSpace = renderlab::ColorSpace::SceneLinear;

        // 交换链 framebuffer：显示链把最终像素直接写进这里
        nvrhi::IFramebuffer* outputTarget = nullptr;
        Extent2D outputSize;

        float deltaTimeSeconds = 0.f;
        uint64_t frameIndex = 0;
    };

    class IDisplayChain
    {
    public:
        virtual ~IDisplayChain() = default;

        // 创建内部资源（曝光纹理、Bloom 金字塔、LUT 等）。输入纹理尚未存在，不要在这里绑定它。
        virtual Status Initialize(LabContext& context) = 0;

        // 记录本帧的显示工作；返回错误会退回宿主的直接 blit，并记录一条 error 日志。
        virtual Status Record(LabContext& context, nvrhi::ICommandList* commands, const DisplayInput& input) = 0;

        // 显示相关的参数（曝光、Bloom 强度、曲线选择）在宿主的实验面板里显示。
        virtual void BuildUI(LabContext& context) = 0;

        // 输出分辨率变化：重建按输出尺寸计算的资源。
        virtual void OnOutputResized(LabContext& context, const Extent2D& outputSize) = 0;
    };
}
