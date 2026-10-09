# PrismLab

Windows/D3D12 渲染实验项目。目前只有 `PrismDeferred` 一个 Sample：它描述程序场景、配置并调用 `PrismSurface` 的 Depth Pass，输出设备深度或线性深度预览。GBuffer、延迟光照和显示变换尚未实现；实施顺序见 [渲染学习路线](docs/rendering-study-roadmap.md)。

## 边界

- `samples/Deferred/`：创建地面、球、方块等场景对象和材质，选择场景与预设，设置每个 Pass 的参数并按顺序调用它们，发布调试结果。
- `framework/`：运行宿主、命令列表、场景资源生命周期与资产加载、相机、shader 包、资源缓存、回放、计时、截图和 UI。不会创建具体程序场景对象。
- `algorithms/Surface/`：只使用 NVRHI 和 C++ 标准库的 Depth Pass。Sample 负责将场景/相机数据转换为算法输入，并从 shader 库加载 NVRHI shader；`algorithms/shadows/` 暂只有设置和 PCF 核心。
- `external/Donut-Samples/`：Git 子模块，提供 Donut/NVRHI 及其自带场景、材质与绘制设施；不要与项目自有源文件混淆。

## 构建

需要 Visual Studio 2022 C++ 工具、Windows SDK、CMake 3.24+ 和 Git。初始化 Donut 子模块后：

```powershell
git submodule update --init external/Donut-Samples
git -C external/Donut-Samples submodule update --init --recursive donut
cmake --preset my-project
cmake --build --preset my-project-debug --parallel
.\build\my-project\bin\Debug\PrismDeferred.exe
```

也可以执行 `powershell -NoProfile -File prism.ps1 -Target PrismDeferred`；脚本会在需要时配置工程，默认构建 Debug 并启动唯一的 Sample。Release 使用 `cmake --build --preset my-project --parallel`。运行时 shader 路径为 `prism/<目标名>/<文件>.hlsl`；F6 对 Prism shader 包执行热重载，接口签名变化仍需完整构建。

## 场景与观察

默认预设在 `samples/Deferred/presets/default.json`，程序场景定义在同目录的 `ProceduralScene.cpp`。通用 `FSceneHost` 只管理加载、资源与刷新：程序场景工厂由 Sample 注入，glTF/GLB 资产则使用 Donut 加载接口；当前 Deferred 明确只接受程序场景。

可通过 `--config` 覆盖配置，使用 `--capture`、`--bench` 和 `--metrics` 保存截图与性能信息；日志位于可执行文件旁的 `prism.log`。完整操作与参数可通过 `--help` 查看。

## 许可

Prism 自身代码按 [MIT](LICENSE) 发布。Donut 等依赖保留各自许可，见 [第三方声明](THIRD_PARTY_NOTICES.md)。
