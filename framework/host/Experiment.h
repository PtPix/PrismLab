#pragma once

// Host layer: the experiment interface.
//
// This is the whole contract an experiment has with the host. An experiment implements algorithms, shaders and its
// own UI section; the host owns the window, device, swap chain, camera, scene, config, ImGui, GPU
// timing and the frame loop. Adding an experiment means adding an Experiment subclass and a factory.

#include <framework/nvrhi/BufferPool.h>
#include <framework/nvrhi/GpuProfiler.h>
#include <framework/nvrhi/RenderTargetPool.h>
#include <framework/nvrhi/ResourceTable.h>
#include <framework/nvrhi/ShaderLibrary.h>

#include <framework/donut/HostConfig.h>
#include <framework/donut/SceneData.h>

#include <framework/pipelines/SceneForwardPipeline.h>

#include "DebugViewRegistry.h"
#include "DisplayChain.h"
#include "Metrics.h"
#include "Params.h"
#include "TemporalServices.h"

#include <framework/types/CameraData.h>
#include <framework/types/ColorSpace.h>
#include <framework/types/FrameInfo.h>
#include <framework/types/Status.h>
#include <framework/types/Types.h>

#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/View.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace prism::host
{
    // 宿主提供给实验的回调：影响宿主行为，而不是让实验直接操作宿主内部状态。
    struct ExperimentCallbacks
    {
        // 请求清空时域历史（例如参数变化后）
        std::function<void(prism::HistoryResetReason)> requestHistoryReset;

        // 保存任意纹理（内部会等待 GPU 空闲；只用于截图/验证，不要每帧调用）
        std::function<bool(nvrhi::ITexture*, const std::filesystem::path&, nvrhi::ResourceStates)> saveTexture;

        // 实验主动结束（例如自动跑完一组对比后退出）
        std::function<void()> requestQuit;
    };

    // 实验初始化后一直有效的服务；实验不拥有其中任何对象。
    // 按用途分成三组：context.gpu / context.scene.data / context.output。
    struct ExperimentContext
    {
        // 设备与执行设施：创建 shader、PSO、资源都从这里取。
        struct GpuServices
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
        };

        // 共享场景与光照环境。
        struct SceneServices
        {
            pipeline::SceneForwardPipeline* pipeline = nullptr;
            const adapter::SceneData* data = nullptr;

            dm::float3 ambientTop = dm::float3(0.f);
            dm::float3 ambientBottom = dm::float3(0.f);
        };

        // 输出、诊断与回调。
        struct OutputServices
        {
            // 显示链接缝（见 host/DisplayChain.h）。为空时宿主直接把实验输出 blit 到交换链，
            // 表现为"线性 HDR 未做显示变换"；实现由使用者提供。
            IDisplayChain* displayChain = nullptr;

            // 时域服务接缝（见 host/TemporalServices.h）。为空表示没有历史/采样序列基础设施，
            // 需要它的 feature 应当明确报错，而不是自己临时造一套。实现由使用者提供。
            ITemporalServices* temporal = nullptr;

            // 实验本帧输出的颜色空间：显示链据此判断输入是否可以直接显示。
            prism::ColorSpace colorSpace = prism::ColorSpace::SceneLinear;

            // 调试视图登记（见 host/DebugViewRegistry.h）：每帧把想看的中间纹理发布进来，
            // 宿主面板里可切换显示。发布顺序必须每帧稳定。
            DebugViewRegistry* debugViews = nullptr;

            // 指标上报（见 host/Metrics.h）：实验把数值（能量、NaN 计数、复用率）报给宿主，
            // 由宿主统一进面板与 CSV。
            Metrics* metrics = nullptr;

            ExperimentCallbacks callbacks;

            // 时域算法需要抖动时设置采样数（例如 8）；0 表示不抖动。
            // 抖动序列由宿主生成（Halton 2,3），并通过 ExperimentFrame::frame 传给实验。
            uint32_t jitterSampleCount = 0;
        };

        GpuServices gpu;
        SceneServices scene;
        OutputServices output;

        // 宿主配置与仓库根下的资产目录（可能不存在）。
        const adapter::HostConfig* config = nullptr;
        std::filesystem::path assetsDirectory;
    };

    // 每帧数据：FrameInfo/CameraData 来自 framework/types；view/previousView 是 Donut 视图对象，
    // 供实验驱动共享场景管线（C++ 侧本来就由 Donut 管理，直接用它们即可）。
    struct ExperimentFrame
    {
        nvrhi::ICommandList* commands = nullptr;

        prism::FrameInfo frame;
        prism::CameraData camera;

        donut::engine::IView* view = nullptr;
        donut::engine::IView* previousView = nullptr;

        Extent2D renderSize;
        Extent2D outputSize;
    };

    class Experiment
    {
    public:
        virtual ~Experiment() = default;

        [[nodiscard]] virtual const char* GetName() const = 0;
        [[nodiscard]] virtual const char* GetDescription() const { return ""; }

        // 创建 shader、管线、绑定与自己的渲染目标（通过 context.gpu.targets 声明）。
        virtual Status Initialize(ExperimentContext& context) = 0;

        // 每帧开始、命令列表尚未打开时调用（frame.commands 为 nullptr）。
        // 读回、数值验证和 CPU 侧准备工作放这里：此时可以安全地等待 GPU 空闲。
        virtual void BeginFrame(ExperimentContext& context, const ExperimentFrame& frame)
        {
            (void)context;
            (void)frame;
        }

        // 记录本帧的 GPU 工作，返回要显示到交换链的纹理；返回 nullptr 表示本帧不显示。
        // 不要在实验里提交命令列表或等待设备空闲：命令列表由宿主管理。
        virtual nvrhi::ITexture* Render(ExperimentContext& context, const ExperimentFrame& frame) = 0;

        // ImGui 面板：只画实验自己的参数，不重复宿主的相机/场景/计时信息。
        virtual void BuildUI(ExperimentContext& context) { (void)context; }

        // 键盘输入：默认不消费（相机优先）；返回 true 表示实验消费了该按键。
        virtual bool OnKey(ExperimentContext& context, int key, int action, int mods)
        {
            (void)context;
            (void)key;
            (void)action;
            (void)mods;
            return false;
        }

        // 渲染分辨率或输出分辨率变化：需要按尺寸重建的资源在这里处理。
        virtual void OnResize(ExperimentContext& context, const Extent2D& renderSize, const Extent2D& outputSize)
        {
            (void)context;
            (void)renderSize;
            (void)outputSize;
        }

        // 自检结果：冒烟测试/CI 用它决定进程退出码（例如契约自检失败时返回 false）。
        virtual bool PassedVerification() const { return true; }

        virtual void Shutdown(ExperimentContext& context) { (void)context; }
    };

    std::unique_ptr<Experiment> CreateExperiment();
}
