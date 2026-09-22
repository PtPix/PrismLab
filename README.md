# Prism

D3D12 图形学实验平台。仓库分成两半：`algorithms/` 是解耦的算法核心（HLSL 只通过 `PRISM_*`
桥接函数接触宿主），`framework/` 是全部基于 Donut / NVRHI 的设施（类型、执行、适配、管线、
宿主）。写一个新实验只需要实现一个 Experiment 子类和它的 shader：窗口、设备、相机、场景、UI、
GPU 计时、配置、截图、清理顺序都由框架提供。

研究方向见 `E:/UE_5.8/RenderLabDocs/RenderLab_Architecture_and_Research_Roadmap.md`。

## 目录

```text
external/Donut-Samples/   官方仓库子模块，固定提交（Donut / NVRHI / glfw / imgui / cgltf）
algorithms/shaders/       解耦核心：可复用 HLSL 算法（只通过 PRISM_* 桥接函数接触宿主）
framework/types/          算法消费的数据类型（算法与框架共享，直接用 donut::math）
framework/nvrhi/          执行设施：shader 变体、命名瞬态资源、GPU 计时、读回、绘制辅助
framework/donut/          Donut 设施：配置、相机、场景源、光源与几何转换
framework/pipelines/      管线接线：可复用 Pass（共享前向场景路径）
framework/host/           应用宿主：窗口、设备、相机、场景、UI、帧循环、命令行、WinMain
samples/                  实验：forward、contract、starter（每个实验自带 config.json）
prism.ps1                 唯一构建 / 运行入口（找 cmake、按需 configure、构建、启动）
```

`algorithms/` 与 `framework/` 的边界是项目里唯一的解耦点：算法 shader 只能通过 `PRISM_*`
桥接函数与宿主交互，换宿主时只重写桥接层；`framework/` 的 C++ 直接用 `donut::math` 与
Donut / NVRHI 设施，不假装与引擎无关。

Donut-Samples 的 `donut` 是嵌套子模块，NVRHI 等由它继续管理。主仓库只保存子模块提交引用，
不复制第三方源码，也不修改官方示例。默认不初始化大型 glTF 示例资产，不构建官方示例。

## 环境

- Windows，支持 D3D12 的显卡与驱动。
- Visual Studio 2022，安装“使用 C++ 的桌面开发”和 Windows SDK。
- CMake 3.24 或以上、Git；从 VS Developer PowerShell 操作可直接使用 VS 附带的 CMake。
- 首次配置需要联网，由依赖的 CMake 脚本下载 DXC 和 DirectX-Headers 等构建依赖，下载物位于忽略的 `build/` 中。

## 获取依赖

从自己的远端克隆后，在仓库根目录执行以下两步。不要使用顶层 `--recursive` 克隆，否则也会递归获取上游的大型示例资产。

```powershell
git submodule update --init external/Donut-Samples
git -C external/Donut-Samples submodule update --init --recursive donut
```

## 构建与运行

```powershell
cmake --preset my-project
cmake --build --preset my-project --parallel

.\build\my-project\bin\PrismForward.exe    # 共享前向管线 + 实验自己的调试视图 Pass
.\build\my-project\bin\PrismContract.exe   # 契约自检（矩阵/深度约定、CPU 与 GPU 一致性）
.\build\my-project\bin\PrismStarter.exe    # 不依赖框架的最小三角形示例
```

也可以用 Visual Studio 打开根目录或生成的 `build/my-project/Prism.sln`，启动目标是
`PrismForward`。Debug 启用 NVRHI 校验与可用的 D3D12 调试运行时；运行包需要保留 EXE 旁的
`shaders/` 目录。

### 一键构建 + 运行

`prism.ps1` 是唯一入口（查找 cmake、按需 configure preset、构建、启动程序），
编辑器里的运行任务、F5 调试与 Code Runner 都调用它；宿主参数用 `-AppArgs` 原样转发：

```powershell
# 终端：构建并启动实验
powershell -NoProfile -File prism.ps1 -Target PrismForward

# 无头运行：跑 30 帧后退出，失败返回非零退出码，并回显日志尾部
powershell -NoProfile -File prism.ps1 -Target PrismContract -AppArgs --smoke-test=30 -TailLog

# 性能测量：预热 30 帧后测量 120 帧，写出指标 CSV
powershell -NoProfile -File prism.ps1 -Target PrismForward -AppArgs --bench=120 --metrics forward_bench.csv

# 只构建（等价于 CMake preset 的 Debug / Release 变体）
cmake --build --preset my-project-debug
cmake --build --preset my-project
```

编辑器集成：

* **运行任务**：`.vscode/tasks.json` 提供配置、Debug/Release 构建、三个实验的运行、契约自检与基准测量；`Ctrl+Shift+B` 直接执行默认构建。
* **F5 调试**：`.vscode/launch.json` 提供 ForwardExperiment / ContractExperiment / Starter 及冒烟测试、截图等配置，启动前自动构建。
* **Code Runner 扩展**：`.vscode/settings.json` 已配置 `code-runner.executorMap`，在实验源码上按 Run Code 会构建并运行对应程序（需要安装第三方扩展 `formulahendry.code-runner`）。

不指定 `-Target` 时按源文件推断：`samples/forward` → `PrismForward`，
`samples/contract` → `PrismContract`，`samples/starter` → `PrismStarter`，
框架代码（`framework/`、`algorithms/`）默认 `PrismForward`。

### 命令行

```powershell
--config <path>           指定 JSON 配置文件（默认是实验自己的 samples/<name>/config.json）
--scene <source>          覆盖场景来源：procedural | gltf
--asset <path>            覆盖场景资产（场景 .json、.gltf 或 .glb）
--smoke-test[=N]          渲染 N 帧（默认 3）后退出，用于验证构建与初始化
--capture <path>          截图后退出（配合 --capture-frame，默认第 2 帧）
--write-reference <path>  把该帧输出写成浮点参考图（.f32）后退出
--reference <path>        与该帧输出和浮点参考图比较，超差时进程返回非零
--tolerance <v>           参考图比较容差，默认 0.01
--bench[=N]               预热后测量 N 帧（默认 120），写出指标 CSV 后退出
--bench-warmup[=N]        基准的预热帧数，默认 30
--metrics <path>          指标 CSV 路径（配合 --bench / --smoke-test）
--debug-view <n>          显示第 n 个登记的中间结果（0 = 实验输出）
--width/--height <n>      窗口尺寸
--no-vsync                关闭垂直同步
--no-timing               关闭 GPU 时间戳查询
--help                    打印用法
```

示例：

```powershell
# 建立浮点基线，然后回归比较（数值判定，失败返回非零）
.\PrismForward.exe --write-reference forward_ref.f32 --capture-frame 4
.\PrismForward.exe --reference forward_ref.f32 --capture-frame 4 --tolerance 0.0001

# 预热 30 帧后测量 120 帧，逐帧指标 + 末尾 mean/min/max 汇总
.\PrismForward.exe --bench=120 --bench-warmup=30 --metrics forward_bench.csv

# 给第 2 个中间结果（场景深度）截图，不需要手点面板
.\PrismForward.exe --debug-view 2 --capture depth.png --capture-frame 4
```

`--smoke-test` / `--bench` / `--capture` / `--reference` 这类无人值守运行会把日志写到 EXE 旁的
`prism.log`（交互运行时日志显示在应用内控制台）。自检、参考图比较失败时进程返回非零退出码，
可直接用于 CI。

### 场景

默认是程序化测试场景（无外部资产，5 个网格 / 8 个实例 / 2 个光源）。加载 glTF 需要先取回媒体资产：

```powershell
git -C external/Donut-Samples submodule update --init media
.\build\my-project\bin\PrismForward.exe --scene gltf --asset media/sponza-plus.scene.json
```

资产路径支持相对仓库根或绝对路径，程序会自可执行文件目录向上查找。

## 写一个新实验

1. `samples/<name>/` 下放 `xxx.h/.cpp`、自己的 `*.hlsl`、`config.json`、`CMakeLists.txt`；
   `shaders.cfg` 由构建自动生成，不需要手写。
2. 继承 `prism::host::Experiment`：`Initialize` 里用 `context.gpu.resources->Get(slot)` 声明资源、
   建 PSO，`Render` 录制 Pass，`BuildUI` 里 `m_Params.BuildUI()`；需要读回或数值验证时用
   `BeginFrame`。
3. 定义工厂 `std::unique_ptr<prism::host::Experiment> prism::host::CreateExperiment()`。
4. CMakeLists 写一次
   `prism_add_target(<target> KIND EXECUTABLE SOURCES ... SHADERS <file.hlsl>:<stage> CONFIG config.json)`：
   框架库、shader 编译（entry point 约定为 `main_<stage>`）、依赖关系与默认配置都由它处理；
   顶层只需 `add_subdirectory(samples/<name>)`。

`samples/forward` 是最小完整例子（含自己的全屏 Pass、参数表和调试视图发布）；
`samples/contract` 展示了 GPU/CPU 数值自检的写法。shader 第一行要包含
`Prism/Common/Platform.hlsli`（矩阵行主序、深度与颜色约定）。

## 连接自己的 GitHub 仓库

本地仓库使用 `main` 分支。创建一个空 GitHub 仓库后，替换下面的地址：

```powershell
git remote add origin https://github.com/YOUR_NAME/YOUR_REPOSITORY.git
git push -u origin main
```

不要把 `external/Donut-Samples` 的官方 origin 改成自己的主仓库。升级依赖时检查并提交主仓库中的子模块引用变化。

## 来源

`framework/host` 基于 Donut-Samples 的 `examples/basic_triangle` 与 Donut 的 device/UI 框架改写，
保留其版权与 MIT 许可声明。第三方依赖遵循各自许可，详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

## 两种构建选项

项目只提供两个公开 Preset，配置和构建使用相同的名字：

```powershell
# 只生成和构建 Prism 自己的目标
cmake --preset my-project
cmake --build --preset my-project --parallel

# 生成和构建 Prism 以及当前 D3D12 配置适用的全部 Donut 示例
cmake --preset all-projects
cmake --build --preset all-projects --parallel
```

全部程序输出到 `build/all-projects/bin/`。例如：

```powershell
.\build\all-projects\bin\PrismForward.exe
.\build\all-projects\bin\basic_triangle.exe --dx12
.\build\all-projects\bin\feature_demo.exe --dx12
```

`all-projects` 包含 Prism、Feature Demo、普通光栅示例、光追示例、Meshlet、异步计算、线程渲染、Work Graphs 和光追反射。Vulkan 专属的 `shader_specializations` 与需要额外 Aftermath SDK 的示例不属于当前 D3D12 配置。部分程序运行时还要求支持对应 GPU 功能，Feature Demo 和场景类示例可能需要上游媒体资产。

CMake Preset 负责配置和构建，不能安全地同时启动十几个窗口程序；需要测试哪个示例，直接运行 `bin` 中对应的 EXE。
