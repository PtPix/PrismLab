# Prism architecture

This document describes the current implementation. The dated architecture review
is historical design material; it is not a specification of current paths or APIs.

## Ownership

| Module | Owns | Must not own |
| --- | --- | --- |
| `framework/core` | Status, IDs and scalar/math value types | Device, scene graph, UI |
| `framework/render` | Resources, passes, shader loading/reflection, GPU timing, render contracts | Application context, tools, concrete algorithms |
| `framework/scene` | Borrowed CPU/GPU scene views, surface semantics and producer interface | Scene uploads or a renderer implementation |
| `framework/adapters/donut` | Camera implementation, optional imported/procedural scenes and legacy forward adapter | Mandatory application scene policy |
| `framework/tools` | Comparison, replay, inspection, capture, metrics, compiler process | Algorithm implementation |
| `framework/app` | Window/device lifetime, frame driver, configuration, UI integration | Default scene or lighting pipeline |
| `algorithms/<family>` | Method settings, inputs/outputs, HLSL and algorithm-specific C++ scheduling | Windows, ImGui, ExperimentContext |
| `samples/<family>` | Demonstration scene, composition, presets, controls, comparison | Reusable algorithm internals |
| `tests/cpu`, `tests/gpu` | Verification fixtures and executable checks | Product demonstrations |
| `examples` | Optional low-level third-party integration references | Starter template for new experiments |

A C++ algorithm may directly use NVRHI and render services. Its reusable HLSL core
can use explicit bridge functions where useful. There is no additional RHI wrapper,
mandatory render graph or universal algorithm base class.

## Dependency direction

```text
prism_core <- prism_render <- algorithm libraries <- samples
                  ^              ^
                  |         prism_scene (contracts)
                  |
              prism_tools <- prism_app <- samples
prism_core <- prism_replay ---^
prism_core <- prism_camera <- prism_app
prism_scene <- prism_donut_scene <- Forward / Contract only
```

Arrows mean that the target on the right depends on the target on the left.
The scene contracts use NVRHI views and render data. `prism_render` does not depend
on `prism_scene` or any algorithm. `prism_app` does not link `prism_donut_scene`.
Donut engine infrastructure is intentionally reused; concrete render effects belong
to the user's algorithms. The optional legacy scene adapter links Donut rendering.

## Files and naming

Directories use lowercase names; C++ types/files use PascalCase for new components.
Keep private shader entry points and CPU/HLSL shared layouts with their owning module.
Small modules keep sources beside their headers. Add a `shaders/` subdirectory only
when it helps separate a substantial set of shader files. Do not add empty layers.

Existing Forward and Contract filenames are retained to avoid unrelated churn.
`algorithms/shadows` currently holds settings and the PCF kernel only; it is not a
complete shadow renderer. Its future C++ passes belong there, not under framework.

`cmake/PrismTargets.cmake` assembles libraries/samples. `ShaderPackages.cmake` owns
shader compilation, package discovery and reload manifests. `DonutExamples.cmake`
contains optional upstream examples. The root CMake file only selects dependencies
and modules. `framework/CMakeLists.txt` lists framework targets and their sources.

## Public contracts

`Experiment.h` contains the sample lifecycle. `ExperimentContext.h` groups borrowed
GPU, scene, tool, temporal and presentation services. `ExperimentFrame.h` supplies
per-frame values. Reusable render algorithms accept typed inputs and
`gpu::RenderServices`, not the application context.

Scene buffers and surface output textures are application-supplied. The surface
interface does not perform material evaluation, visibility or lighting. Temporal and
display interfaces similarly contain no effect implementation or UI methods.

Resource requests have stable IDs. Copying shares identity; separate default requests
remain separate even with identical names. Names are labels, not lookup keys.
Caches own resources; scene input views borrow them. Handle synchronization explicitly
when replacing externally owned buffers. Pass registration requires ShaderLibrary to
outlive its passes; the application destruction order enforces this for sample members.

## Samples and presentation

`PrismStarter` is a scene-free fullscreen experiment using the standard host. Its
procedural gradient demonstrates shader iteration and the logical replay clock.
`PrismForward` explicitly selects the legacy forward adapter and is a comparison
reference. `PrismContract` lives under GPU tests and remains directly runnable for
interactive matrix/depth inspection. `PrismDonutTriangle` is an optional raw Donut
example built with `PRISM_BUILD_EXAMPLES=ON`.

Each sample owns `presets/default.json`; further camera, material, lighting and
technique presets belong beside it. Shared large assets belong under `assets/`.
The framework does not provide bloom, tone mapping, sky, IBL, TAA or SSAO algorithms.

## Build and verification

Debug and Release outputs are independent under `bin/<configuration>`. Do not use
the old unqualified `bin` executables left by previous builds. A sample manifest lists
its own and transitive dependency shader packages. F6 builds them into a separate
staging directory, prepares all managed PSOs and commits together at a frame boundary.

Tests cover CPU replay/camera behavior, same-name resource isolation, per-pixel buffer
resize, compute/raster/comparison output, dependent shader package reload, incompatible
interfaces, compiler failure, and Shutdown on normal/failed initialization. Contract
checks verify matrix and depth agreement on the GPU. CPU checks create no GPU device.
