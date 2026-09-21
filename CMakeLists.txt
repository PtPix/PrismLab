cmake_minimum_required(VERSION 3.24)
project(PrismLab LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This project currently targets Windows and D3D12.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin")
endforeach()

set(DONUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/Donut-Samples/donut")
if(NOT EXISTS "${DONUT_DIR}/nvrhi/CMakeLists.txt")
    message(FATAL_ERROR "Dependencies missing. See the submodule commands in README.md.")
endif()

# Default to our own targets; upstream demos are enabled explicitly below.
set(DONUT_WITH_DX11 OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_DX12 ON CACHE BOOL "" FORCE)
set(DONUT_WITH_VULKAN OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_TINYEXR OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(DONUT_SHADERS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/framework")
add_subdirectory("${DONUT_DIR}" "${CMAKE_BINARY_DIR}/donut" EXCLUDE_FROM_ALL)
include("${DONUT_DIR}/compileshaders.cmake")

# ---------------------------------------------------------------------------
# RenderLab framework
#
# Layers and their dependency direction (see docs/architecture.md):
#   rl_contracts   <- rl_algorithms
#   rl_contracts   <- rl_nvrhi_common <- rl_adapter_donut
#   rl_contracts   <- rl_pipelines (Donut render passes)
#   all of the above <- rl_host <- samples
# ---------------------------------------------------------------------------
set(RL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(RL_CONTRACTS_INCLUDE "${RL_ROOT}/include")
set(RL_ALGORITHM_SHADERS "${RL_ROOT}/algorithms/shaders")
set(RL_LAB_SHADER_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/renderlab")

# Contracts: parameters and semantics only. Depends on the math library, never on Donut scene
# types, NVRHI objects or UI.
add_library(rl_contracts INTERFACE)
target_include_directories(rl_contracts INTERFACE "${RL_CONTRACTS_INCLUDE}")
target_link_libraries(rl_contracts INTERFACE donut_core)

# Algorithms: reusable HLSL cores that talk to the host through bridge functions
# (algorithms/shaders/...). Also the include root for the .hlsli files.
add_library(rl_algorithms INTERFACE)
target_include_directories(rl_algorithms INTERFACE "${RL_ALGORITHM_SHADERS}")

# NVRHI execution layer: shader variants, named transient resources, GPU timing, readback,
# pipeline/draw helpers, geometry batch view.
add_library(rl_nvrhi_common STATIC
    backends/nvrhi/common/Formats.cpp
    backends/nvrhi/common/ShaderLibrary.cpp
    backends/nvrhi/common/RenderTargetPool.cpp
    backends/nvrhi/common/BufferPool.cpp
    backends/nvrhi/common/ResourceTable.cpp
    backends/nvrhi/common/GpuProfiler.cpp
    backends/nvrhi/common/PipelineUtils.cpp
    backends/nvrhi/common/TextureReadback.cpp
    backends/nvrhi/common/DebugView.cpp
)
target_include_directories(rl_nvrhi_common PUBLIC "${RL_ROOT}" "${RL_CONTRACTS_INCLUDE}")
# rl_algorithms 提供算法侧共享头（例如 DebugView_cb.h）的包含路径与 shader 目录。
target_link_libraries(rl_nvrhi_common PUBLIC rl_contracts rl_algorithms donut_core nvrhi)

# Donut adapter: camera controller, configuration, scene sources, light and geometry conversion.
add_library(rl_adapter_donut STATIC
    adapters/donut/HostConfig.cpp
    adapters/donut/CameraController.cpp
    adapters/donut/LightAdapter.cpp
    adapters/donut/ProceduralScene.cpp
    adapters/donut/SceneHost.cpp
)
target_include_directories(rl_adapter_donut PUBLIC "${RL_ROOT}" "${RL_CONTRACTS_INCLUDE}")
target_link_libraries(rl_adapter_donut PUBLIC rl_nvrhi_common rl_algorithms donut_engine donut_app)

# Pipelines: reusable pass wiring. The forward scene path is the baseline every lab can consume.
add_library(rl_pipelines STATIC
    pipelines/SceneForwardPipeline.cpp
)
target_include_directories(rl_pipelines PUBLIC "${RL_ROOT}" "${RL_CONTRACTS_INCLUDE}")
target_link_libraries(rl_pipelines PUBLIC rl_contracts donut_render donut_engine)

# Host: window, device, swap chain, camera, scene, UI, GPU timing, frame loop, WinMain.
add_library(rl_host STATIC
    host/CommandLine.cpp
    host/Params.cpp
    host/Metrics.cpp
    host/ImageReference.cpp
    host/LabHost.cpp
    host/UiOverlay.cpp
    host/Application.cpp
    host/Entry.cpp
)
target_include_directories(rl_host PUBLIC "${RL_ROOT}" "${RL_CONTRACTS_INCLUDE}")
target_link_libraries(rl_host PUBLIC rl_adapter_donut rl_pipelines rl_nvrhi_common donut_app donut_render donut_engine)

foreach(rl_target rl_nvrhi_common rl_adapter_donut rl_pipelines rl_host)
    if(MSVC)
        # /W4 for our own code, no warnings from Donut / NVRHI / imgui headers.
        target_compile_options(${rl_target} PRIVATE /W4 /utf-8 /external:anglebrackets /external:W0)
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Experiments
# ---------------------------------------------------------------------------
include("${RL_ROOT}/cmake/renderlab.cmake")

# 框架自带的 shader（公共调试视图）：所有实验共用，输出到与其他实验 shader 相同的目录。
donut_compile_shaders(
    TARGET RenderLabShaders
    PROJECT_NAME "RenderLab framework shaders"
    CONFIG "${RL_ROOT}/algorithms/shaders/RenderLab/Debug/shaders.cfg"
    SOURCES "${RL_ROOT}/algorithms/shaders/RenderLab/Debug/DebugView.hlsl"
    DXIL "${RL_LAB_SHADER_OUTPUT}/dxil"
    INCLUDES "${RL_ALGORITHM_SHADERS}"
    FOLDER "RenderLab")

add_subdirectory(samples/starter)
add_subdirectory(samples/lab_forward)
add_subdirectory(samples/lab_contract)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT PrismLabForward)

# The all-projects preset adds every Donut-Samples target supported by this D3D12 configuration.
# Vulkan-only and optional Aftermath targets are omitted.
option(PRISMLAB_BUILD_ALL_PROJECTS "Build PrismLab and all applicable Donut examples" OFF)
if(PRISMLAB_BUILD_ALL_PROJECTS)
    set(donut_samples_dir "${CMAKE_CURRENT_SOURCE_DIR}/external/Donut-Samples")

    # Work Graphs needs the newer D3D12 headers and runtime from Agility SDK.
    set(DONUT_D3D_AGILITY_SDK_URL
        "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.717.1-preview")
    set(DONUT_D3D_AGILITY_SDK_FETCH_DIR "" CACHE STRING "" FORCE)
    include("${DONUT_DIR}/cmake/FetchAgilitySDK.cmake")
    file(COPY
        "${DONUT_D3D_AGILITY_SDK_PATH}/build/native/bin/x64/D3D12Core.dll"
        "${DONUT_D3D_AGILITY_SDK_PATH}/build/native/bin/x64/D3D12SDKLayers.dll"
        DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/D3D12/")

    add_subdirectory("${donut_samples_dir}/feature_demo" "${CMAKE_BINARY_DIR}/examples/feature_demo" EXCLUDE_FROM_ALL)

    set(donut_example_dirs
        basic_triangle
        vertex_buffer
        deferred_shading
        headless
        bindless_rendering
        variable_shading
        rt_triangle
        rt_shadows
        rt_bindless
        rt_particles
        meshlets
        threaded_rendering
        async_compute
        work_graphs
        rt_reflections
    )
    foreach(example IN LISTS donut_example_dirs)
        add_subdirectory(
            "${donut_samples_dir}/examples/${example}"
            "${CMAKE_BINARY_DIR}/examples/${example}"
            EXCLUDE_FROM_ALL)
    endforeach()

    add_custom_target(PrismLabAllProjects)
    add_dependencies(PrismLabAllProjects
        PrismLabStarter
        PrismLabForward
        PrismLabContract
        feature_demo
        basic_triangle
        vertex_buffer
        deferred_shading
        headless
        bindless_rendering
        variable_shading
        rt_triangle
        rt_shadows
        rt_bindless
        rt_particles
        meshlets
        threaded_rendering
        async_compute
        work_graphs_d3d12
        rt_reflections
    )
endif()
