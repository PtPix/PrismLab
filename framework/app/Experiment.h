#pragma once
#include "ExperimentContext.h"
#include "ExperimentFrame.h"
#include <memory>
namespace Prism::Host
{
	class IExperiment
	{
	  public:
		virtual ~IExperiment() = default;

		[[nodiscard]] virtual const char* GetName() const = 0;
		[[nodiscard]] virtual const char* GetDescription() const
		{
			return "";
		}

		// 创建 shader、管线、绑定与自己的渲染目标（通过 context.gpu.targets 声明）。
		virtual FStatus Initialize(FExperimentContext& Context) = 0;

		// 每帧开始、命令列表尚未打开时调用（frame.commands 为 nullptr）。
		// 读回、数值验证和 CPU 侧准备工作放这里：此时可以安全地等待 GPU 空闲。
		virtual void BeginFrame(FExperimentContext& Context, const FExperimentFrame& Frame)
		{
			(void)Context;
			(void)Frame;
		}

		// 记录本帧的 GPU 工作，返回要显示到交换链的纹理；返回 nullptr 表示本帧不显示。
		// 不要在实验里提交命令列表或等待设备空闲：命令列表由宿主管理。
		virtual nvrhi::ITexture* Render(FExperimentContext& Context, const FExperimentFrame& Frame) = 0;

		// ImGui 面板：只画实验自己的参数，不重复宿主的相机/场景/计时信息。
		virtual void BuildUI(FExperimentContext& Context)
		{
			(void)Context;
		}

		// 键盘输入：默认不消费（相机优先）；返回 true 表示实验消费了该按键。
		virtual bool OnKey(FExperimentContext& Context, int Key, int Action, int Mods)
		{
			(void)Context;
			(void)Key;
			(void)Action;
			(void)Mods;
			return false;
		}

		// 渲染分辨率或输出分辨率变化：需要按尺寸重建的资源在这里处理。
		virtual void OnResize(FExperimentContext& Context, const FExtent2D& RenderSize, const FExtent2D& OutputSize)
		{
			(void)Context;
			(void)RenderSize;
			(void)OutputSize;
		}

		// Called once after any Initialize attempt, including partial failure, while the device is alive.
		virtual void Shutdown(FExperimentContext& Context)
		{
			(void)Context;
		}
	};

	std::unique_ptr<IExperiment> CreateExperiment();
} // namespace Prism::Host
