# PrismLab

D3D12 图形学实验平台。仓库提供一套分层框架（契约 / 算法 / 执行 / 适配 / 管线 / 宿主），
写一个新实验只需要实现一个 Lab 子类和它的 shader：窗口、设备、相机、场景、UI、GPU 计时、
配置、截图、清理顺序都由宿主提供。

架构与分层规则见 [docs/architecture.md](docs/architecture.md)，研究方向见
`E:/UE_5.8/RenderLabDocs/RenderLab_Architecture_and_Research_Roadmap.md`。

## 目录

```text
external/Donut-Samples/   官方仓库子模块，固定提交（Donut / NVRHI / glfw / imgui / cgltf）
include/renderlab/        契约层：参数与语义，无 Donut 场景类型、无 NVRHI 对象
algorithms/shaders/       算法层：可复用 HLSL 核心（用桥接函数与宿主解耦）
backends/nvrhi/common/    执行层：shader 变体、命名瞬态资源、GPU 计时、读回、绘制辅助
adapters/donut/           适配层：配置、相机、场景源、光源与几何转换
pipelines/                管线层：可复用 Pass 接线（共享前向场景路径）
host/                     宿主层：窗口、设备、相机、场景、UI、帧循环、命令行、WinMain
samples/                  实验：lab_forward、lab_contract，以及最小的 starter
configs/host/             宿主与实验的 JSON 配置
docs/                     架构与依赖记录
```

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

.\build\my-project\bin\PrismLabForward.exe    # 共享前向管线 + 实验自己的调试视图 Pass
.\build\my-project\bin\PrismLabContract.exe   # 契约自检（矩阵/深度约定、CPU 与 GPU 一致性）
.\build\my-project\bin\PrismLabStarter.exe    # 不依赖框架的最小三角形示例
```

也可以用 Visual Studio 打开根目录或生成的 `build/my-project/PrismLab.sln`，启动目标是
`PrismLabForward`。Debug 启用 NVRHI 校验与可用的 D3D12 调试运行时；运行包需要保留 EXE 旁的
`shaders/` 目录。

### 命令行

```powershell
--config <path>        指定 JSON 配置文件（默认 configs/host/camera_default.json）
--scene <source>       覆盖场景来源：procedural | gltf
--asset <path>         覆盖场景资产（场景 .json、.gltf 或 .glb）
--smoke-test[=N]       渲染 N 帧（默认 3）后退出，用于验证构建与初始化
--capture <path>       截图后退出（配合 --capture-frame，默认第 2 帧）
--width/--height <n>   窗口尺寸
--no-vsync             关闭垂直同步
--no-timing            关闭 GPU 时间戳查询
--help                 打印用法
```

`--smoke-test` 与 `--capture` 会把日志写到 EXE 旁的 `renderlab.log`（交互运行时日志显示在应用内
控制台）。自检失败时进程返回非零退出码，可直接用于 CI。

### 场景

默认是程序化测试场景（无外部资产，5 个网格 / 8 个实例 / 2 个光源）。加载 glTF 需要先取回媒体资产：

```powershell
git -C external/Donut-Samples submodule update --init media
.\build\my-project\bin\PrismLabForward.exe --scene gltf --asset media/sponza-plus.scene.json
```

资产路径支持相对仓库根或绝对路径，程序会自可执行文件目录向上查找。

## 写一个新实验

1. `samples/lab_<name>/` 下放 `xxx_lab.h/.cpp`、自己的 `*.hlsl`、`shaders.cfg`、`CMakeLists.txt`。
2. 继承 `renderlab::host::Lab`：`Initialize` 建 shader/PSO/绑定并声明渲染目标，`Render` 录制 Pass，
   `BuildUI` 画参数；需要读回或数值验证时用 `BeginFrame`。
3. 定义工厂 `std::unique_ptr<renderlab::host::Lab> renderlab::host::CreateLab()`，链上 `rl_host` 即可，
   不需要 `WinMain`。
4. 在顶层 `CMakeLists.txt` 加 `add_subdirectory(samples/lab_<name>)`。

`samples/lab_forward` 是最小完整例子（约 200 行，含自己的全屏 Pass 与调试视图）；
`samples/lab_contract` 展示了 GPU/CPU 数值自检的写法。shader 里第一行要包含
`RenderLab/Common/Platform.hlsli`（矩阵行主序、深度与颜色约定）。

## 连接自己的 GitHub 仓库

本地仓库使用 `main` 分支。创建一个空 GitHub 仓库后，替换下面的地址：

```powershell
git remote add origin https://github.com/YOUR_NAME/YOUR_REPOSITORY.git
git push -u origin main
```

不要把 `external/Donut-Samples` 的官方 origin 改成自己的主仓库。升级依赖时检查并提交主仓库中的子模块引用变化。

## 来源

host 层基于 Donut-Samples 的 `examples/basic_triangle` 与 Donut 的 device/UI 框架改写，
保留其版权与 MIT 许可声明。第三方依赖遵循各自许可，详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

## 两种构建选项

项目只提供两个公开 Preset，配置和构建使用相同的名字：

```powershell
# 只生成和构建 PrismLab 自己的目标
cmake --preset my-project
cmake --build --preset my-project --parallel

# 生成和构建 PrismLab 以及当前 D3D12 配置适用的全部 Donut 示例
cmake --preset all-projects
cmake --build --preset all-projects --parallel
```

全部程序输出到 `build/all-projects/bin/`。例如：

```powershell
.\build\all-projects\bin\PrismLabForward.exe
.\build\all-projects\bin\basic_triangle.exe --dx12
.\build\all-projects\bin\feature_demo.exe --dx12
```

`all-projects` 包含 PrismLab、Feature Demo、普通光栅示例、光追示例、Meshlet、异步计算、线程渲染、Work Graphs 和光追反射。Vulkan 专属的 `shader_specializations` 与需要额外 Aftermath SDK 的示例不属于当前 D3D12 配置。部分程序运行时还要求支持对应 GPU 功能，Feature Demo 和场景类示例可能需要上游媒体资产。

CMake Preset 负责配置和构建，不能安全地同时启动十几个窗口程序；需要测试哪个示例，直接运行 `bin` 中对应的 EXE。
