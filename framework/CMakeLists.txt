add_library(prism_core INTERFACE)
target_include_directories(prism_core INTERFACE "${PRISM_ROOT}")
target_link_libraries(prism_core INTERFACE donut_core)
add_library(prism_render STATIC
    render/resources/BufferCache.cpp render/resources/TextureCache.cpp
    render/resources/ResourceTable.cpp render/resources/Formats.cpp
    render/profiling/GpuProfiler.cpp render/shaders/ShaderLibrary.cpp
    render/shaders/ShaderInterface.cpp render/passes/PassSupport.cpp
    render/passes/ComputePass.cpp render/passes/RasterPass.cpp render/passes/FullscreenPass.cpp)
target_link_libraries(prism_render PUBLIC prism_core donut_engine nvrhi)
get_filename_component(dxc_bin "${SHADERMAKE_DXC_PATH}" DIRECTORY)
get_filename_component(dxc_root "${dxc_bin}/../.." ABSOLUTE)
target_include_directories(prism_render PRIVATE "${dxc_root}/inc")
target_compile_definitions(prism_render PRIVATE PRISM_DXCOMPILER_PATH="${dxc_bin}/dxcompiler.dll")
add_library(prism_scene INTERFACE)
target_link_libraries(prism_scene INTERFACE prism_render)
add_library(prism_camera STATIC adapters/donut/CameraController.cpp)
target_link_libraries(prism_camera PUBLIC prism_core donut_app)
add_library(prism_donut_scene STATIC
    adapters/donut/SceneHost.cpp adapters/donut/ProceduralScene.cpp
    adapters/donut/LightAdapter.cpp adapters/donut/SceneForwardPipeline.cpp adapters/donut/ForwardScene.cpp)
target_link_libraries(prism_donut_scene PUBLIC prism_scene donut_render)
add_library(prism_replay STATIC tools/replay/ReplayTrack.cpp tools/replay/ReplayController.cpp)
target_link_libraries(prism_replay PUBLIC prism_core)
add_library(prism_tools STATIC
    tools/capture/TextureReadback.cpp tools/capture/ImageReference.cpp tools/metrics/Metrics.cpp
    tools/inspection/DebugView.cpp tools/comparison/ComparisonPass.cpp tools/comparison/ComparisonController.cpp
    tools/shaders/ShaderReload.cpp tools/shaders/ShaderBuildTask.cpp)
target_link_libraries(prism_tools PUBLIC prism_render prism_replay)
target_compile_definitions(prism_tools PRIVATE PRISM_CMAKE_PATH="${CMAKE_COMMAND}")
prism_add_shader_package(PrismTools SHADERS
    tools/inspection/shaders/DebugView.hlsl:vs tools/inspection/shaders/DebugView.hlsl:ps
    tools/comparison/Difference.hlsl:ps)
set_property(TARGET prism_tools PROPERTY PRISM_SHADER_PACKAGES PrismTools)
add_dependencies(prism_tools PrismToolsShaders)
add_library(prism_app STATIC
    app/Application.cpp app/CommandLine.cpp app/Entry.cpp app/ExperimentHost.cpp
    app/HostConfig.cpp app/Params.cpp app/UiOverlay.cpp app/FramePresentation.cpp
    app/ExperimentTools.cpp app/ExperimentToolsUI.cpp)
target_link_libraries(prism_app PUBLIC prism_tools prism_camera donut_app)
set_property(TARGET prism_app PROPERTY PRISM_DEPENDENCIES prism_tools prism_camera)
foreach(target prism_render prism_camera prism_donut_scene prism_replay prism_tools prism_app)
    prism_target_defaults(${target})
endforeach()
