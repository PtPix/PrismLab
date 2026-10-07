# PrismLab 渲染架构与图形算法学习路线

本文是 PrismLab 后续 `algorithms/` 与 `samples/` 建设的长期执行计划。目标不是收集彼此孤立的
Shader，而是从基础渲染管线出发，逐步建立可以验证、比较、组合和优化的实时渲染算法库，并用这些
实现理解 Unreal Engine 的渲染架构、工程约束和特定优化。

本文对照 Unreal Engine 5.8 和当前公开的 Direct3D 12 文档，最后核对日期为 2026-10-07。
UE 的具体实现会继续演进，因此 Prism 的模块边界以稳定的图形学输入输出为准，UE 类型、文件和
Pass 名称只作为源码阅读入口，不成为 Prism 算法接口的一部分。

## 1. 项目目标

PrismLab 同时服务四个目标：

1. **理解基础原理。** 每种算法先完成可解释的基线，再进行工程优化。
2. **理解 UE 架构。** 用自己的实现理解 RDG、Mesh Drawing Pipeline、GPU Scene、Nanite、
   Virtual Shadow Maps、Lumen、TSR、Substrate 等系统为什么这样组织。
3. **建立可组合算法库。** 阴影、材质、直接光、间接光、反射、时域重建和显示变换通过明确契约连接，
   而不是绑定到某一个 Sample。
4. **容纳 UE 之外的研究。** ReSTIR、DDGI、Path Guiding、Wavefront Path Tracing、Gaussian
   Splatting、神经缓存和谱渲染等方向应能接入同一实验基础设施。

这个项目不以完整复刻 UE 为目标。UE 是用于观察成熟工程取舍的参照系；Prism 应保留算法可见性，
让资源、数据布局、调度和误差来源可以被逐步检查。

## 2. 当前基础与缺口

现有框架已经提供：

- 窗口、D3D12 设备和 NVRHI 命令录制；
- Compute、Raster、Fullscreen Pass 封装；
- Shader 编译、反射、热重载和事务回滚；
- 稳定资源 ID、Texture/Buffer Cache 和资源表；
- GPU 计时；
- Comparison、Replay、Capture、Metrics 和 Debug View；
- CPU/GPU 场景借用接口；
- Surface、Temporal、Display 的公共契约；
- 程序场景、glTF 场景和 Donut Forward 参考路径。

当前主要缺口是算法层：

- `algorithms/shadows` 只有设置和 PCF 核心，没有完整阴影渲染调度；
- 没有 Prism 自己生成的 Depth、GBuffer 和 Motion Vector；
- 没有 Prism 自己的材质求值、直接光、IBL、曝光和 Tone Mapping；
- 没有 Hi-Z、时域历史消费者、光追参考或 GPU Driven 路径；
- `PrismForward` 仍主要用于观察旧 Donut Forward 结果。

因此近期工作重点应是填充 Algorithm 和 Sample，不继续扩大通用框架。只有当至少两个真实算法出现
相同的资源或调度问题时，才把解决方案下沉到 `framework/`。

## 3. 总体依赖链

```text
Scene / Geometry
      |
      v
Visibility / Geometry Processing
      |
      v
Depth + GBuffer + Motion + IDs
      |                    \
      |                     -> Screen-space and temporal inputs
      v
Shadows / AO / Reflections / GI
      |
      v
Material evaluation + Direct/Indirect lighting
      |
      v
Temporal reconstruction / Denoising / Upscaling
      |
      v
Exposure / Tone mapping / Output transform

Path tracer ---------------------> reference for lighting and visibility
```

依赖顺序决定实施顺序。没有稳定 Depth 和 Motion 时，不应先实现复杂 TAA；没有可信 BSDF 和参考图时，
不应先判断 GI 是否正确；没有 Hi-Z 和 GPU 场景时，不应直接尝试 Nanite 风格管线。

## 4. 所有模块遵守的边界

### 4.1 Framework

`framework/` 只放执行机制和稳定契约：资源、Pass、Shader、计时、场景视图、实验宿主和工具。
它不拥有具体 AO、阴影、GI、TAA 或 Tone Mapping 实现。

### 4.2 Algorithm

`algorithms/<family>` 拥有一项技术的：

- Settings；
- Inputs 和 Outputs；
- CPU/GPU 数据结构；
- Shader；
- C++ Pass 调度；
- 内部持久资源和 History；
- Debug 数据；
- 数值测试和 GPU Contract 测试。

Algorithm 可以使用 NVRHI 和 `Prism::Gpu::FRenderServices`，但不能包含 `ExperimentContext`、ImGui、
窗口、命令行或演示场景。

建议的最小布局为：

```text
algorithms/shadows/
  CMakeLists.txt
  ShadowRenderer.h
  ShadowRenderer.cpp
  ShadowSettings.h
  ShadowInputs.h
  ShadowOutputs.h
  ShadowDebug.h
  README.md
  shaders/
    ShadowDepth.hlsl
    ShadowResolve.hlsl
    PCF.hlsli
    PCSS.hlsli
```

公开接口保持强类型，不通过字符串查找算法资源：

```cpp
namespace Prism::Shadows
{
	struct FShadowInputs
	{
		const FFrameInfo& Frame;
		const FCameraData& Camera;
		const FSceneFrameData& Scene;
		const Gpu::FSceneGpuData& GpuScene;
	};

	struct FShadowOutputs
	{
		nvrhi::ITexture* Visibility = nullptr;
		nvrhi::ITexture* Atlas = nullptr;
	};

	class FShadowRenderer
	{
	  public:
		FStatus Initialize(Gpu::FRenderServices& Services);
		FStatus Record(nvrhi::ICommandList* Commands, const FShadowInputs& Inputs,
					   const FShadowSettings& Settings, FShadowOutputs& Outputs);
	};
}
```

### 4.3 Sample

`samples/<family>` 负责：

- 选择程序场景或资产场景；
- 组合多个 Algorithm；
- 创建展示资源；
- UI、命令行和预设；
- Beauty、Analysis、Compare 三种工作模式；
- 固定镜头、回放和截图；
- 解释技术优势、限制和失败情况。

Sample 不应复制算法内部的 PSO、Binding、History 或 Filter 实现。

### 4.4 Test

- CPU 测试验证数学、数据打包、分页、采样分布和状态迁移；
- GPU Contract 验证矩阵、格式、坐标和少量确定像素；
- 图像回归验证固定场景的稳定输出；
- 性能测试单独运行每种方法，避免 A/B 同帧执行造成资源竞争。

## 5. Prism 固定数据约定

后续算法默认遵守 `framework/render/data/Conventions.h`：

| 语义 | 约定 |
| --- | --- |
| 世界空间 | 右手、Y-up、米 |
| 视空间 | 左手、相机朝 +Z |
| Device Depth | 默认 Forward-Z，近 0、远 1、Clear 1、Less |
| UV | 左上角原点，范围 `[0, 1]` |
| 矩阵 | HLSL 使用行向量 `mul(v, M)`，CPU 矩阵直接上传 |
| 场景颜色 | 显示变换前为 Linear HDR RGB |
| 粗糙度 | 存感知粗糙度 `r`，GGX 内部计算 `alpha = r * r` |
| Visibility | 0 为完全遮挡，1 为完全可见，并绑定一个光源或样本 |
| Motion | `previousUnjitteredUV - currentUnjitteredUV` |

新算法必须在 Inputs/Outputs 中声明以下信息：

- 分辨率和有效矩形；
- 格式和颜色空间；
- Depth Convention；
- Normal Space 和编码；
- Roughness Encoding；
- Motion 单位以及是否含 Jitter；
- 资源归属和有效期；
- History 所属 ViewId；
- 需要响应的 History Reset Reason。

## 6. 一项算法的统一完成条件

每项算法按以下顺序推进，全部完成后才从“实验”标记为“可复用”：

1. **理论说明**：公式、符号、单位、坐标空间和假设。
2. **CPU 或离线基线**：小输入上的可验证结果。
3. **朴素 GPU 实现**：直接对应论文或推导，不提前优化。
4. **中间结果可视化**：采样、权重、Mip、Tile、Reject Mask、Variance 等。
5. **优化实现**：记录每项优化改变了什么瓶颈。
6. **参考结果**：路径追踪、暴力搜索或高采样版本。
7. **失败案例**：薄几何、快速运动、显露、高频材质、小亮光源等。
8. **自动验证**：CPU Test、GPU Contract 或固定图像回归。
9. **性能报告**：GPU 时间、显存、分辨率扩展性和质量误差。
10. **UE 对照**：对应的 UE 模块、关键数据流和不同工程取舍。

算法 README 至少包含：原始论文、实现参考、自己的推导、Pass 图、资源表、复杂度、参数解释、
已知问题和实验结论。

## 7. 阶段 0：实验测量基础

### 目标

让后续结果能够被复现、解释和比较。

### 实施内容

1. 增加 D3D12 能力报告：
   - Shader Model；
   - Wave Ops 和 Wave Lane 范围；
   - 16-bit 类型；
   - DXR Tier；
   - Mesh Shader Tier；
   - Variable Rate Shading Tier；
   - Sampler Feedback Tier；
   - Work Graphs Tier。
2. 为每个 Algorithm Pass 建立稳定的 GPU Marker 和计时名称。
3. 记录每帧 Draw、Dispatch、TraceRays、可见实例、可见 Cluster 和资源字节数。
4. 保存 Run Manifest：
   - Git revision；
   - Build Configuration；
   - GPU、驱动和 Feature Level；
   - Render/Output Size；
   - Sample、场景、镜头和预设；
   - Algorithm Settings；
   - Random Seed；
   - Warm-up 和测量帧区间。
5. 增加通用错误视图：NaN、Inf、负能量、越界法线、无效粗糙度和异常 Motion。

### 验收

- 同一 Replay 和 Preset 可以重现相同逻辑帧；
- CSV 和截图能够追溯到完整配置；
- 缺少硬件能力时显示明确原因，而不是创建失败或黑屏；
- Work Graphs 等实验能力不成为普通 Sample 的强制依赖。

## 8. 阶段 1：Prism 自有 Surface 和基础显示链

### 新模块

```text
algorithms/surface/
algorithms/materials/
algorithms/display/
samples/deferred/
```

### Pass 顺序

1. **Depth Prepass**
   - 静态和动态实例；
   - Alpha Mask 后续单独加入；
   - 验证 Forward-Z Clear 和 Compare。
2. **GBuffer Pass**
   - World Normal + Perceptual Roughness；
   - Linear BaseColor + Metalness；
   - Emissive；
   - Motion Vector；
   - InstanceId 和 MaterialId。
3. **Position Reconstruction**
   - 从 Device Depth 重建 View/World Position；
   - 与 CPU 已知位置比较米制误差。
4. **单方向光 Deferred Lighting**
   - Lambert；
   - GGX；
   - 暂时无阴影。
5. **Display**
   - 固定曝光；
   - 简单 Filmic/ACES Fit；
   - Linear 到 sRGB；
   - 输出前后分离，确保 HDR 误差比较不经过 Tone Mapping。

### 调试视图

- Device/Linear Depth；
- World Position；
- World Normal；
- BaseColor、Roughness、Metalness、Emissive；
- Motion X/Y/Magnitude；
- InstanceId、MaterialId；
- Diffuse、Specular 和最终 Scene Linear Color。

### 验收场景

- 单三角形和单位立方体，用于矩阵和插值检查；
- 粗糙度/金属度二维材质球阵列；
- 运动物体、静止相机；
- 静止物体、运动相机；
- Camera Cut 和 Resize。

### 验收

- CPU/GPU 世界位置误差在 Schema 容差内；
- 静止相机和物体的 Motion 为零；
- Jitter 不污染未抖动 Motion；
- Forward 与 Deferred 使用同一 BRDF 时结果在约定误差内；
- Capture、Comparison 和 Reference 保存的颜色空间明确。

## 9. 阶段 2：ShadowLab

扩展现有 `algorithms/shadows`，不要另建重复模块。

### 2.1 基础 Shadow Map

1. 方向光正交投影；
2. 聚光灯透视投影；
3. 单面和双面 Shadow Caster；
4. Hard Compare；
5. Receiver Frustum 和 Shadow Caster Culling；
6. Shadow Atlas 的最小分配器。

### 2.2 Bias 专项

分别实现并可独立调节：

- Constant Depth Bias；
- Slope-scaled Bias；
- Receiver Bias；
- World-space Normal Bias。

诊断场景应同时出现 Acne、Peter Panning、斜接收面和薄片，避免只在一个普通模型上凭感觉调参。

### 2.3 Filter

1. 2x2 Hardware PCF；
2. 3x3、5x5、7x7 Kernel；
3. Poisson Disk；
4. Blue-noise/Hash Rotation；
5. GatherCmp 与普通采样路径对比；
6. Separable 近似与完整二维 Kernel 对比。

### 2.4 PCSS

分成可观察的三个阶段：

1. Blocker Search；
2. Average Blocker Depth；
3. Penumbra Estimation 和可变半径 PCF。

输出 Blocker Count、Average Blocker Distance 和 Penumbra Radius Debug View。分别验证 Spot Light 的米制
Emitter Radius 与 Directional Light 的 Angular Radius，不能混用两个物理量。

### 2.5 Cascaded Shadow Maps

1. Uniform、Logarithmic 和 Practical Split；
2. Cascade Fit；
3. Texel Snapping；
4. Stable CSM；
5. Cascade Blend；
6. 每 Cascade 独立 Culling；
7. 远距离 Cascade 更新频率实验。

### 2.6 Moment 方法

1. VSM Moment 写入；
2. Separable Blur；
3. Chebyshev Upper Bound；
4. Light Bleeding Reduction；
5. EVSM 正负指数；
6. 浮点范围、精度和溢出诊断；
7. 可选 MSM 研究。

### 2.7 Contact Shadow

- View-space Ray March；
- Thickness 和最大距离；
- 与 Shadow Map 组合；
- 屏幕边界和遮挡缺失可视化。

### 2.8 简化 Virtual Shadow Map

1. 16K 级逻辑虚拟地址空间；
2. Page Table；
3. Physical Page Pool；
4. 由主视图 Depth 产生 Page Request；
5. Request 去重和分配；
6. Page Render List；
7. 跨帧 Cache；
8. Light、Caster 和 Projection 改变后的失效；
9. Directional Light Clipmap；
10. Page Residency、Cache Hit/Miss 和 Invalidated Page 可视化。

UE VSM 当前使用 16K 虚拟阴影图、128x128 Page、按屏幕深度需求分配并跨帧缓存。Prism 的实现应先证明
分页、请求和失效，再研究与 Nanite 风格 Cluster Culling 的结合，避免第一版直接追求 UE 的完整规模。

### ShadowLab 验收

- 同一场景可以切换 Hard、PCF、PCSS、CSM、EVSM 和 Virtual；
- 所有方法使用相同光源、相机和曝光；
- 有近接触、远距离、薄几何、动态 Caster 和大半影场景；
- 保存质量、GPU 时间、Atlas/Page 内存和动态更新成本；
- Path Tracer 完成后补充 Ray-traced Shadow 参考。

## 10. 阶段 3：MaterialLab、IBL 与大量光源

### 3.1 BRDF 基础

按组件单独实现和验证：

1. Lambert Diffuse；
2. Burley Diffuse；
3. GGX/Trowbridge-Reitz NDF；
4. Smith Correlated Visibility；
5. Schlick Fresnel；
6. Dielectric F0 与 Metallic 工作流；
7. 多重散射能量补偿；
8. White Furnace 和 Reciprocity 测试。

CPU 测试验证有限输入、极限粗糙度、掠射角和能量范围。Shader 中保留 Diffuse、D、V、F、PDF 和最终
贡献的 Debug View。

### 3.2 IBL

1. HDR Equirectangular 到 Cubemap；
2. Diffuse Irradiance Convolution；
3. GGX Importance Sampling；
4. Specular Prefilter Mip Chain；
5. BRDF Integration LUT；
6. Split Sum；
7. Rotation 和 Exposure；
8. PDF、Sample Count 和 Firefly 分析。

### 3.3 扩展材质

建议顺序：

1. Clear Coat；
2. Anisotropic GGX；
3. Cloth Sheen；
4. Thin Film；
5. Transmission；
6. 简化双层 Slab；
7. Subsurface Profile；
8. Hair BSDF 单独进入后期 FiberLab。

UE Substrate 的学习重点是 Slab、BSDF Layer、Coverage、Parameter Blending 和每像素 Closure 成本。Prism
第一版只需要显式的少数层，不需要先实现材质图编译器。

### 3.4 Light Culling 与采样

1. CPU Light List；
2. Tiled Deferred；
3. Clustered Deferred；
4. Logarithmic Z Slice；
5. Overflow 和最大光源数处理；
6. IES Profile；
7. LTC Rect Light；
8. Environment Alias Table；
9. Emissive Triangle Alias Table；
10. Light Tree；
11. RIS；
12. ReSTIR DI。

### 验收

- 材质球阵列与 White Furnace 结果可解释；
- Forward、Deferred 和 Path Tracer 共用同一参数语义；
- 1、16、256、1024 个光源有独立性能曲线；
- Tiled/Clustered 可视化每 Tile/Cluster 光源数量、Overflow 和空 Cluster；
- ReSTIR DI 显示 Reservoir Weight、M、Selected Light、Temporal/Spatial Reuse 来源。

## 11. 阶段 4：Hi-Z、屏幕空间与时域重建

### 4.1 Hi-Z

1. Min/Max Reduce 与 Depth Convention 对应；
2. NPOT 和奇数尺寸；
3. 完整 Mip Chain；
4. Mip 选择和覆盖范围验证；
5. Depth Pyramid Debug View。

Hi-Z 是 SSR、SSGI、GPU Occlusion、Contact Shadow 和部分粒子碰撞的公共 Algorithm，可以独立复用。

### 4.2 AO

1. SSAO Hemisphere Kernel；
2. Noise Rotation；
3. Range Check；
4. HBAO Horizon Search；
5. GTAO；
6. Bent Normal；
7. Half-resolution；
8. Depth/Normal Aware Blur；
9. Bilateral Upsample；
10. Temporal Accumulation；
11. RTAO Reference。

### 4.3 SSR

1. View-space Linear March；
2. Thickness Test；
3. Hi-Z Traversal；
4. Mip 上下行；
5. Binary Refinement；
6. Roughness Cone；
7. Hit Confidence；
8. Screen Edge Fade；
9. Environment Probe Fallback；
10. Temporal Resolve。

### 4.4 TAA 与 TAAU

1. Halton/R2 Jitter；
2. Current 到 Previous Reprojection；
3. Bilinear/Catmull-Rom History Sample；
4. Neighborhood Min/Max Clamp；
5. YCoCg Clamp；
6. Variance Clip；
7. Depth/Normal/Velocity Disocclusion；
8. Responsive/Reactive Mask；
9. History Confidence；
10. Sharpen；
11. TAAU；
12. Dynamic Resolution。

### TemporalLab 必备视图

- Jitter Pattern；
- Motion Vector；
- Reprojected UV；
- History Color；
- History Weight；
- Clamp Before/After；
- Reject Reason；
- Disocclusion；
- Reactive Mask；
- Variance/Confidence。

### 验收场景

- 亚像素细线和栅栏；
- 高亮物体经过高频背景；
- 平移、旋转和快速推进镜头；
- 新物体显露；
- 透明和像素动画；
- Render Size 与 Output Size 不同；
- Camera Cut、Resize、参数切换和 Replay Restart。

UE TSR 的学习重点是输入契约、History 管理、Reject/Responsive 机制、较低输入分辨率和后处理链位置，
而不是试图逐行复制生产 Shader。

## 12. 阶段 5：DXR 与参考 Path Tracer

### 12.1 Ray Tracing 基础

1. Device Feature 检测；
2. Static BLAS；
3. Compacted BLAS；
4. Dynamic BLAS Update；
5. TLAS Instance 和 Stable InstanceId；
6. Shader Table；
7. Ray Generation、Miss、Closest Hit、Any Hit；
8. Inline Ray Query；
9. Alpha-tested Geometry；
10. Debug Ray、HitT、InstanceId、PrimitiveId 和 Barycentrics。

### 12.2 Progressive Path Tracer

按以下顺序增加能力：

1. Primary Ray；
2. Lambert Bounce；
3. GGX Sampling；
4. Emissive Material；
5. Next Event Estimation；
6. Balance/Power Heuristic MIS；
7. Russian Roulette；
8. HDR Environment Importance Sampling；
9. Emissive Triangle Sampling；
10. Multiple Bounces；
11. Accumulation 和 Sample Count；
12. Firefly Clamp 仅作为可选诊断，不掩盖 PDF 错误。

### 12.3 AOV 与 Denoise

- Albedo；
- World Normal；
- Linear Depth；
- Motion；
- Direct/Indirect Diffuse；
- Direct/Indirect Specular；
- Hit Distance；
- First/Second Moment；
- Variance。

先实现 Bilateral、À-trous 和 SVGF 风格时空滤波，再决定是否接入外部 NRD 作为对照。

### 12.4 Wavefront

1. Ray Queue；
2. Material Classification；
3. Prefix Sum/Compaction；
4. Shadow Ray Queue；
5. 每 Bounce Queue 统计；
6. Persistent Thread 或间接 Dispatch 实验；
7. Megakernel 与 Wavefront 的发散、带宽和调度比较。

### 验收

- Cornell Box 能量和颜色传播合理；
- 环境采样 PDF 与采样分布匹配；
- MIS 不重复或漏算路径；
- 固定 Seed 可重复；
- 输出可作为 Shadow、AO、Reflection、GI 和 BRDF 的参考图。

## 13. 阶段 6：现代直接光、GI 与混合反射

### 13.1 DDGI

1. 规则 Probe Grid；
2. Probe Ray Trace；
3. Irradiance Atlas；
4. Distance/Visibility Moments；
5. Octahedral Mapping；
6. Hysteresis；
7. Probe Relocation；
8. Probe Classification；
9. Backface 和 Inside-Geometry 处理；
10. Scrolling Volume / Clipmap；
11. Probe Update Budget。

### 13.2 Screen Probe 与 Radiance Cache

1. Screen Probe Placement；
2. Adaptive Probe；
3. Screen Trace；
4. Miss Fallback；
5. World-space Radiance Cache；
6. Probe Filtering；
7. Final Gather；
8. Temporal Reuse；
9. Full-resolution Shading Composite。

### 13.3 Hybrid Trace

按照从便宜到昂贵的顺序：

1. Screen Trace；
2. Distance Field / Voxel / Surfel 软件表示；
3. Hardware Ray Tracing；
4. Environment/Probe Fallback。

每条路径必须输出来源标记、命中距离和 Confidence，便于观察切换边界。

### 13.4 ReSTIR

1. RIS 基线；
2. Reservoir Update；
3. Weight Sum 和 M；
4. Temporal Reuse；
5. Spatial Reuse；
6. Visibility Reuse；
7. Bias 修正；
8. ReSTIR DI；
9. ReSTIR GI；
10. 与 Light Tree 和 Path Tracer 对照。

### 验收

- 室内外转换、门开关、动态灯和动态物体下没有不可解释的长期残留；
- 可分别关闭 Screen、Software、Hardware 路径；
- 显示 Probe、Ray、Cache Cell、Reservoir 和 History Reject；
- 与 Path Tracer 比较 HDR 误差和收敛；
- 明确记录 Sample Count、更新预算、GPU 时间和显存。

## 14. 阶段 7：GPU Driven Visibility 与虚拟化几何

### 14.1 Instance 管线

1. CPU Frustum Culling 基线；
2. GPU Frustum Culling；
3. Hi-Z Occlusion Culling；
4. Scan、Compaction 和 Visible List；
5. ExecuteIndirect；
6. GPU LOD Selection；
7. Occlusion History；
8. Camera Cut 后保守可见性。

### 14.2 Meshlet

1. CPU Meshlet Builder；
2. 最大 Vertex/Primitive 限制；
3. Bounding Sphere；
4. Normal Cone；
5. Meshlet Frustum/Cone/Occlusion Culling；
6. 传统 Indexed Draw Fallback；
7. Mesh Shader；
8. Amplification Shader；
9. Meshlet Occupancy 和 Wave 利用率分析。

### 14.3 Visibility Buffer

1. PrimitiveId + Barycentrics；
2. Attribute Fetch；
3. Material Resolve；
4. Quad Derivative/显式梯度；
5. Alpha Mask；
6. 与 GBuffer 带宽和材质复杂度比较。

### 14.4 Cluster LOD 和 Streaming

1. Cluster Error Metric；
2. Hierarchical Cluster Tree/DAG；
3. Screen-space Error Selection；
4. Parent/Child 连续性；
5. Cluster Page；
6. Request Buffer；
7. Physical Page Pool；
8. Residency 和 Eviction；
9. Software Rasterization 实验；
10. 与 Virtual Shadow Page Culling 组合。

Prism 应把这条路线称为 Nanite-style 或 Virtualized Geometry Lab，明确它是对核心思想的重建，
而不是声称复刻 UE Nanite 的完整内部实现。

### 验收

- 城市/阵列场景中 CPU Draw Submission 显著下降；
- 逐级显示 Instance、Meshlet、Cluster 和 Page 的裁剪原因；
- 快速转动相机不会因旧 Hi-Z 永久漏绘；
- LOD 变化有误差指标，不只根据距离硬切；
- 给出传统 Draw、ExecuteIndirect、Mesh Shader 三条路径的性能比较。

## 15. 阶段 8：Virtual Texture 与资源流送

### 实施内容

1. Tiled Resource 基础；
2. Virtual Address 到 Physical Tile；
3. Page Table；
4. Feedback Buffer；
5. Feedback 去重和压缩；
6. Physical Tile Pool；
7. LRU/Clock Eviction；
8. Parent Mip Fallback；
9. 异步磁盘读取和 Upload；
10. Sampler Feedback 路径；
11. Pre-fetch 和 Camera Velocity 实验；
12. Residency、Requested Mip、Missing Tile 和 Eviction 可视化。

虚拟纹理、虚拟阴影和虚拟几何可以共享概念与调试 UI，但第一版不要强行共用同一 Page Manager。等三个
系统都完成最小实现后，再提取真正相同的地址、分配和统计组件。

## 16. 阶段 9：专题算法

以下方向建立在前面公共输入之上，可以按兴趣选择，不要求严格顺序。

### Atmosphere 和 Clouds

- Rayleigh/Mie；
- Transmittance、Multi-scattering、Sky-view LUT；
- Aerial Perspective；
- Weather Map；
- Cloud Density Ray March；
- Empty-space Skipping；
- Cloud Shadow；
- Temporal Reconstruction。

### Participating Media

- Beer-Lambert；
- Henyey-Greenstein；
- Froxel Volume；
- Light Injection；
- Volumetric Shadow；
- Temporal Reprojection；
- Heterogeneous Medium 和 Delta Tracking 参考。

### Water

- Gerstner Wave；
- Tessendorf FFT Ocean；
- Normal/Jacobian/Foam；
- Reflection/Refraction；
- Beer-Lambert Absorption；
- Underwater Scattering；
- Screen-space 或 Photon Caustics 实验。

### Transparency

- Sorted Alpha Blend；
- Weighted Blended OIT；
- Depth Peeling；
- Per-pixel Linked List；
- Stochastic Transparency；
- 多层折射和时域 Coverage。

### Hair、Cloth 和 Subsurface

- Kajiya-Kay；
- Marschner；
- Hair Multiple Scattering；
- Strand LOD；
- Cloth Sheen；
- Diffusion Profile；
- Separable SSS；
- Thickness Transmission；
- Random Walk Reference。

## 17. 阶段 10：UE 之外的研究方向

这些方向放在 `algorithms/research/<topic>` 或独立技术族中，不污染基础渲染路径：

- Bidirectional Path Tracing；
- Metropolis Light Transport；
- Vertex Connection and Merging；
- Photon Mapping；
- ReSTIR Path Tracing；
- Path Guiding；
- Spectral Rendering；
- Differentiable/Inverse Rendering；
- 3D Gaussian Splatting；
- Mesh-Splat Hybrid；
- Neural Radiance Cache；
- Neural Texture Compression；
- Learned Denoising；
- Stochastic Texture Filtering。

每个研究模块必须说明训练/预处理边界、数据来源、许可、硬件要求以及是否仍能使用 HLSL/DX12 工具链。

## 18. Sample 组合版图

| Sample | 主要组合 | 研究目的 |
| --- | --- | --- |
| `PrismDeferred` | Surface → Shadow → Clustered Lighting → IBL → TAA → Display | 长期综合基线 |
| `PrismShadowLab` | Hard / PCF / PCSS / CSM / EVSM / Virtual | 阴影质量、缓存和分页 |
| `PrismMaterialLab` | BSDF → IBL → Area Light → Path Reference | 材质能量与分层 |
| `PrismLightLab` | Tile / Cluster / Light Tree / RIS / ReSTIR DI | 大量光源与采样 |
| `PrismScreenSpaceLab` | Hi-Z → AO / SSR / SSGI | 屏幕空间命中与缺失 |
| `PrismTemporalLab` | Motion → TAA/TAAU → Denoise | History、显露和重建 |
| `PrismPathTracer` | DXR → NEE/MIS → Accumulation | 参考图和采样实验 |
| `PrismHybridGI` | Screen → DDGI/Cache → DXR → Denoise | Lumen 风格混合 GI |
| `PrismGpuDriven` | Hi-Z → Instance/Meshlet Cull → Indirect/Mesh Shader | GPU Driven 和虚拟几何 |
| `PrismVolumeLab` | Atmosphere → Fog → Clouds | 参与介质与时域采样 |
| `PrismResearch` | 可替换研究模块 | 非 UE 算法和论文复现 |

每个 Sample 至少提供：

- 一个诊断场景；
- 一个 Beauty 场景；
- 一个动态失败场景；
- 固定 Camera Bookmark；
- Low、Medium、High、Reference 质量档；
- Analysis Preset；
- 可重复 Benchmark Preset。

## 19. 建议共用的展示场景

### 雕塑庭院

- 柱廊、格栅、石材和金属雕塑；
- 低角度方向光和冷色环境；
- 用于 Shadow、AO、GI、SSS 和 Volume；
- 必须包含近接触、远景、薄片和大半影。

### 材质摄影棚

- 陶瓷、金属、玻璃、清漆、布料和皮肤测试体；
- HDR 环境与矩形灯；
- 用于 BRDF、IBL、Reflection、Subsurface 和 Path Tracing；
- 包含 White Furnace 和 Roughness/Metalness 阵列。

### 雨夜街角或室内

- 大量局部灯、湿地面、暖窗光和高亮标牌；
- 用于 Clustered Lighting、ReSTIR、SSR、Denoise 和 TAA；
- 包含快速相机平移、遮挡显露和动态灯。

### 海岸山谷

- 天空、云、雾、水、地形和植被；
- 用于 Atmosphere、Volume、Ocean、Terrain 和 GPU Driven；
- 保存固定日照时间、风场和 Flythrough。

正式质量比较必须锁定相机、时间、曝光和随机种子。A/B 交互模式可同帧运行两个方法；正式性能测试应
分别运行，避免缓存、带宽和显存竞争影响结论。

## 20. UE 架构学习支线

Prism 的每个阶段都配一个更小的 UE 实验，用来把独立图形算法映射到生产引擎架构。

### UE-1：Global Shader 和 RDG

建立一个独立 UE Plugin：

1. `FGlobalShader`；
2. Shader Parameter Struct；
3. `FSceneViewExtensionBase`；
4. 在明确位置添加 RDG Compute/Fullscreen Pass；
5. 创建 RDG Texture/Buffer；
6. Extract 到跨帧 History；
7. 使用 RDG Event 和 RDG Insights 观察资源生命周期。

阅读重点：`FRDGBuilder`、Pass Parameters、Access、External Resource、Transient Resource、Pass Culling、
Async Compute Fence 和 Barrier。

### UE-2：Thread Ownership

跟踪一份数据从 Game Thread 到 Render Thread：

1. `UPrimitiveComponent`；
2. `FPrimitiveSceneProxy`；
3. Render Command；
4. `FRenderResource` 初始化和释放；
5. Fence 和延迟销毁。

练习目标是能解释每块内存由哪个线程拥有、什么时候可以修改、什么时候可以销毁，而不只让代码运行。

### UE-3：Mesh Drawing Pipeline

跟踪：

```text
UPrimitiveComponent
  -> FPrimitiveSceneProxy
  -> FMeshBatch
  -> FMeshPassProcessor
  -> FMeshDrawCommand
  -> RHICommandList
```

分别观察 Cached 和 Dynamic Mesh Batch、Draw Command 合并、GPU Scene、Dynamic Instancing 和自定义 Mesh
Pass。对照 Prism 中每帧直接 Draw 的成本，理解 UE 为什么使用 Retained Mode。

### UE-4：Shader、Permutation 和 PSO

1. Global/Material/Mesh Material Shader 的差异；
2. Vertex Factory；
3. Shader Permutation Domain；
4. `ShouldCompilePermutation`；
5. PSO Initializer；
6. PSO Precaching；
7. 异步编译和首次使用 Hitch；
8. Stable Cache 与 Driver Cache。

### UE-5：生产特性的拆解

按依赖顺序阅读和实验：

1. TSR：Motion、History、Reject、Upscale、Post Process 位置；
2. VSM：Page Request、Cache、Clipmap、Nanite Culling；
3. Nanite：Cluster、Hierarchy、Streaming、Visibility、Material Resolve；
4. Lumen：Scene Representation、Screen Trace、Surface Cache、Radiance Cache、Final Gather；
5. Substrate：Slab、Layer、Closure、Parameter Blending；
6. GPU Scene 和 Instance Culling；
7. Async Compute 和 RDG 调度。

每次 UE 阅读输出一份短笔记：入口、数据生产者、主要 Pass、跨帧状态、关键优化、限制，以及 Prism 中
可以进行的最小复现实验。

## 21. 是否在 Prism 中实现 Render Graph

当前 Prism 明确不强制 Algorithm 使用 Render Graph，这一点近期保持不变。完整 Render Graph 只有在
以下条件同时出现后才进入计划：

1. 至少三个真实算法需要自动资源生命周期；
2. 手写 Barrier 或临时资源复用成为明确错误来源；
3. 已有稳定的 Pass Inputs/Outputs；
4. 能用现有 GPU 测试验证 Graph 前后结果一致。

届时先实现独立 `RenderGraphLab`：

- Pass 声明；
- 资源读写依赖；
- 拓扑排序；
- Dead Pass Culling；
- 生命周期分析；
- Transient Resource Aliasing；
- Barrier 生成；
- Graph Visualization。

在两个真实 Algorithm 验证收益后，再决定是否放入 `framework/render/graph`。不要为了形式接近 UE，
提前把简单 Pass 隐藏在复杂抽象中。

## 22. 测试和性能规范

### CPU Test

适合验证：

- Matrix/Depth 转换；
- BRDF、PDF 和 MIS Weight；
- Cascade Split；
- Cluster Z Slice；
- Page Table、Allocator 和 Eviction；
- Sampling Distribution；
- Reservoir Update；
- Replay/History 状态机。

### GPU Contract

适合验证：

- Odd Size Dispatch；
- Texture Format 和通道含义；
- CPU/HLSL Shared Struct；
- Prefix Sum/Compaction；
- Hi-Z Reduce；
- Motion 和 Position Reconstruction；
- 指定像素的 Shadow/BRDF 结果；
- Shader Reload 后接口保持。

### 图像回归

- Reference 保存在 Display Transform 之前；
- LDR Screenshot 用于最终展示，不替代 HDR 数值比较；
- 时域算法先固定 Replay、Seed 和 Warm-up；
- Monte Carlo 算法使用多 Seed 的均值、方差和收敛曲线；
- 同时记录 Max、Mean、RMSE，必要时增加感知指标。

### 性能报告

每项优化至少报告：

- GPU Pass 时间和总时间；
- Render/Output 分辨率；
- 显存和瞬态峰值；
- Draw/Dispatch/Ray 数量；
- 可见对象、光源、Probe、Page 或 Ray 数量；
- Quality 参数；
- 与基线的图像误差；
- 测试 GPU、驱动和构建配置。

优化结论必须说明瓶颈属于 ALU、带宽、Occupancy、发散、同步、CPU Submission、PSO 或资源驻留中的哪一类。

## 23. 推荐执行顺序

不要一次建立全部目录。只在开始真实实现时创建对应模块。

| 里程碑 | 交付物 | 退出条件 |
| --- | --- | --- |
| M0 | 能力报告、Marker、Run Manifest、通用错误视图 | 一次运行可以完整复现和解释 |
| M1 | Surface、GBuffer、Motion、基础 GGX、Display、`PrismDeferred` | 自有管线输出稳定 HDR 画面 |
| M2 | 完整 `ShadowLab` 到 CSM/PCSS/EVSM | 偏差、半影、级联和性能可解释 |
| M3 | IBL、扩展 BRDF、Tiled/Clustered Lighting | 材质和大量光源有参考与曲线 |
| M4 | Hi-Z、GTAO、SSR、TAA/TAAU | History、显露和屏幕缺失可视化 |
| M5 | DXR Path Tracer、AOV、基础 Denoise | 能生成其他算法的参考图 |
| M6 | DDGI、Radiance Cache 或 ReSTIR DI 中的一条主线 | 动态场景有质量/性能/失败报告 |
| M7 | GPU Instance/Meshlet/Indirect/Mesh Shader | CPU Submission 和可见性收益明确 |
| M8 | Virtual Shadow/Texture/Geometry 中至少两种分页系统 | 驻留、缓存和失效行为可观测 |
| M9 | Volume、Water、Hair、Splat 或 Neural 专题 | 不破坏基础组合契约 |

M1 到 M5 是核心课程，建议按顺序完成。M6 以后根据兴趣选择主线，不需要追求目录数量。对于个人项目，
一项少而完整、拥有参考、诊断和报告的实现，比十项只能输出一张图片的实现更有价值。

## 24. 近期第一个实现切片

下一步直接建立 `algorithms/surface` 和 `samples/deferred`，限定第一版只完成：

1. 程序场景的 Depth Prepass；
2. World Normal/Roughness、BaseColor/Metalness、Motion GBuffer；
3. World Position Reconstruction；
4. 单方向光 Lambert + GGX；
5. 固定曝光和 Tone Mapping；
6. 所有 GBuffer Debug View；
7. 静态、相机运动、物体运动三个 Preset；
8. 一个 GPU Contract 验证 Position/Motion；
9. 一份 Forward 与 Deferred 对比截图和 GPU 时间。

完成这个切片后，再把现有 `algorithms/shadows` 接入 `PrismDeferred`。这样 ShadowLab 从第一天起就建立在
可复用的 Surface、Lighting 和 Display 输入上，而不会形成只能在单个 Sample 中运行的孤立实现。

## 25. 主要参考

### Unreal Engine

- [Graphics Programming for Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/graphics-programming-for-unreal-engine)
- [Render Dependency Graph](https://dev.epicgames.com/documentation/unreal-engine/render-dependency-graph-in-unreal-engine?lang=en-US)
- [Mesh Drawing Pipeline](https://dev.epicgames.com/documentation/unreal-engine/mesh-drawing-pipeline-in-unreal-engine)
- [Threaded Rendering](https://dev.epicgames.com/documentation/unreal-engine/threaded-rendering-in-unreal-engine?lang=en-US)
- [PSO Precaching](https://dev.epicgames.com/documentation/unreal-engine/pso-precaching-for-unreal-engine)
- [Nanite Virtualized Geometry](https://dev.epicgames.com/documentation/unreal-engine/nanite-in-unreal-engine)
- [Virtual Shadow Maps](https://dev.epicgames.com/documentation/it-it/unreal-engine/virtual-shadow-maps-in-unreal-engine)
- [Lumen Technical Details](https://dev.epicgames.com/documentation/unreal-engine/lumen-technical-details-in-unreal-engine)
- [Temporal Super Resolution](https://dev.epicgames.com/documentation/unreal-engine/temporal-super-resolution-in-unreal-engine?lang=en-US)
- [Substrate Materials](https://dev.epicgames.com/documentation/unreal-engine/substrate-materials-in-unreal-engine?lang=en-US)

### Direct3D 12

- [DirectX Specifications](https://microsoft.github.io/DirectX-Specs/)
- [DirectX Raytracing](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html)
- [Mesh Shader](https://microsoft.github.io/DirectX-Specs/d3d/MeshShader.html)
- [Sampler Feedback](https://microsoft.github.io/DirectX-Specs/d3d/SamplerFeedback.html)
- [Work Graphs](https://microsoft.github.io/DirectX-Specs/d3d/WorkGraphs.html)

### PrismLab 内部文档

- [当前架构](architecture.md)
- [实验框架使用说明](framework-experiments.md)
- [编码风格](coding-style.md)
- [历史架构评审](architecture-review-2026-09-22.md)
