# PrismLab 架构

本文描述仓库当前的分层、每层的职责与依赖规则、实验（Lab）需要实现的最小接口，以及
"只写算法" 这条要求在代码里的落点。路线图与研究方向见仓库外的
`RenderLab_Architecture_and_Research_Roadmap.md`（UE_5.8/RenderLabDocs）。

## 1. 目标与约束

* **算法优先**：写一个新实验时，只写 shader、参数和 Pass 录制逻辑；相机、场景、窗口、
  UI、计时、配置、截图都不重复实现。
* **算法可迁移**：算法核心与宿主、引擎、PSO 解耦，换宿主时只重写适配层。
* **不静默替换算法**：管线里的每一级要么是实验自己的算法，要么是公开可替换的基线。
* **可验证**：约定（矩阵、深度、颜色）与数值结果必须能被自动检查，而不是靠肉眼。

## 2. 分层与目录

```text
include/renderlab/contracts/    契约层：参数与语义（无 Donut 场景类型、无 NVRHI 对象、无 UI）
algorithms/shaders/             算法层：可复用的 HLSL 核心（桥接函数与宿主解耦）
backends/nvrhi/common/          执行层：shader 变体、命名瞬态资源、GPU 计时、读回、绘制辅助
adapters/donut/                 Donut 适配层：配置、相机、场景源、光源/几何转换
pipelines/                      管线层：可复用的 Pass 接线（当前：共享前向场景路径）
host/                           宿主层：窗口、设备、相机、场景、UI、帧循环、命令行、WinMain
samples/                        实验：每个实验 = 一个 Lab 子类 + shader + 一个 CreateLab 工厂
configs/host/                   宿主与实验的 JSON 配置
```

依赖方向（单向，不允许回指）：

```text
contracts  <-- algorithms
contracts  <-- backends/nvrhi/common  <-- adapters/donut
contracts  <-- pipelines (Donut 渲染 Pass)
上面全部  <-- host  <-- samples
```

对应 CMake 目标：`rl_contracts`、`rl_algorithms`、`rl_nvrhi_common`、`rl_adapter_donut`、
`rl_pipelines`、`rl_host`。

## 3. 数据契约

契约层是唯一定义"这些东西到底是什么"的地方，文件在 `include/renderlab/contracts/`。

| 文件 | 内容 |
| --- | --- |
| `Conventions.h` | 世界/视图空间、深度、UV、颜色、矩阵、粗糙度、可见性约定与深度换算 |
| `CameraData.h` | 视图/投影矩阵（当前帧与上一帧）、抖动、世界位置重建 |
| `FrameInfo.h` | 帧序号、时间、分辨率、抖动、历史重置原因 |
| `LightData.h` | 光源记录 + 固定对齐的 GPU 结构（`static_assert` 校验大小与偏移） |
| `SurfaceData.h` | GBuffer 语义（法线空间、粗糙度编码、运动矢量）与 CPU 侧表面采样 |
| `ShadowSettings.h` | 阴影功能的输入/输出契约（M2 的落点） |
| `PixelFormat.h` / `Status.h` | 与图形 API 无关的像素格式；不抛异常的错误返回 |

关键约定（`Conventions.h` 有完整说明）：

```text
世界空间  右手系、Y 轴向上、单位米
视图空间  左手系，D3D 风格投影，z ∈ [0, 1]
深度      forward-Z：近 0 远 1，清空值 1.0，比较 Less
矩阵      HLSL 用行向量写法 mul(v, M)，CPU 侧 dm::float4x4 原样上传
          worldToClip = worldToView * viewToClip，clipToWorld = inverse(worldToClip)
颜色      线性 HDR，shader 不做隐式 gamma
粗糙度    r 是感知粗糙度，GGX alpha = r * r，平方只做一次
```

HLSL 侧必须包含 `algorithms/shaders/RenderLab/Common/Platform.hlsli`，它负责
`#pragma pack_matrix(row_major)`（矩阵打包与 CPU 一致）并复述上述约定。

## 4. 实验接口

一个实验只需要实现 `renderlab::host::Lab`（`host/Lab.h`）：

```cpp
class Lab
{
public:
    virtual const char* GetName() const = 0;
    virtual const char* GetDescription() const;
    virtual Status Initialize(LabContext& context) = 0;        // 建 shader / PSO / 绑定 / 渲染目标
    virtual void BeginFrame(LabContext&, const LabFrame&);      // 读回与验证（命令列表未打开）
    virtual nvrhi::ITexture* Render(LabContext&, const LabFrame&) = 0; // 录制本帧 GPU 工作，返回显示纹理
    virtual void BuildUI(LabContext& context);                  // 只画自己的参数
    virtual bool OnKey(LabContext&, int key, int action, int mods);
    virtual void OnResize(LabContext&, const Extent2D&, const Extent2D&);
    virtual bool PassedVerification() const;                    // 冒烟测试/CI 的退出码
};
```

宿主提供的东西（`LabContext` / `LabFrame`）：

* 设备、shader 工厂、公共 Pass、`gpu::ShaderLibrary`、`gpu::RenderTargetPool`、`gpu::GpuProfiler`
* 共享场景（`SceneData`：Donut 图 + 光源记录 + 几何批次 + 材质）、共享前向管线
* 每帧的 `FrameInfo` 与 `CameraData`（含上一帧矩阵与抖动）、视图对象、渲染/输出分辨率
* 回调：请求历史重置、保存纹理、请求退出

实验**不**做的事：创建窗口/设备/交换链、创建相机、构建场景、写 ImGui 框架、写 GPU 计时表、
管理命令列表与提交、处理窗口缩放、解析命令行、退出清理顺序。

## 5. 执行层

`backends/nvrhi/common/`：

* `ShaderLibrary`：按 (路径, 入口, 类型, 宏) 缓存 shader 变体，shader 热重载时清缓存。
* `RenderTargetPool`：按名字声明瞬态纹理（格式/用法/尺寸），尺寸变化自动重建，缓存 framebuffer。
* `GpuProfiler` + `ScopedGpuScope`：分 Pass 的 GPU 时间戳（每槽位一个查询，结果可用后才复用），
  同时写 PIX/NSight 调试标记。
* `TextureReadback`：`SaveTextureToImage`（人工查看）与 `ReadTexture`（数值验证）。
* `PipelineUtils`：compute/fullscreen 管线创建、全屏四边形绘制、`MakeFullViewportState`。
* `GeometryBatch`：不含 Donut 类型的几何批次视图（缓冲组 + 绘制记录 + 世界包围盒），
  供阴影图、深度预pass、GBuffer 这类自绘 Pass 使用。

两条来自实战的硬性注意点：

* Donut 的全屏顶点着色器输出 **4 个顶点**（`SV_VertexID` 0..3），配套 `TriangleStrip`；
  用 3 个顶点或 `TriangleList` 只会画半个屏幕。
* 用池里的纹理创建 staging 纹理做读回时，不要清掉 `isShaderResource` 之类的标志，
  否则 D3D12 后端会在创建 staging 时崩溃（`TextureReadback.cpp` 有注释说明）。

## 6. 平台适配与桥接

Donut 只出现在 `adapters/donut` 与 `pipelines/`：`adapters/donut` 把
"配置 JSON、Donut 相机、Donut 场景图、Donut 光源" 转成契约数据；
`pipelines/SceneForwardPipeline` 把 Donut 的前向着色封装成"场景 → 纹理"的一条调用。

算法核心用桥接函数与宿主解耦，例如 `algorithms/shaders/Shadows/PCF.hlsli` 只依赖：

```hlsl
float RL_LoadShadowDepth(int2 texelCoord);
float RL_ShadowDepthFromWorld(float3 worldPosition);
bool  RL_IsInsideShadowMap(int2 texelCoord);
```

换阴影图布局、换滤波核或换宿主时只重写这三个函数，PCF 逻辑不动。

## 7. 宿主

`host/` 负责把一切装配起来并在退出时按图形 API 要求的顺序拆掉：

```text
Application.cpp   设备/交换链、shader 挂载、服务装配、场景加载、消息循环、清理顺序
LabHost.cpp       每帧状态（FrameInfo/CameraData/抖动/历史重置）、命令列表、blit 到交换链、
                  截图、冒烟测试、GPU 计时作用域
UiOverlay.cpp     宿主机信息 + 实验参数 + GPU 计时表 + 日志控制台
CommandLine.cpp   --config/--scene/--asset/--smoke-test/--capture/--width/--height/--no-vsync/--no-timing
Entry.cpp         共用的 WinMain（实验只提供 CreateLab()）
```

日志在交互运行时进入应用内控制台；在 `--smoke-test` / `--capture` 运行时保留文件日志
（`bin/renderlab.log`），保证 CI 能看到初始化与自检输出。

## 8. 新增一个实验

1. `samples/lab_<name>/` 下放 `xxx_lab.h/.cpp`、`*.hlsl`（算法）、`shaders.cfg`、`CMakeLists.txt`。
2. 实现 `Lab` 子类：`Initialize` 里建 shader/PSO/绑定并声明渲染目标；`Render` 里录制 Pass；
   `BuildUI` 里画参数。
3. 提供工厂：`std::unique_ptr<renderlab::host::Lab> renderlab::host::CreateLab()`。
4. 顶层 `CMakeLists.txt` 里 `add_subdirectory(samples/lab_<name>)`。

一个最小实验的主体大致是这样（无需任何相机/场景/UI 代码）：

```cpp
Status MyLab::Initialize(host::LabContext& context)
{
    m_Color = { "MyColor", PixelFormat::RGBA16_FLOAT,
                gpu::TextureUsage::ShaderResource | gpu::TextureUsage::RenderTarget };
    context.targets->GetOrCreate(m_Color);

    nvrhi::ShaderHandle ps = context.shaders->GetShader("renderlab/my_pass.hlsl", "main_ps", nvrhi::ShaderType::Pixel);
    m_Pipeline = gpu::CreateFullScreenPipeline(context.device, { context.commonPasses->m_FullscreenVS, ps, ... });
    return Status::Ok();
}

nvrhi::ITexture* MyLab::Render(host::LabContext& context, const host::LabFrame& frame)
{
    gpu::ScopedGpuScope scope(*context.profiler, frame.commands, "My pass");
    // 需要场景时：context.scenePipeline->RenderScene(frame.commands, *context.scene->graph, *frame.view, ...);
    // 需要矩阵时：frame.camera.current.worldToClip / clipToWorld
    gpu::DrawFullScreenQuad(frame.commands, m_Pipeline, framebuffer, m_BindingSet);
    return context.targets->Find("MyColor");
}
```

已有实验：

* `samples/lab_forward`（`PrismLabForward`）：共享前向管线 + 自己的调试视图 Pass
  （设备深度 / 线性深度 / 世界位置 / 深度导数法线）。
* `samples/lab_contract`（`PrismLabContract`）：契约自检，见下一节。
* `samples/starter`（`PrismLabStarter`）：不依赖框架的最小三角形成立示例。

## 9. 验证与诊断

* **契约自检**：`PrismLabContract` 用一个 compute Pass 从深度重建世界位置与线性深度，
  读回后由 CPU 校验：矩阵往返、深度往返、世界位置投影回原像素、CPU/GPU 重建一致。
  失败时进程返回非零退出码（`Lab::PassedVerification`）。
* **截图**：`--capture <file> --capture-frame <n>`，或实验通过 `context.callbacks.saveTexture`。
* **GPU 计时**：UI 里的 Pass 表 + `--no-timing` 关闭；冒烟测试用它做回归对比。
* **日志**：所有 NVRHI/D3D12 校验信息通过消息回调进入同一份日志。

这套自检已经抓到两个真实问题（保留在这里作为"为什么要验证"的例子）：

1. 实验 shader 漏了 `#pragma pack_matrix(row_major)`，矩阵按列主序解释：
   线性深度完全正确、世界位置全错（误差 12 米）。
2. 全屏 Pass 用 3 个顶点配 Donut 的 4 顶点全屏 VS，只画了半个屏幕。

## 10. 与路线图的对应

* **M0**：宿主 + 程序化场景 + ImGui 已完成；本架构把它拆成了 `host` / `adapters/donut`。
* **M1**：契约层、shader 编译与变体、GPU 计时、契约自检已完成（`PrismLabContract`）。
* **M2 起**：每个研究方向落在同一套骨架里。
  * 阴影：`contracts/ShadowSettings.h` + `algorithms/shaders/Shadows/*` +
    `backends/nvrhi` 的阴影图资源与 Pass 接线。
  * 可见性与几何：`gpu::GeometryBatch`（阴影图、深度预pass、实例剔除）。
  * 时域（TAA/超分）：`FrameInfo` 的历史重置原因 + `CameraData` 的上一帧矩阵与抖动 +
    `PixelFormat::RG16_FLOAT` 运动矢量 + 历史纹理放在 `RenderTargetPool`。
  * 材质与着色：`SurfaceData.h` 的 GBuffer 语义 + 算法核心 HLSL。
  * 光照与全局光照：`LightData.h` 的记录与 GPU 布局 + 算法核心 HLSL。
  * 后处理与性能：全屏 Pass 框架、`GpuProfiler`、数值回归与截图对比。

## 11. 已知限制

* 后端只有 D3D12（Vulkan/DX11 的 CMake 开关保持关闭）。
* `GeometryBatch` 只覆盖不透明、无形变的三角形：alpha test、蒙皮、displacement 需要显式扩展，
  不允许悄悄退化成"渲染得不完整"。
* 场景源支持程序化与 Donut 的 glTF/场景 JSON；资产场景需要 `media` 目录存在。
* 还没有 tonemapping：线性 HDR 直接 blit 到交换链，画面会过曝（M2 之后再补显示变换）。
* 历史纹理与滚动索引由实验自己管理；宿主只提供重置原因与上一帧矩阵。
