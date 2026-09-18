# 依赖与工具链记录

<!-- generated: 2026-09-18 19:14:52 -->

## 子模块提交

| 组件 | 提交 |
| --- | --- |
| PrismLab | 01648e18af702ae9f93d3b93ef5416996a4e20be |
| Donut-Samples | d766a7697b67b787714c3f8837fb271583306ab7 |
| donut | 69082638b014e3dae85a42c44b63e4187857c2c9 |
| nvrhi | ada8a144d37c31d67b31c40dc457a4bad4e083b7 |
| ShaderMake | <not-a-git-checkout> |
| glfw | <not-a-git-checkout> |
| imgui | <not-a-git-checkout> |
| cgltf | <not-a-git-checkout> |

第三方库（`thirdparty/*`）由 `git submodule status --recursive` 给出标签版本：
glfw 3.4、imgui v1.62-4778、cgltf v1.14-7。

## 工具链

- 后端：D3D12（DX11 / Vulkan 已在 CMake 中关闭）
- 生成器：Visual Studio 17 2022（x64）
- C++ 标准：C++17，静态运行库（MultiThreaded / MultiThreadedDebug）
- Shader 编译：ShaderMake（`donut/compileshaders.cmake`），框架 DXIL 输出到 `bin/shaders/framework/dxil`
- DXC：随 ShaderMake 构建流程下载，版本见首次配置时的构建日志

## 说明

- 所有依赖都通过提交固定，不使用浮动分支。
- 每次升级依赖后运行 `powershell -ExecutionPolicy Bypass -File scripts/record_deps.ps1` 并一起提交结果。
- 该脚本只改写上表中的提交与时间戳，其余内容手写维护。
