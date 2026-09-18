# PrismLab

用于图形学实验的最小 Donut + NVRHI 起步工程。当前仅包含一个 D3D12 彩色三角形、自己的 C++ 入口与 HLSL Shader。

## 目录

```text
external/Donut-Samples/  官方仓库子模块，固定提交
samples/starter/        自己的示例、Shader 与构建文件
CMakeLists.txt          仅引入 Donut 库及自己的示例
CMakePresets.json       Windows x64 构建配置
```

Donut-Samples 的 `donut` 是嵌套子模块，NVRHI 等由它继续管理。主仓库只保存子模块提交引用，不复制第三方源码，也不修改官方示例。默认不初始化大型 glTF 示例资产，不构建 feature_demo 或其他官方示例。

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
.\build\my-project\bin\PrismLabStarter.exe
```

也可用 Visual Studio 打开根目录或生成的 `build/my-project/PrismLab.sln`，启动目标是 `PrismLabStarter`。

程序显示彩色三角形，支持调整窗口大小。`--smoke-test` 会在绘制三帧后自动退出，用于检查设备、Shader 加载和绘制流程。Shader 相对可执行文件定位，无需指定工作目录。

```powershell
.\build\my-project\bin\PrismLabStarter.exe --smoke-test
cmake --build build/my-project --config Debug --target PrismLabStarter --parallel
```

Debug 启用 NVRHI 校验与可用的 D3D12 调试运行时。Release/Debug 共用输出目录，切换后先构建所需配置。运行包需要保留 EXE 旁的 `shaders/` 目录。

## 开始实验

先修改 `samples/starter/triangle.hlsl` 或 `main.cpp`，重新构建即可。需要第二个 Demo 时新增 `samples/<name>/` 及其 CMake 目标；出现真实复用需求后再提取算法库或 Pass 类。

初期不预置空框架、不包含模型、设计长文或生成产物。构建输出统一在 `build/`；个人配置使用被忽略的 `CMakeUserPresets.json`。

## 连接自己的 GitHub 仓库

本地仓库使用 `main` 分支。创建一个空 GitHub 仓库后，替换下面的地址：

```powershell
git remote add origin https://github.com/YOUR_NAME/YOUR_REPOSITORY.git
git push -u origin main
```

不要把 `external/Donut-Samples` 的官方 origin 改成自己的主仓库。升级依赖时检查并提交主仓库中的子模块引用变化。

## 来源

三角形起步代码基于 NVIDIA Donut-Samples 的 `examples/basic_triangle` 修改，保留其版权与 MIT 许可声明。第三方依赖遵循各自许可，详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

## 两种构建选项

项目只提供两个公开 Preset，配置和构建使用相同的名字：

```powershell
# 只生成和构建自己的 PrismLabStarter
cmake --preset my-project
cmake --build --preset my-project --parallel

# 生成和构建 PrismLabStarter 以及当前 D3D12 配置适用的全部 Donut 示例
cmake --preset all-projects
cmake --build --preset all-projects --parallel
```

全部程序输出到 `build/all-projects/bin/`。例如：

```powershell
.\build\all-projects\bin\basic_triangle.exe --dx12
.\build\all-projects\bin\vertex_buffer.exe --dx12
.\build\all-projects\bin\feature_demo.exe --dx12
```

`all-projects` 包含 PrismLab、Feature Demo、普通光栅示例、光追示例、Meshlet、异步计算、线程渲染、Work Graphs 和光追反射。Vulkan 专属的 `shader_specializations` 与需要额外 Aftermath SDK 的示例不属于当前 D3D12 配置。部分程序运行时还要求支持对应 GPU 功能，Feature Demo 和场景类示例可能需要上游媒体资产。

CMake Preset 负责配置和构建，不能安全地同时启动十几个窗口程序；需要测试哪个示例，直接运行 `bin` 中对应的 EXE。
