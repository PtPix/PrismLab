#pragma once

// Host layer: the experiment interface.
//
// This is the whole contract a lab has with the host. A lab implements algorithms, shaders and its
// own UI section; the host owns the window, device, swap chain, camera, scene, config, ImGui, GPU
// timing and the frame loop. Adding an experiment means adding a Lab subclass and a factory.

#include <backends/nvrhi/common/BufferPool.h>
#include <backends/nvrhi/common/GpuProfiler.h>
#include <backends/nvrhi/common/RenderTargetPool.h>
#include <backends/nvrhi/common/ResourceTable.h>
#include <backends/nvrhi/common/ShaderLibrary.h>

#include <adapters/donut/HostConfig.h>
#include <adapters/donut/SceneData.h>

#include <pipelines/SceneForwardPipeline.h>

#include "DebugViewRegistry.h"
#include "DisplayChain.h"
#include "Metrics.h"
#include "Params.h"
#include "TemporalServices.h"

#include <renderlab/contracts/CameraData.h>
#include <renderlab/contracts/ColorSpace.h>
#include <renderlab/contracts/FrameInfo.h>
#include <renderlab/contracts/Status.h>
#include <renderlab/contracts/Types.h>

#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace renderlab::host
{
    // 宿主提供给实验的回调：影响宿主行为，而不是让实验直接操作宿主内部状态。
    struct LabCallbacks
    {
        // 请求清空时域历史（例如参数变化后）
        std::function<void(renderlab::HistoryResetReason)> requestHistoryReset;

        // 保存任意纹理（内部会等待 GPU 空闲；只用于截图/验证，不要每帧调用）
        std::function<bool(nvrhi::ITexture*, const std::filesystem::path&, nvrhi::ResourceStates)> saveTexture;

        // 实验主动结束（例如自动跑完一组对比后退出）
        std::function<void()> requestQuit;
    };

    // 实验初始化后一直有效的服务；实验不拥有其中任何对象。
    struct LabContext
    {
        nvrhi::IDevice* device = nullptr;
        donut::engine::ShaderFactory* shaderFactory = nullptr;
        donut::engine::CommonRenderPasses* commonPasses = nullptr;

        gpu::ShaderLibrary* shaders = nullptr;
        gpu::RenderTargetPool* targets = nullptr;
        gpu::BufferPool* buffers = nullptr;

        // 资源表：纹理与缓冲的统一入口，实验用自己声明的槽位获取（见 ResourceTable.h）。
        gpu::ResourceTable* resources = nullptr;

        gpu::GpuProfiler* profiler = nullptr;

        pipeline::SceneForwardPipeline* scenePipeline = nullptr;
        const adapter::SceneData* scene = nullptr;
        const adapter::HostConfig* config = nullptr;

        // 显示链接缝（见 host/DisplayChain.h）。为空时宿主直接把实验输出 blit 到交换链，
        // 表现为"线性 HDR 未做显示变换"；实现由使用者提供。
        IDisplayChain* displayChain = nullptr;

        // 时域服务接缝（见 host/TemporalServices.h）。为空表示没有历史/采样序列基础设施，
        // 需要它的 feature 应当明确报错，而不是自己临时造一套。实现由使用者提供。
        ITemporalServices* temporal = nullptr;

        // 实验本帧输出的颜色空间：显示链据此判断输入是否可以直接显示。
        renderlab::ColorSpace outputColorSpace = renderlab::ColorSpace::SceneLinear;

        // 调试视图登记（见 host/DebugViewRegistry.h）：每帧把想看的中间纹理发布进来，
        // 宿主面板里可切换显示。发布顺序必须每帧稳定。
        DebugViewRegistry* debugViews = nullptr;

        // 指标上报（见 host/Metrics.h）：实验把数值（能量、NaN 计数、复用率）报给宿主，
        // 由宿主统一进面板与 CSV。
        Metrics* metrics = nullptr;

        // 仓库根下的资产目录（可能不存在）：实验读取数据文件时使用。
        std::filesystem::path assetsDirectory;

        dm::float3 ambientTop = dm::float3(0.f);
        dm::float3 ambientBottom = dm::float3(0.f);

        LabCallbacks callbacks;

        // 时域算法需要抖动时设置采样数（例如 8）；0 表示不抖动。
        // 抖动序列由宿主生成（Halton 2,3），并通过 LabFrame::frame 传给实验。
        uint32_t jitterSampleCount = 0;
    };

    // 每帧数据：全部来自契约层，实验不直接依赖 Donut 相机或宿主内部状态。
    struct LabFrame
    {
        nvrhi::ICommandList* commands = nullptr;

        renderlab::FrameInfo frame;
        renderlab::CameraData camera;

        donut::engine::IView* view = nullptr;
        donut::engine::IView* previousView = nullptr;

        Extent2D renderSize;
        Extent2D outputSize;
    };

    class Lab
    {
    public:
        virtual ~Lab() = default;

        [[nodiscard]] virtual const char* GetName() const = 0;
        [[nodiscard]] virtual const char* GetDescription() const { return ""; }

        // 创建 shader、管线、绑定与自己的渲染目标（通过 context.targets 声明）。
        virtual Status Initialize(LabContext& context) = 0;

        // 每帧开始、命令列表尚未打开时调用（frame.commands 为 nullptr）。
        // 读回、数值验证和 CPU 侧准备工作放这里：此时可以安全地等待 GPU 空闲。
        virtual void BeginFrame(LabContext& context, const LabFrame& frame)
        {
            (void)context;
            (void)frame;
        }

        // 记录本帧的 GPU 工作，返回要显示到交换链的纹理；返回 nullptr 表示本帧不显示。
        // 不要在实验里提交命令列表或等待设备空闲：命令列表由宿主管理。
        virtual nvrhi::ITexture* Render(LabContext& context, const LabFrame& frame) = 0;

        // ImGui 面板：只画实验自己的参数，不重复宿主的相机/场景/计时信息。
        virtual void BuildUI(LabContext& context) { (void)context; }

        // 键盘输入：默认不消费（相机优先）；返回 true 表示实验消费了该按键。
        virtual bool OnKey(LabContext& context, int key, int action, int mods)
        {
            (void)context;
            (void)key;
            (void)action;
            (void)mods;
            return false;
        }

        // 渲染分辨率或输出分辨率变化：需要按尺寸重建的资源在这里处理。
        virtual void OnResize(LabContext& context, const Extent2D& renderSize, const Extent2D& outputSize)
        {
            (void)context;
            (void)renderSize;
            (void)outputSize;
        }

        // 自检结果：冒烟测试/CI 用它决定进程退出码（例如契约自检失败时返回 false）。
        virtual bool PassedVerification() const { return true; }

        virtual void Shutdown(LabContext& context) { (void)context; }
    };

    std::unique_ptr<Lab> CreateLab();
}
