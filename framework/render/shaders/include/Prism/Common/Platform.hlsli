#ifndef PRISM_PLATFORM_HLSLI
#define PRISM_PLATFORM_HLSLI

// Prism platform conventions; see framework/render/data/Conventions.h.
// 每个实验 shader 的第一行都应该包含本文件。

// 1) 矩阵打包为行主序，与 Donut 的 shader 以及 CPU 侧的 dm::float4x4 内存布局一致。
//    有了这一行，HLSL 的 mul(v, M) 与 CPU 的 v * M 完全等价。
//    漏掉它的症状非常有迷惑性：深度相关的计算全部正确、依赖矩阵的位置计算全错
//    （ContractExperiment 的自检覆盖了这一点）。
#pragma pack_matrix(row_major)

// 2) 深度：forward-Z，近平面 0、远平面 1，清空值 1.0，比较函数 Less。
#define PRISM_DEPTH_CONVENTION_FORWARD_Z 0

// 3) 颜色：线性 HDR，shader 里不做隐式 gamma；显示变换由专门的 pass 负责。

// 4) 单位：世界空间为米、右手系、Y 轴向上。

// 5) 可见性：0 = 完全遮挡，1 = 完全可见，并且总是与某个光源/采样绑定。

#endif // PRISM_PLATFORM_HLSLI
