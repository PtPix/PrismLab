# Prism

个人 D3D12 渲染技术实验平台。你实现 HLSL、GPU 数据结构和 C++ 调度；框架负责设备、窗口、Pass 模板、调试与实验工具。场景和完整渲染管线由 Sample 显式选择，不再由宿主强制创建。

Donut / NVRHI 提供基础设施。SSAO、TAA、Bloom、ToneMapping、Sky、IBL 等效果由你自己实现；旧 Donut Forward 仅作为可选参考适配器。

## 目录

```text
framework/core/       基础值类型与状态
framework/render/     数据约定、资源、Pass、Shader、GPU 计时
framework/scene/      CPU/GPU 场景与 Surface 接口
framework/adapters/   Donut 相机、可选场景与旧前向适配
framework/tools/      比较、回放、调试、捕获、指标、Shader 编译
framework/app/        应用与帧循环、配置、UI 装配
algorithms/<family>/  技术族：HLSL、Settings、Inputs/Outputs、C++ 调度
samples/<family>/    演示入口、参数面板、管线组合、presets
examples/            可选的底层集成参考
cmake/               目标声明、Shader 包与可选示例构建
tests/cpu/           无窗口、无 GPU 的测试
tests/gpu/           GPU 回归与 Contract 交互检查
```

当前架构与归属规则见 [architecture.md](docs/architecture.md)，API 示例见 [framework-experiments.md](docs/framework-experiments.md)。

## 环境和依赖

Windows、D3D12 显卡、Visual Studio 2022 C++ 工具、Windows SDK、CMake 3.24+、Git。首次配置会下载 DXC 和 DirectX-Headers。

```powershell
git submodule update --init external/Donut-Samples
git -C external/Donut-Samples submodule update --init --recursive donut
```

默认不下载上游大型媒体资产。

## 构建与运行

```powershell
cmake --preset my-project
cmake --build --preset my-project --parallel
.\build\my-project\bin\Release\PrismStarter.exe

# Debug，开启 NVRHI 校验
cmake --build --preset my-project-debug
.\build\my-project\bin\Debug\PrismForward.exe

# 或通过脚本自动寻找 CMake、构建并运行
powershell -NoProfile -File prism.ps1 -Target PrismStarter -Config Debug
```

Debug / Release 的 EXE、Shader、重载脚本和日志分别位于 `bin/<配置>/`，互不覆盖。旧版 `bin/` 根目录的产物不再使用。VS Code 调试路径已同步。

- `PrismStarter`：统一宿主下的无场景全屏 Pass，新实验起点。
- `PrismForward`：显式选择旧前向适配器，展示深度等调试输出。
- `PrismContract`：位于 `tests/gpu/contract`，验证矩阵/深度的 CPU-GPU 一致性。
- `PrismDonutTriangle`：原始 Donut 三角形参考，需 `-DPRISM_BUILD_EXAMPLES=ON`。

```powershell
# 构建所有适用的上游 Donut 示例
cmake --preset all-projects
cmake --build --preset all-projects --parallel
```

上游例子可能要求额外 GPU 功能或媒体资产，程序输出到 `build/all-projects/bin/<配置>/`。

## 写一个技术族

算法放在 `algorithms/<family>`：参数、类型、Shader、C++ Pass 调度及内部状态一起维护。Sample 放在 `samples/<family>`，处理演示场景、技术选择、UI、比较与美术预设。不要从算法代码包含 `ExperimentContext` 或 ImGui。

```cmake
prism_add_target(MySample KIND EXECUTABLE
    SOURCES MySample.cpp
    SHADERS Resolve.hlsl:ps
    LINK prism_my_algorithm
    CONFIG presets/default.json)
```

运行时 Shader 路径是 `prism/<包名>/<文件>.hlsl`。CMake 自动生成 Shader 配置、依赖包清单和重载脚本。F6 重编译当前 Sample 及其依赖的 Shader 包，失败时保留旧管线。

`algorithms/shadows` 目前只有阴影设置和 PCF 核心，尚未实现完整阴影系统。SceneFrameData / SceneGpuData / SceneSurfacePipeline 仅接口，由你提供 Buffer、Texture 和生产逻辑。

## 实验与测试

Comparison 支持 A/B、并排、Wipe、差分和冻结。Replay 支持固定步长、镜头/参数录制、暂停、单步和 JSON 持久化。显示变换通过你实现的 DisplayChain 接入。

```powershell
cmake --preset my-project -DPRISM_BUILD_TESTS=ON
cmake --build build/my-project --config Debug --target PrismCpuTests PrismInfrastructureTests PrismContract
ctest --test-dir build/my-project -C Debug --output-on-failure

.\build\my-project\bin\Debug\PrismContract.exe --smoke-test=12 --no-vsync
.\build\my-project\bin\Debug\PrismForward.exe --capture forward.png --capture-frame 4
.\build\my-project\bin\Debug\PrismForward.exe --bench=120 --bench-warmup=30 --metrics metrics.csv
```

CPU 测试不创建窗口和设备；GPU 测试需要 Windows 桌面与 D3D12。失败返回非零退出码。`--capture` 保存不含 UI 的最终展示结果；`--write-reference` / `--reference` 使用显示转换前的浮点结果。其他参数见 `--help`。日志位于 EXE 旁的 `prism.log`。

## 场景与预设

Forward 从自己的 `presets/default.json` 读取镜头、灯光与演示配置，可使用 `--config` 覆盖。多个 Sample 共享的大型 glTF、贴图等放在 `assets/`。旧前向适配器默认可生成测试几何，也支持 Donut 场景加载：

```powershell
git -C external/Donut-Samples submodule update --init media
.\build\my-project\bin\Debug\PrismForward.exe --scene gltf --asset media/sponza-plus.scene.json
```

## 来源

应用入口基于 Donut-Samples 与 Donut 的设备/UI 框架，保留其版权和 MIT 许可。第三方许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

Prism 自身代码以 MIT 发布，见 [LICENSE](LICENSE)；`external/` 下的子模块各自保留原许可。
