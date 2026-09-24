#ifndef PRISM_SHADOWS_PCF_HLSLI
#define PRISM_SHADOWS_PCF_HLSLI

// 阴影算法核心（M2 的第一块），不依赖任何具体 PSO 或宿主的资源布局。
//
// 与宿主的耦合通过下面这组桥接函数完成：换阴影图布局、换滤波核或换宿主时只重写桥接层，
// 比较与滤波逻辑保持不变。使用本文件的 shader 必须实现它们：
//
//   float PRISM_LoadShadowDepth(int2 texelCoord)         阴影图设备深度（光源裁剪空间）
//   float PRISM_ShadowDepthFromWorld(float3 worldPos)    世界位置在阴影图中的设备深度
//   bool  PRISM_IsInsideShadowMap(int2 texelCoord)       纹素是否落在阴影图覆盖范围内
//
// 约定：
//   * 可见性 0 = 完全遮挡，1 = 完全可见；
//   * 阴影图之外的可见性由 ShadowSettings::outsideShadowMapVisibility 决定，必须显式写出，
//     不能把"图外"当成"无遮挡"或"完全遮挡"而不说明；
//   * depthBiasNdc 是比较深度上的偏移，与光栅阶段的 depth/slope bias 分开配置（两者都要有）。
//
// PCSS 的半影需要"遮挡物距离（米）"与"光源尺寸"，首版没有提供米制换算的桥接函数，
// 因此这里不给出半成品实现：M2 补齐 PRISM_ShadowDepthInMeters(...) 之后再添加。

#include "framework/render/shaders/include/Prism/Common/Platform.hlsli"

float HardShadowVisibility(float3 worldPosition, int2 texelCoord, float depthBiasNdc)
{
    const float shadowDepth = PRISM_LoadShadowDepth(texelCoord);
    const float receiverDepth = PRISM_ShadowDepthFromWorld(worldPosition);

    return (receiverDepth - depthBiasNdc <= shadowDepth) ? 1.f : 0.f;
}

// PCF：在纹素空间做方阵平均。radiusTexels 由调用方按纹素与世界尺寸的换算给出，
// 算法核心不假设阴影图分辨率或投影类型。
float PcfShadowVisibility(
    float3 worldPosition,
    int2 texelCoord,
    float depthBiasNdc,
    float radiusTexels,
    int sampleCount)
{
    if (sampleCount <= 1 || radiusTexels <= 0.f)
        return HardShadowVisibility(worldPosition, texelCoord, depthBiasNdc);

    const float receiverDepth = PRISM_ShadowDepthFromWorld(worldPosition) - depthBiasNdc;

    const int side = max(1, int(sqrt(float(sampleCount))));
    const float step = (side > 1) ? (2.f * radiusTexels / float(side - 1)) : 0.f;

    float visible = 0.f;
    float taken = 0.f;

    [loop] for (int y = 0; y < side; ++y)
    {
        [loop] for (int x = 0; x < side; ++x)
        {
            const int2 sampleTexel = texelCoord + int2(
                int(-radiusTexels + float(x) * step + 0.5f),
                int(-radiusTexels + float(y) * step + 0.5f));

            // 图外的样本不参与平均：它们的可见性由调用方按契约处理。
            if (!PRISM_IsInsideShadowMap(sampleTexel))
                continue;

            visible += (receiverDepth <= PRISM_LoadShadowDepth(sampleTexel)) ? 1.f : 0.f;
            taken += 1.f;
        }
    }

    return (taken > 0.f) ? (visible / taken) : 1.f;
}

#endif // PRISM_SHADOWS_PCF_HLSLI
