// 公共调试视图的常量布局：C++ 与 HLSL 同时包含。
//
// 调试视图只做"通道选择 + 缩放偏移 + 伪彩"，不做任何解码或物理含义推断：
// 语义由发布纹理的 feature 在其文档与 UI 标签里说明。

#ifndef PRISM_DEBUG_VIEW_CB_H
#define PRISM_DEBUG_VIEW_CB_H

struct FDebugViewConstants
{
	float2 InverseSize;
	int Mode; // 0 RGB, 1 R, 2 G, 3 B, 4 A, 5 亮度, 6 伪彩, 7 = 1 - R
	int Reserved0;
	float Scale;
	float Bias;
	float2 Reserved1;
};

#endif // PRISM_DEBUG_VIEW_CB_H
