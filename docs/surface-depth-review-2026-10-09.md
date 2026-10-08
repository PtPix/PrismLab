# Surface 深度 Prepass 评审与重构方案

评审日期 2026-10-09，基准提交 `0eb83bd`。

本文对照 `rendering-study-roadmap.md` 的阶段 1 切片（§8、§24）与 `docs/architecture.md` 的模块
规范，评审 `algorithms/Surface` 和 `samples/Deferred` 的现状，给出问题清单、目标接口、目录重命名
方案与命名决策。附录 A/B 是评审所依据的"新增算法/新增 Sample 检查清单"。

本文只记录结论与方案。实施状态见文末第 8 节。

> **行号说明**：文中行号以 `0eb83bd` 为基准。此后对同一批文件做过格式化改动，行号会漂移，
> 引用时以符号名（函数名、结构体名）为准。

---

## 1. 评审依据

| 依据 | 出处 | 相关条款 |
| --- | --- | --- |
| 算法边界 | `rendering-study-roadmap.md` §4.2 | Algorithm 拥有一项技术的 Settings/Inputs/Outputs/Shader/调度/内部状态/Debug/测试；不得包含 `ExperimentContext`、ImGui、窗口、命令行、演示场景 |
| Sample 边界 | §4.3 | Sample 拥有场景选择、算法组合、展示资源、UI、预设、三种工作模式 |
| 数据约定 | §5 | 世界空间右手 Y-up 米；视空间左手 +Z；Device Depth 默认 Forward-Z；粗糙度存感知值；Motion = `previousUnjitteredUV - currentUnjitteredUV`；新算法必须声明分辨率、格式、颜色空间、Depth Convention、Normal Space、Roughness Encoding、Motion 单位、资源归属、History 所属 ViewId |
| 完成条件 | §6 | 十项：理论说明、CPU/离线基线、朴素 GPU 实现、中间结果可视化、优化实现、参考结果、失败案例、自动验证、性能报告、UE 对照 |
| 度量基础 | §7 | M0：能力报告、每个 Algorithm Pass 的稳定 GPU Marker 与计时名、每帧 Draw/Dispatch 计数、Run Manifest、通用错误视图 |
| 阶段 1 切片 | §8、§24 | Depth Prepass → GBuffer → Position Reconstruction → 单方向光 → Display；验收要求 CPU/GPU 世界位置误差在容差内、静止场景 Motion 为零、Jitter 不污染未抖动 Motion |
| 目录规范 | `docs/architecture.md` | "Directories use lowercase names; C++ types/files use PascalCase"；"Small modules keep sources beside their headers… Do not add empty layers"；算法不得依赖 Donut |
| 编码风格 | `docs/coding-style.md` | 开括号另起一行；控制流一律加花括号；**缩进用 Tab**，显示宽度 4 |
| 下沉判据 | §2 | 只有至少两个真实算法出现相同的资源或调度问题时，才把方案下沉到 `framework/` |
| Replay 要求 | `docs/framework-experiments.md` | 自定义场景动画必须由逻辑时钟驱动，并从起点重建历史 |

---

## 2. 当前状态

### 2.1 已完成

| 层 | 内容 |
| --- | --- |
| `algorithms/Surface` | `FDepthRenderer` 可复用深度预渲染：借用式 position-only 几何、per-draw push constant `ObjectToClip`、Forward/Reverse-Z 双 PSO、对 buffer range 与 framebuffer 的完整前置校验 |
| `samples/Deferred` | 程序场景 → 深度 → 设备/线性深度预览；发布 DebugView 与 Comparison；`--config` 可覆盖预设 |
| framework 契约 | 已提供但尚未被 Prism 自己的算法消费：`FGBufferSchema`、`FSceneSurfaceData` + `Validate`、`FSceneFrameData`（Current/Previous/revision）、`Gpu::FSceneGpuData`、`IDisplayChain`、`CameraData` 的 CPU 侧 `LinearizeDepth` / `DeviceDepthFromLinear` / `ReconstructWorldPosition` / `WorldToUnjitteredUv` |
| 验证模板 | `tests/gpu/contract` 已是"GPU 算 → CPU 稀疏校验"的完整范式，校验 depth → world position 链 |
| 资源设施 | `TextureCache` 支持 MRT（`GetFramebuffer(const std::vector<ITexture*>&, ITexture*)`）；`FRasterPass` 按 framebuffer 格式缓存 PSO 变体 |

### 2.2 构建状态

`cmake --preset my-project` + `cmake --build build/my-project --config Debug` 通过，输出
`bin/Debug/{PrismDeferred,PrismForward,PrismStarter}.exe`。

注意：`build/my-project` 与 `build/ue-style-audit` 两个既存构建树在本次评审前都没有
`PrismDeferred` 产物，即该 Sample 自 `0eb83bd` 起从未成功构建过。

---

## 3. 问题清单

### A 类：正确性问题

#### A1. 空批次 → 报错并且不清屏

`FDepthRenderer::Record` 的前置校验里包含 `Geometry.Draws.empty()`，与目标格式校验挤在同一个
条件里，共用一条错误信息。

两个缺陷叠加：

1. **行为错**：合法的空场景（或将来所有实例被剔除）应当"清屏 + 返回 Ok"。现在返回
   `InvalidArgument`，深度缓冲保留上一帧内容 —— 静默的错画面。
2. **错误信息误导**：真实原因是"没有 draw"，报的却是"深度目标不是 D32 单采样纯深度 mip0/slice0"。

对比 `FRasterPass` 的校验风格：它校验状态完整性，不把"没有工作"当作错误。

#### A2. 清屏职责放在算法内部，无法表达"不清屏"

`Record` 内部调用 `clearDepthStencilTexture`。对"唯一的深度生产者"是合理的，但下一个消费者
GBuffer 必须**读**深度且**不得重复清**，而当前接口没有 `bClearDepth` 这样的输入可以表达。

清屏是"资源的初始化语义"，属于持有资源的一侧。把它埋在 `Record` 里，等于把算法的调用语义和
资源生命周期绑死。

#### A3. 传入的 `Convention` 与纹理实际 clearValue 没有一致性校验

`Record` 用参数 `Convention` 选 PSO 和清屏值；纹理的 clear value 由 sample 从相机算出后写进
`FTextureRequest.ClearDepth`，最终进入 `nvrhi::TextureDesc.clearValue`。

两者不一致时（`SetPrimaryDepthConvention` 在帧中改变，或 sample 忘记同步），会得到"用 reverse-Z
的 PSO 画进 forward-Z 清屏的缓冲"，而**没有任何代码会发现**。纹理 desc 里明明有 `clearValue`
可供校验。

### B 类：接口与规范问题

#### B1. 没有 Settings 结构（违反 §4.2）

`nvrhi::RasterCullMode::None` 硬编码在 `Initialize`。路线图 §9.2.1 的"单面和双面 Shadow Caster"、
§6 的"薄几何失败案例"都需要 cull mode 可调，目前没有任何旋钮。

#### B2. 没有 Inputs/Outputs，6 个散参数，且 `Depth` 与 `Target` 冗余（违反 §4.2）

```cpp
FStatus Record(ICommandList*, const FDepthBatch&, const float4x4& WorldToClip,
               EDepthConvention, ITexture* Depth, IFramebuffer* Target);
```

- 按 §4.2 应为 `Record(Commands, Inputs, Settings, Outputs)`。
- `WorldToClip` 与 `Convention` 都是 `FCameraData` 的派生量，不该由调用方拆开传递。
- `Depth` 与 `Target` 是同一条信息传两遍，然后算法要反过来校验二者一致。framebuffer 的
  depth attachment 里已有纹理。把"调用方可能传错"变成算法的校验负担，是接口设计问题。

#### B3. `Initialize` 签名与框架标准不一致

框架统一使用 `Gpu::FRenderServices`（打包 `Device/Shaders/Targets/Buffers/Resources/Profiler/
CommonPasses`）。同一 Sample 内的 `FDepthPreview::Initialize(Gpu::FRenderServices&)` 才是正确
范例。当前 `Initialize(IDevice*, FShaderLibrary&)` 拿不到 `Profiler`，直接堵死 B4。

#### B4. 没有 GPU marker / 计时（违反 §7 M0 第 2 项）

`Record` 内没有 `FScopedGpuScope`。这是全部问题里改动最小、收益最高的一条：

```cpp
Gpu::FScopedGpuScope Scope(*Services.Profiler, Commands, "Surface.Depth");
```

同时提供 PIX / Nsight 中的同名区间。

#### B5. 没有 Debug 数据出口（违反 §7.3）

没有 Draw 数、三角形数、深度是否确实被清过这些信息。

#### B6. `PositionStride` 是"假的可配置"；`BaseVertex` 用运行时错误掩盖类型不匹配

- `FDepthBuffers::PositionStride` 默认 `sizeof(dm::float3)` 且可赋值，但 `Record` 校验
  `PositionStride != sizeof(dm::float3)` 即报错，`Initialize` 的 input layout 也写死
  `elementStride 12`。既然只能是 12，该字段就是误导。
- `FDrawRecord::BaseVertex` 是 `int32_t`，`FDepthDraw::BaseVertex` 是 `uint32_t`，转换处检查
  `< 0` 就返回 nullptr 并吞掉整帧。NVRHI 的 `setStartVertexLocation` 确实只接受 uint32，限制
  真实 —— 但它属于**场景 producer 的契约**，应在 `FGeometryBatch` 生产侧保证，而不是每帧在渲染
  热路径上做运行时检查。

### C 类：缺失项

#### C1. 没有 README.md（违反 §4.2 与 §6）

算法族 README 至少应包含：原始论文与实现参考、自己的推导、Pass 图、资源表、复杂度、参数解释、
已知问题与实验结论。

#### C2. 该 Pass 没有任何自动验证 —— 而 `PrismContract` 会让人误以为有

这是风险最高的一条。`PrismContract` 看起来很到位，但它验证的是 **legacy Forward 的深度缓冲**
（`contract.h` 使用 `Adapter::FForwardScene`），不是 `FDepthRenderer`。`tests/cpu/Main.cpp` 的
`RunDepthConventionTests` 测的也是 `CameraController` 的约定切换。

结论：**`FDepthRenderer` 目前唯一的证据是"人眼看深度预览图正常"**，违反 §6 第 8 项与 §24 第 8 项。
后果是反直觉的：改动深度链后运行 `PrismContract --smoke-test` 会通过，因为它跑的是另一条路径。

#### C3. 深度目标与相机约定的同步逻辑在三处重复（已达 §2 下沉判据）

```cpp
// samples/Deferred/Deferred.cpp、samples/forward/forward.cpp、tests/gpu/contract/contract.cpp
if (DepthRequest.ClearDepth != GetDepthClearValue(Frame.Camera.DepthConvention))
{
    DepthRequest.ClearDepth = GetDepthClearValue(Frame.Camera.DepthConvention);
    /* 各自的 OnResize() */
}
```

§2 的判据是"两个以上真实算法出现相同问题就下沉到 `framework/`"。三处重复已经达标。

### D 类：命名、目录与风格

#### D1. 目录大小写与自身规范冲突

见第 5 节。

#### D2. `DepthProjection.h` 是死代码

见第 7 节。**已在本提交中删除**（同时补掉了 `CMakeLists.txt` 的悬空引用，否则 configure 直接失败）。

#### D3. 缩进风格漂移

`docs/coding-style.md` 规定"缩进用 Tab，宽度 4"，`framework/` 下的文件确实使用 Tab
（`RasterPass.cpp` 有 294 个 Tab）；但 `algorithms/Surface/*` 与 `samples/Deferred/*` 全部使用
**空格**（Tab 计数为 0）。目录重命名（第 5 节）是修掉这一漂移的天然时机：改名与格式统一放在同一个
提交里，diff 才不会混入无关噪音。

---

## 4. 修改方案

### 4.1 目标接口

按 §4.2 的最小布局拆成三份头文件。

```cpp
// algorithms/surface/DepthSettings.h
#pragma once
#include <cstdint>

namespace Prism::Surface
{
    // 不直接使用 nvrhi::RasterCullMode：Settings 之后要进 FParamTable，
    // 而参数表只支持 bool/int/float，所以用 Prism 自己的整型枚举。
    enum class EDepthCullMode : uint32_t
    {
        TwoSided = 0,
        FrontFacing,
        BackFacing,
        Count
    };

    struct FDepthSettings
    {
        bool bClearDepth = true;   // GBuffer 会传 false
        EDepthCullMode CullMode = EDepthCullMode::TwoSided;
    };
}
```

```cpp
// algorithms/surface/DepthInputs.h
#pragma once
#include <framework/render/data/CameraData.h>
#include <nvrhi/nvrhi.h>
#include <vector>

namespace Prism::Surface
{
    struct FDepthBuffers
    {
        nvrhi::IBuffer* Positions = nullptr;
        nvrhi::BufferRange PositionRange;
        nvrhi::IBuffer* Indices = nullptr;
        nvrhi::Format IndexFormat = nvrhi::Format::R32_UINT;

        // PositionStride 字段删除：布局固定为 float3 / 12 字节，写进注释与断言，
        // 不再让调用方以为可以传别的 stride。
    };

    struct FDepthDraw
    {
        uint32_t BufferGroupIndex = 0;
        uint32_t FirstIndex = 0;   // 元素索引，不是字节偏移
        uint32_t IndexCount = 0;
        uint32_t BaseVertex = 0;   // 生产侧保证非负；NVRHI 只接受 uint32
        dm::affine3 ObjectToWorld = dm::affine3::identity();
    };

    struct FDepthBatch
    {
        std::vector<FDepthBuffers> BufferGroups;
        std::vector<FDepthDraw> Draws;
    };

    struct FDepthInputs
    {
        // 使用 Camera.Raster（含 jitter）——必须与所有写同一深度缓冲的光栅 Pass 一致。
        const FDepthBatch& Geometry;
        const FCameraData& Camera;
        nvrhi::IFramebuffer* Target = nullptr;   // 深度附件即输出，不再单独传纹理
    };

    struct FDepthDebugInfo
    {
        uint32_t DrawCount = 0;
        uint64_t TriangleCount = 0;
        bool bCleared = false;
    };

    struct FDepthOutputs
    {
        // 由 Record 从 Target 的深度附件填入，借用不拥有；供调试视图与比较发布使用。
        nvrhi::ITexture* Depth = nullptr;
        FDepthDebugInfo Debug;
    };
}
```

```cpp
// algorithms/surface/DepthRenderer.h（改动部分）
class FDepthRenderer
{
public:
    FStatus Initialize(Gpu::FRenderServices& Services);
    FStatus Record(nvrhi::ICommandList* Commands, const FDepthInputs& Inputs,
                   const FDepthSettings& Settings, FDepthOutputs& Outputs);

private:
    Gpu::FRasterPass ForwardPass;
    Gpu::FRasterPass ReversePass;
    nvrhi::BindingLayoutHandle BindingLayout;
    nvrhi::InputLayoutHandle InputLayout;
    Gpu::FGpuProfiler* Profiler = nullptr;   // 从 Services 缓存
};
```

### 4.2 `Record` 的执行顺序

```cpp
// ① 参数校验：Target 非空、只有一个深度附件、mip0/slice0、D32、单采样
// ② 一致性校验（新增，覆盖 A3）：
//    深度附件纹理的 clearValue 必须等于 GetDepthClearValue(Convention)，
//    否则返回 InvalidArgument 并说明是约定不一致，而不是笼统的"目标格式不对"
// ③ 清屏：受 Settings.bClearDepth 控制 —— 独立于是否存在 draw
// ④ 空批次：清完即返回 Ok，并填写 Outputs，不是错误
// ⑤ 计时区间：Gpu::FScopedGpuScope Scope(*Profiler, Commands, "Surface.Depth");
// ⑥ 逐个 draw：保留现有校验，把 stride / negative BaseVertex 的断言前移到几何契约侧
```

### 4.3 Sample 侧简化效果

改造后 `samples/Deferred` 的 `Render` 中，这一段（13 行几何转换 + 6 个散参数 + BaseVertex 检查）：

```cpp
DepthBatch.Draws.push_back({Draw.BufferGroupIndex, Draw.FirstIndex, Draw.IndexCount, ...});
DepthRenderer.Record(Frame.Commands, DepthBatch, Frame.Camera.Raster.WorldToClip,
                     Frame.Camera.DepthConvention, Depth, DepthTarget);
```

变为：

```cpp
FDepthInputs Inputs{ DepthBatch, Frame.Camera, DepthTarget };
FDepthSettings Settings;                 // bClearDepth = true
FDepthOutputs Outputs;
const FStatus Status = DepthRenderer.Record(Frame.Commands, Inputs, Settings, Outputs);
Context.Tools.DebugViews->Publish(GetName(), "Device depth", Outputs.Depth);
```

Sample 不再需要知道 `Raster.WorldToClip`，不再需要知道 `EDepthConvention`，也不再需要手动转换
几何 —— 这些正是平台层应当承担的工作。

### 4.4 C3 的下沉方案

在 `framework/render/resources/` 增加一个小工具，消掉三处重复：

```cpp
// 语义：跟踪"相机深度约定 → 深度目标请求"；约定变化时更新 clearValue。
// TextureCache 已把 clearValue 变化判定为需要重建纹理，因此调用方只需清自己的 binding cache。
class FDepthTargetHelper
{
public:
    void Configure(Gpu::FTextureRequest& Request, const char* Name);                     // Initialize 时
    bool Sync(nvrhi::ITexture*& OutTexture, const FCameraData& Camera,
              Gpu::FTextureCache& Targets);                                              // 每帧；返回是否重建过
};
```

返回 `true` 时 Sample 调用自己的 `ClearBindings()`。三个 Sample 各减约 6 行，且不可能再忘记同步。

### 4.5 验证补齐（对应 C2）

新增（或扩展 `PrismContract`）一条针对 Prism 自有深度链的检查：

1. **场景 fixture**：单三角形 + 单位立方体（§8 明确要求的验收场景）。
2. **GPU**：用 `FDepthRenderer` 渲染深度，复用 `contract_check.hlsl` 的模式解码世界位置。
3. **CPU**：`FCameraData::ReconstructRasterWorldPosition` 重算，按 `SampleStride` 稀疏比较。
4. **断言**：空批次时深度必须等于 clear value —— 这一条正好覆盖 A1。
5. **断言**：forward-Z 与 reverse-Z 两个约定都要跑通（`contract_check.hlsl` 已有 `bReverseZ` 分支）。

---

## 5. 目录重命名

### 5.1 现状

`docs/architecture.md` 规定目录用小写；路线图 §24 自己写的也是小写（"下一步直接建立
`algorithms/surface` 和 `samples/deferred`"）。当前处于半迁移状态：

| 目录 | 大小写 | 是否符合规范 |
| --- | --- | --- |
| `algorithms/shadows` | 小写 | 是 |
| `algorithms/Surface` | 大写 S | **否** |
| `samples/forward` | 小写 | 是 |
| `samples/starter` | 小写 | 是 |
| `samples/Deferred` | 大写 D | **否** |
| `framework/**` | 小写 | 是 |

本次重命名是**向路线图与 architecture.md 回归**，不是引入新规范。

### 5.2 步骤

Windows 文件系统大小写不敏感，只有大小写差异的 `git mv` 会被静默忽略，必须分两步：

```powershell
git mv algorithms/Surface algorithms/surface_tmp
git mv algorithms/surface_tmp algorithms/surface

git mv samples/Deferred samples/deferred_tmp
git mv samples/deferred_tmp samples/deferred
```

### 5.3 必须同步修改的引用（已全量 grep 确认）

| 文件 | 内容 |
| --- | --- |
| `CMakeLists.txt` | `add_subdirectory(algorithms/Surface)` → `algorithms/surface`；`add_subdirectory(samples/Deferred)` → `samples/deferred` |
| `samples/Deferred/Deferred.h` | `#include <algorithms/Surface/DepthRenderer.h>` → `<algorithms/surface/...>` |
| `samples/Deferred/DepthPreview.h` | 注释中的 `algorithms/Surface` |
| `prism.ps1` | `Resolve-Target` 的正则 `'/samples/Deferred/'` → `'/samples/deferred/'`（大小写敏感） |
| `README.md` | `algorithms/Surface` → `algorithms/surface` |

### 5.4 不需要改的

- **CMake target 名**（`PrismSurface`、`PrismDeferred`）：PascalCase 正确，且 `PrismSurface` 出现在
  shader 虚拟路径 `prism/PrismSurface/SurfaceDepth.hlsl` 中，改名会使全部 shader 路径失效。
- **`namespace Prism::Surface`**：保留。路线图 §4.2 的示例使用 `namespace Prism::Shadows`，即
  "一族一命名空间"。（另记：`algorithms/shadows` 目前的类型在裸 `namespace Prism` 中，是另一种
  先例，将来统一时应向 `Prism::Shadows` 靠。）
- **文档中的 `PrismDeferred`**：那是 Sample 名而非目录名。

### 5.5 建议顺手补的两处文档缺口

| 文件 | 问题 |
| --- | --- |
| `.vscode/launch.json` | **没有 `PrismDeferred` 的调试配置**，只有 Forward / Contract / Starter。新建的 Sample 无法在 VS Code 中直接 F5 |
| `docs/architecture.md` | "Samples and presentation" 一节列出了 Starter / Forward / Contract / DonutTriangle，**漏了 PrismDeferred** |

---

## 6. 命名决策

### 6.1 `Surface` 这个名字是否合适

**合适，而且有比"路线图这么写"更强的理由。** 框架里 `Surface` 已经有了确定含义：

```cpp
framework/scene/SurfaceData.h
  FSurfaceGeometry        // WorldPosition / WorldNormal / UnjitteredUv / DeviceDepth / bIsBackground
  FSurfaceShading         // BaseColor / PerceptualRoughness / Metalness / Emissive

framework/scene/SceneSurfacePipeline.h
  ESurfaceChannel         // Depth, NormalRoughness, BaseColorMetalness, Emissive,
                          // Motion, InstanceId, MaterialId
  FSceneSurfaceData
  ISceneSurfacePipeline   // "Implemented by the application"
```

即 `Surface` = "Depth + Normal/Roughness + BaseColor/Metalness + Emissive + Motion + IDs"这一整套
表面数据。所以最准确的表述是：

> **`algorithms/surface/` 就是 `Prism::Pipeline::ISceneSurfacePipeline` 的生产侧实现。**

Depth prepass、GBuffer、Position Reconstruction 正好是它的三个 Pass，与 §8 的 Pass 顺序一致。
名字完全自洽。

### 6.2 是否应新建 `depth/` 子目录

**不应新建。** 四条理由：

1. `docs/architecture.md` 明确规定："Small modules keep sources beside their headers… **Do not add
   empty layers.**"
2. 路线图 §4.2 给出的族布局是**扁平**的：shadows 的示例为 `ShadowRenderer.*` +
   `shaders/{ShadowDepth.hlsl, ShadowResolve.hlsl, PCF.hlsli, PCSS.hlsli}`，即"族 = 目录，族内多个
   技术平铺"。surface 应同构。
3. depth 与 gbuffer **共享同一份几何输入契约和同一个深度约定**。拆开后借用视图要么放到第三处
   （`algorithms/surface/common/`，即真正的空壳层），要么让 `gbuffer/` 去 include `depth/`，
   形成不该有的依赖方向。
4. **depth prepass 没有独立持久状态**。判据不是"概念上能否分开"，而是"子目录内是否有独立的持久
   资源和独立的 Settings 存活周期"。

"给 depth 一个身份"的正确方式是**文件前缀**，而不是目录。

### 6.3 目标目录布局

```text
algorithms/surface/
  CMakeLists.txt
  README.md
  Geometry.h                    ← 族内共享的借用几何视图（depth / gbuffer 共用）
  DepthSettings.h  DepthInputs.h  DepthOutputs.h  DepthRenderer.h/.cpp
  GBufferSettings.h GBufferInputs.h GBufferOutputs.h GBufferRenderer.h/.cpp   ← 下一步
  PositionReconstruction.h/.cpp                                              ← 再下一步
  shaders/
    SurfaceDepthCb.h    SurfaceDepth.hlsl
    SurfaceGBufferCb.h  SurfaceGBuffer.hlsl
    SurfaceCommon.hlsli         ← 以后：法线解码、深度重建、roughness 转换
```

现存的 `shaders/SurfaceDepth.hlsl` 命名已经踩在这个方案上，可视为原有意图的旁证。

### 6.4 何时才该拆分

判据：**族内出现多个各自带独立持久状态的子系统**时。

| 族 | 现在 | 将来 |
| --- | --- | --- |
| `algorithms/surface` | 扁平 | 保持扁平 |
| `algorithms/shadows` | 扁平 | §9.2.8 的 VSM 有 Page Table + Physical Page Pool + 跨帧 Cache + 失效逻辑；当 virtual 那套文件数超过约 10 个时，拆 `virtual/` 是有理由的 |

区别不在层级美感，而在"是否有独立的持久状态"。

若仍希望保留另一种可能，方案 B 为：`algorithms/surface/{Geometry.h, depth/, gbuffer/}`。优点是每个
技术自成一个 §4.2 最小布局、可整块复制当模板；代价是多一个族根文件、目录空洞、两处 `../` include。
触发条件为"族内 ≥3 个技术且各自有独立持久资源"。当前规模（depth 约 5 个文件）选方案 A。

---

## 7. `DepthProjection.h`（已删除）

事实核对（全仓库 grep）：

- `MakeReverseZProjection` 的调用点为 **0**；唯一出现处是自身定义与 `algorithms/Surface/CMakeLists.txt`
  的 SOURCES 列表。
- 它是一行转发：`MakePerspectiveProjection(FovY, Aspect, ZNear, ZFar, EDepthConvention::ReversedZ0To1)`，
  而该函数早已存在于 `framework/render/data/CameraData.h`。
- 实际使用的路径是 `FCameraController` → `FViewMatrices::Build` → `Camera.Raster.WorldToClip`。
- reversed-Z 是**可达的**：JSON 中 `camera.depthConvention = "reverse"`，以及运行时的
  `Context.Callbacks.SetPrimaryDepthConvention`。

删除理由（不止"没人用"）：

1. 零调用点，纯死代码。
2. **制造第二处真相**：投影矩阵构造规则在 `CameraData.h`，该头文件又包一层。将来改投影约定
   （例如换 infinite far plane）需要记得改两处。
3. **职责越界**：`algorithms/surface` 不该拥有"相机投影矩阵如何构造"。算法层自造投影矩阵会绕过
   `FViewMatrices::Build` 的缓存与约定登记（`SetPrimaryDepthConvention` 会请求历史重置、更新
   clear value），是隐藏的坑。
4. 删掉后 `algorithms/surface` 与"投影/相机"彻底解耦，符合 §4.2。

若确实想要"reverse-Z 便利函数"，正确位置是 `CameraData.h` 中紧挨 `MakePerspectiveProjection`；
但它依然冗余，因为该函数已接受 `EDepthConvention` 参数。

---

## 8. 执行顺序与退出条件

| # | 动作 | 风险 | 收益 | 状态 |
| --- | --- | --- | --- | --- |
| 0 | 修复 `CMakeLists.txt` 中对已删除 `DepthProjection.h` 的悬空引用 | — | 恢复 configure | **已完成** |
| 1 | 目录重命名（第 5 节清单）+ 补 `.vscode/launch.json` 与 `architecture.md` | 低（机械） | 消除半迁移状态 | 待办 |
| 2 | 删除 `DepthProjection.h` | 极低 | 去掉死代码与职责越界 | **已完成** |
| 3 | 拆 `DepthSettings/Inputs/Outputs`，改 `Initialize`/`Record` 签名（A1/A2/A3/B1/B2/B3/B6） | 中（接口变更，改一处 Sample） | 修掉空批次缺陷；解锁 GBuffer | 待办 |
| 4 | 加 `FScopedGpuScope("Surface.Depth")` 与 `FDepthOutputs.Debug`（B4/B5） | 低 | 补齐 M0 第 2、3 项 | 待办 |
| 5 | 新增 `algorithms/surface/README.md`（C1） | 无 | 满足 §6 | 待办 |
| 6 | 新增该深度链的 GPU Contract 检查（C2） | 中 | 把"人眼验证"换成"机器验证"，为第 3 步兜底 | 待办 |
| 7 | `FDepthTargetHelper` 下沉（C3） | 低 | 三处 Sample 去重 | 待办 |

**执行约束**：第 3 步与第 6 步应当一起做。接口一改，验证必须立刻跟上，否则又会回到"靠看深度图
判断对错"的状态 —— 这正是 C2 描述的现状。

**第 3 步的退出条件**：

- configure 与 Debug/Release 构建通过；
- 空批次场景清屏后返回 Ok（第 6 步的第 4 条断言通过）；
- forward-Z 与 reverse-Z 两种约定的 Contract 检查均通过；
- `PrismDeferred` 的深度预览与改动前逐像素一致。

---

## 附录 A：新增算法检查清单

### A.1 该不该进 `algorithms/`

三个问题，任一为"是"即应进入：

1. 是否有独立的图形学输入输出，能被至少两个 Sample 复用？
2. 是否有跨帧状态或内部持久资源（History、Probe Grid、Page Pool、BLAS）？
3. 是否有可测试的数值语义（公式、数据打包、采样分布、状态迁移）？

三者皆否 → 放 Sample 或 `framework/tools`。不要为目录整齐把演示代码下沉。

### A.2 目录与文件

```text
algorithms/<family>/
  CMakeLists.txt
  <Name>Settings.h          ← 纯 POD 参数
  <Name>Inputs.h / Outputs.h
  <Name>Renderer.h / .cpp
  <Name>Debug.h             ← 可选
  README.md                 ← §6 强制
  shaders/
    <Name>Cb.h              ← CPU/HLSL 共享布局
    <Name>.hlsl
    <Core>.hlsli            ← 可复用核心
```

### A.3 接口规则

- `Initialize(nvrhi::IDevice*, Gpu::FShaderLibrary&)` 或 `Initialize(Gpu::FRenderServices&)`；
  `FShaderLibrary` 必须比 Pass 活得久。
- `Record(Commands, Inputs, Settings, Outputs) → FStatus`。
- Pass 成员**不可移动/拷贝**（`FShaderPass` 已删除拷贝），必须作为算法类成员，**不得放入
  `std::vector`**。
- 失败一律返回 `FStatus`（`[[nodiscard]]`）；不抛异常、不静默。
- 绝不 `Submit()` 或 `WaitForIdle()` —— 命令列表归宿主。
- 禁止包含 `ExperimentContext`、ImGui、窗口、命令行、演示场景；禁止依赖 Donut 类型。

### A.4 Inputs/Outputs 必须声明的元信息（§5）

分辨率与有效矩形；格式与颜色空间；Depth Convention；Normal Space 与编码；Roughness Encoding；
Motion 单位及是否含 Jitter；资源归属与有效期；History 所属 ViewId；需要响应的 History Reset Reason。

写成字段并交给校验函数，不要只留在注释里。

### A.5 CMake 与 Shader

- `prism_add_target(<Target> SOURCES … SHADERS <file>.hlsl:<stage>[:<entry>] INCLUDES shaders/)`。
- 一个 target 一个 shader 包，包名 = target 名 → 运行时路径 `prism/<Target>/<file>.hlsl`。
- 同一 target 内文件名必须唯一（ShaderMake 会扁平化路径）。
- Shader 首行 `#include "Prism/Common/Platform.hlsli"`；矩阵用行向量
  `mul(float4(pos,1), ObjectToClip)`。
- `*Cb.h` 用 `#ifndef` guard，同时被 C++ 与 HLSL include；CPU 侧加 `static_assert(sizeof(...))`。
- 改寄存器、常量布局、输入输出签名或 thread group 尺寸会使 F6 热重载被拒绝 —— 必须完整重建。

### A.6 调试、计时与验证

- `DebugViews->Publish(Group, Name, Texture, Settings)`；
- `Comparison->Publish(Id, {Texture, EColorSpace})`；
- `Gpu::FScopedGpuScope Scope(*Profiler, Commands, "Family.Pass")`；
- CPU Test（`tests/cpu`，无窗口无设备）、GPU Contract（`tests/gpu/contract` 范式）、图像回归
  （参考图保存在显示变换之前）。

---

## 附录 B：新增 Sample 检查清单

### B.1 必做

1. 继承 `Host::IExperiment`，实现 `GetName()`、`Initialize()`、`Render()`；
2. 文件末尾定义 `Prism::Host::CreateExperiment()` 工厂；
3. `GetName()` 的返回值必须与 JSON 中 `experiments.<名字>` 段名一致。

可选重写：`GetDescription`、`BeginFrame`（读回与数值校验，此时可安全等待 GPU）、`BuildUI`、
`OnKey`、`OnResize`、`PassedVerification`、`Shutdown`。

`Shutdown` 在 `Initialize` 失败时也会被调用一次（部分初始化状态），必须防御性实现。

### B.2 CMake 与预设

```cmake
prism_add_target(PrismXxx
    KIND EXECUTABLE
    LINK <算法库> prism_donut_scene
    SOURCES Xxx.h Xxx.cpp XxxCb.h
    SHADERS Xxx.hlsl:ps
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}"
    CONFIG presets/default.json)
```

随后：根 `CMakeLists.txt` 加 `add_subdirectory`；`CMakePresets.json` 的 buildPresets `targets`
加入该 target。`CONFIG` 文件会被复制为 `<可执行文件名>.json` 作为默认配置，`--config` 可覆盖。

### B.3 每帧顺序模板

```cpp
// Initialize
//   ① 校验 Context.Gpu 服务指针
//   ② 声明 FTextureRequest（必须是成员变量：默认构造会分配新 ResourceId）
//   ③ Context.Output.ColorSpace 声明输出所处阶段
//   ④ 创建常量缓冲、binding layout、pass
//   ⑤ 读取自己的参数段

// Render
nvrhi::ITexture* Output = Context.Gpu.Targets->GetOrCreate(Request);
nvrhi::IFramebuffer* Target = Context.Gpu.Targets->GetFramebuffer(Output);
Constants.Write(Frame.Commands, Data);
auto Set = Pass.GetOrCreateBindingSet(Bindings, Layout);
const FStatus Status = Pass.Record(Frame.Commands, Target, {Set});
//   发布调试视图与比较结果
return Status ? Output : nullptr;

// OnResize
Pass.ClearBindings();
```

### B.4 四个易踩的坑

1. **`FTextureRequest` 不能是局部变量** —— 默认构造会分配新 `ResourceId`，每帧新建纹理。
2. **尺寸变化后必须 `ClearBindings()`** —— `TextureCache::SetRenderSize` 会释放跟随分辨率的纹理，
   而 binding set 里还留着旧指针。
3. **`Initialize` 失败后 `Shutdown` 仍会被调用一次**，成员必须能在半初始化状态下安全析构。
4. **不要在 `Render` 里 `Submit` / `WaitForIdle`**；需要等待的 CPU 工作放 `BeginFrame`。

### B.5 参数面板

`Host::FParamTable` 用一份描述符表同时驱动 ImGui、JSON 与参数 hash：

```cpp
inline const Prism::Host::FParamDesc KXxxParams[] = {
    PRISM_PARAM_INT(FSettings, Mode, "Mode", 0, 3, Prism::Host::EParamFlags::None),
    PRISM_PARAM_FLOAT(FSettings, Radius, "Radius (m)", 0.0001f, 0.5f,
                      Prism::Host::EParamFlags::HistoryInvalidating),
};
```

标注 `HistoryInvalidating` 的字段参与 `ComputeHash()`，用于"参数改了才重置历史"。

---

## 主要参考

- `docs/rendering-study-roadmap.md` §2、§4.2、§4.3、§5、§6、§7、§8、§9.2.8、§24
- `docs/architecture.md`（目录规范、ownership 表）
- `docs/coding-style.md`（Tab 缩进、控制流一律加花括号）
- `docs/framework-experiments.md`（Pass 用法、Replay 的逻辑时钟要求、验证命令）
- `tests/gpu/contract/`（GPU Contract 范式）
