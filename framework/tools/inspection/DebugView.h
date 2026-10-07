#pragma once

// NVRHI 层：公共调试视图。
//
// 任何 feature 都可以把自己声明的中间纹理交给宿主显示；这里提供统一的显示 Pass：
// 通道选择、缩放偏移、伪彩。这样新增功能不再各写一个调试 shader，也不需要为了看中间结果
// 专门做一个实验。
//
// 调试输出已经是显示空间的数据（不是线性辐射），显示链应按 EColorSpace::DisplayEncoded 处理。

#include <framework/render/passes/RasterPass.h>

#include <framework/core/Types.h>

#include <nvrhi/nvrhi.h>

#include <cstdint>

namespace Prism::Gpu
{
	enum class EDebugViewMode : uint32_t
	{
		RGB = 0,
		R,
		G,
		B,
		A,
		Luminance,
		FalseColor,

		// 1 - R：给 forward-Z 设备深度这类"远平面接近 1"的数据用，否则整幅图都接近白色
		OneMinusR,
		Count
	};

	struct FDebugViewSettings
	{
		EDebugViewMode Mode = EDebugViewMode::RGB;
		float Scale = 1.f;
		float Bias = 0.f;
	};

	inline const char* ToString(EDebugViewMode Mode)
	{
		switch (Mode)
		{
			case EDebugViewMode::RGB:
				return "RGB";
			case EDebugViewMode::R:
				return "R";
			case EDebugViewMode::G:
				return "G";
			case EDebugViewMode::B:
				return "B";
			case EDebugViewMode::A:
				return "A";
			case EDebugViewMode::Luminance:
				return "Luminance";
			case EDebugViewMode::FalseColor:
				return "False color";
			case EDebugViewMode::OneMinusR:
				return "1 - R";
			default:
				return "unknown";
		}
	}

	// 供 ImGui 组合框使用的名字表（长度 = DebugViewMode::Count）。
	inline const char* const* GetDebugViewModeNames()
	{
		static const char* const Names[] = {"RGB", "R", "G", "B", "A", "Luminance", "False color", "1 - R"};
		return Names;
	}

	class FDebugViewPass
	{
	  public:
		// 加载框架自带的 shader（prism/DebugView.hlsl 的 main_vs / main_ps）。
		bool Initialize(nvrhi::IDevice* Device, FShaderLibrary& Shaders);

		// 把 source 画到 target。source 或 target 变化时内部重建绑定集与管线对象。
		bool Render(nvrhi::ICommandList* Commands, nvrhi::ITexture* Source, nvrhi::IFramebuffer* Target,
					const FDebugViewSettings& Settings);

		[[nodiscard]] bool IsValid() const
		{
			return bReady;
		}

	  private:
		FRasterPass Pass;
		FPassConstants Constants;
		nvrhi::SamplerHandle PointClampSampler;
		nvrhi::BindingLayoutHandle Layout;
		bool bReady = false;
	};
} // namespace Prism::Gpu
