# Rendering experiment infrastructure

The framework handles execution mechanics. Samples own their shaders, scheduling,
resource layouts, scene producers, temporal algorithms and display transforms.
Donut supplies the device/window, shader loading, binding caches, samplers and
fullscreen/blit primitives. No Donut SSAO, TAA, bloom, tone mapping, sky or IBL
implementation is added by these helpers. The existing Forward sample remains a
legacy reference; a new sample can record its entire pipeline itself.

## Modules

| Path | Responsibility |
| --- | --- |
| `framework/render/passes/` | Compute, fullscreen and raster recording; PSO and binding reuse |
| `framework/render/shaders/` | Reload participant contract and DXIL interface reflection |
| `framework/tools/shaders/` | Background compilation and frame-boundary reload |
| `framework/tools/comparison/` | GPU composition, difference, frozen copies and named selection |
| `framework/tools/replay/` | Fixed clock, exact camera/parameter tracks and JSON persistence |
| `framework/app/ExperimentTools*` | Host integration and UI |
| `framework/app/FramePresentation*` | Capturable final image and application display-chain hook |
| `framework/scene/SceneFrameData.h` | Borrowed CPU scene metadata |
| `framework/scene/SceneGpuData.h` | Borrowed buffers and descriptor table |
| `framework/scene/SceneSurfacePipeline.h` | Application surface producer interface |

## Passes

Keep passes as sample/algorithm members. Initialize them once; record in your own
order every frame. `ShaderLibrary` and the device must outlive their passes.
Passes record into the supplied command list; they never submit or wait for the GPU.
Native NVRHI layouts, binding descriptors and graphics states remain available.

```cpp
// Initialize. The group size must match the shader's numthreads declaration.
gpu::ComputePass pass;
auto status = pass.Initialize(context.gpu.device, *context.gpu.shaders,
    {"prism/MySample/filter.hlsl", "main_cs", nvrhi::ShaderType::Compute, {}},
    {layout}, dm::uint3(8, 8, 1));
if (!status) return status;

// Render. A bounds check in HLSL handles the rounded-up dispatch.
nvrhi::BindingSetDesc desc;
desc.bindings = {nvrhi::BindingSetItem::Texture_UAV(0, output)};
auto bindings = pass.Bindings(desc, layout);
status = pass.DispatchExtent(frame.commands, {bindings},
    dm::uint3(frame.renderSize.width, frame.renderSize.height, 1));
```

`Dispatch` takes group counts directly. `FullscreenPass` takes your pixel shader,
layouts, framebuffer and optional viewport; it uses a four-vertex fullscreen strip.
`RasterPass` takes a native `GraphicsPipelineDesc` and managed shader stages;
`Draw(..., indexed=true)` records indexed geometry. `Bind` supports custom command
recording, including indirect draws. Mesh and ray tracing pipelines remain native
NVRHI extensions rather than being forced into this raster helper.

`PassConstants` creates a volatile constant buffer and uploads a trivially copyable
C++ structure. Its allocation size must match that structure. You own packing and
HLSL compatibility. Binding caches retain resource handles; call `ClearBindings()`
on resize/resource replacement to release obsolete bindings. PSOs are cached by
framebuffer format, sample count and attachment configuration, independently of size.

## Shader iteration

Use `prism_add_target(MySample ... SHADERS filter.hlsl:cs)` in CMake. Entries may also
be `file.hlsl:stage:entry`; `SHADER_MODEL` and `SHADER_OPTIONS` configure compilation.
Runtime paths are `prism/<package>/<file>.hlsl`, preventing collisions across modules.
`prism_add_target` declares one package for its own shaders. Packages from `LINK`
dependencies are collected into `<executable>.shaders.json`; the generated reload
script builds the same package list. Use `PRISM_SHADER_PACKAGES` and
`PRISM_DEPENDENCIES` properties for libraries assembled without `prism_add_target`.
Filenames within one target must remain unique because ShaderMake flattens paths.

Press **F6** or use **Shaders / Compile and reload**. Compilation runs outside the
render loop into `.shader-reload/<target>/<generation>/<package>/` next to the executable.
The compiler log is shown on failure. All registered passes prepare candidate PSOs;
a successful transaction replaces them together at a frame boundary and resets
history. A compile, reflection or pipeline failure retains the active generation.
Used raster attachment variants are rebuilt before committing.

Reload is explicit, not a file watcher. It rebuilds the executable's transitive shader
packages, including tool and algorithm packages and their changed HLSL includes. Add/remove shader entries or change CMake options
through the normal CMake build. Native PSOs and shader handles created outside these
passes do not participate automatically; use `ShaderReloadClient` for custom owners.
Shared Donut shaders and the optional legacy forward adapter require a normal build/restart.
Prism inspection and comparison passes participate in package reload.

DXIL reflection rejects changed resource registers/types, constant layouts,
input/output signatures and compute thread-group dimensions. Rebuild the application
when changing a CPU/HLSL contract. Structured/raw buffer contents and semantic
meaning remain application contracts; reflection is not a full schema validator.
Ray tracing libraries can be loaded, but their pipeline reload requires a custom
participant. Development reload needs the configured CMake, ShaderMake and DXC paths.
Staging directories are retained for inspection and can be removed after exit.

## Comparison and presentation

Publish stable IDs each frame from `Render`:

```cpp
context.tools.comparison->Publish("Reference", {reference, ColorSpace::SceneLinear});
context.tools.comparison->Publish("Candidate", {candidate, ColorSpace::SceneLinear});
```

The host publishes `Output` automatically. UI modes are A, B, side by side, wipe
and absolute RGB difference with gain. **Freeze B** copies the selected texture;
subsequent writes to the source do not affect that snapshot. Inputs must be 2D,
single-sample SRVs with equal extents and color-space labels. Resolve MSAA, choose
subresources and convert representations yourself before publishing. Invalid inputs
show a message and leave the sample output visible.

Side by side fits each complete image into its panel; wipe preserves coordinates.
Difference uses point loads and produces a display diagnostic without tone mapping.
The comparison result precedes the application's `gpu::IDisplayChain`; debug views bypass
that transform. Supply your own exposure, tone mapping and display conversion there.
Without a display chain, presentation only blits. `--capture` saves final displayed
pixels before UI. Float references save the selected output before display conversion
(including active comparison/debug selection).

## Replay

Register sample parameter capture/restore callbacks during initialization:

```cpp
auto& replay = *context.tools.replay;
replay.captureParameters = [this] { return SaveParameters(); };
replay.restoreParameters = [this](const Json::Value& json) { LoadParameters(json); };
```

The Replay panel provides recording, playback, pause, step, restart, seed, fixed rate
and JSON save/load. Recording uses a fixed timestep and stores one camera pose and
parameter snapshot per logical frame. Playback restores those exact samples, locks
camera input and pauses at the end. It does not interpolate keyframes or record OS
input events. Restoring algorithm parameters must perform any sample-specific resource
recreation/history reset that an interactive parameter edit would require.

`FrameInfo.frameIndex`, `timeSeconds`, `deltaTimeSeconds` and `randomSeed` describe
the logical experiment clock. `submissionIndex` always advances with rendered frames.
The first logical frame has zero delta; paused frames retain the logical index.
Use these fields for simulation, random sampling and temporal progression. Do not
advance algorithm history a second time on a repeated logical frame. Camera jitter
is derived from the logical frame index. Restart, record, playback and shader reload
request history reset; your algorithm consumes the existing reset flags.

Replay covers camera and registered parameters. It does not serialize scene buffers,
GPU history, external assets, asynchronous loading or arbitrary application state.
Drive custom scene animation from the logical clock and reconstruct history from the
beginning for repeatable comparisons. Replay does not promise cross-device bit identity.

## Scene interfaces

`context.scene.frameData`, `gpuData` and `surfacePipeline` start as null. Assign your
own producers and storage. The host neither fills nor automatically invokes them.
`SceneFrameData` carries stable IDs, current/previous transforms, bounds and revisions.
`SceneGpuData` exposes buffer offset/count/stride/generation, an optional texture table
and a producer-defined `layoutId`. There is no implicit vertex/material packing.

Allocate surface textures yourself, fill `SceneSurfaceData`, set its schema/depth and
motion conventions, validate required channels, and call your `ISceneSurfacePipeline`
implementation with `SurfaceFrame`. Channels are depth, normal/roughness,
base-color/metalness, emissive, motion, instance ID and material ID. Motion is previous
minus current in the declared UV/pixel units, with an explicit jitter flag.

CPU data must survive frame recording. GPU resources must survive their submitted
work; these scene views are borrowed pointers and do not extend resource lifetimes.
Generation/revision fields support your own cache invalidation. No G-buffer generation,
material evaluation, visibility, scene upload, lighting or acceleration structure is
implemented by these interfaces.

## Verification

```powershell
cmake --preset my-project -DPRISM_BUILD_TESTS=ON
cmake --build build/my-project --config Debug --target PrismCpuTests PrismInfrastructureTests PrismContract
ctest --test-dir build/my-project -C Debug --output-on-failure
```

`PrismCpuTests` runs without creating a window or device. GPU tests require a Windows
desktop and D3D12 device. Tests exercise odd-sized compute output,
indexed raster drawing, fullscreen difference, resource replacement, frozen copies,
A/B/wipe pixels, invalid color spaces, real asynchronous compilation, transaction
rollback, incompatible thread groups, failed build retention, replay persistence,
clock controls and camera roll. Existing `PrismContract --smoke-test=12` checks the
camera/depth CPU-GPU contract. GPU timer scopes support nesting; timings carry their
source submission index and may arrive later than the current UI frame.

## Resource identity and lifetime

`TextureCache` and `BufferCache` own persistent allocations; they do not alias memory
across transient lifetimes. Each new `TextureRequest` / `BufferRequest` receives a
`ResourceId`. Copies retain that identity. Names may repeat across algorithm instances.
Use `Find(request.id)`; assign `AllocateResourceId()` explicitly to fork a copied
request into a new independent allocation. Slots carry requests with the same rules.
Resolution changes update both caches through `ResourceTable`. Fixed-size requests
remain independent of render size. The caller synchronizes before retiring resources.

The host calls `Experiment::Shutdown` exactly once after an initialization attempt,
including partial initialization failure. Implement it defensively for partially
created state. The GPU is idle and device/services are still alive at this point;
the sample and remaining GPU services are destroyed before DeviceManager shuts down.

## Source and output ownership

See [architecture.md](architecture.md) for the authoritative module map.
Executables, shader outputs, manifests and runtime logs are isolated under
`build/<preset>/bin/<configuration>/`. Development tools use those paths as well.
`DisplayChain` and `TemporalServices` now receive `gpu::RenderServices` rather than
`ExperimentContext`; their interfaces contain no UI callbacks. Sample `BuildUI`
owns controls. Temporal services receive explicit `BeginFrame` / `EndFrame` calls.
The host supplies no scene by default. Forward and Contract explicitly instantiate
`adapter::ForwardScene`; its Donut renderer is an optional legacy reference only.
