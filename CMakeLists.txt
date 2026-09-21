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

# --- Donut / NVRHI ---------------------------------------------------------
set(DONUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/Donut-Samples/donut")
if(NOT EXISTS "${DONUT_DIR}/nvrhi/CMakeLists.txt")
    message(FATAL_ERROR "Dependencies missing. See the submodule commands in README.md.")
endif()

set(DONUT_WITH_DX11 OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_DX12 ON CACHE BOOL "" FORCE)
set(DONUT_WITH_VULKAN OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_TINYEXR OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(DONUT_SHADERS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/framework")
add_subdirectory("${DONUT_DIR}" "${CMAKE_BINARY_DIR}/donut" EXCLUDE_FROM_ALL)
include("${DONUT_DIR}/compileshaders.cmake")

# ---------------------------------------------------------------------------
# algorithms/   decoupled core: HLSL that reaches its host only through RL_* bridge functions.
#               It must not include framework headers.
# framework/    everything that owns Donut / NVRHI: types, execution helpers, Donut facilities,
#               pipelines and the application host. This half uses donut::math directly.
# samples/      experiments: a Lab subclass, its shaders and its own config.json.
#
# Libraries: rl_types <- rl_nvrhi <- rl_donut <- rl_host, plus rl_pipelines; rl_host <- samples.
# ---------------------------------------------------------------------------
set(RL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(RL_ALGORITHM_SHADERS "${RL_ROOT}/algorithms/shaders")
set(RL_LAB_SHADER_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/renderlab")

# Data algorithms consume, shared with the framework; also the include root for <framework/...>.
add_library(rl_types INTERFACE)
target_include_directories(rl_types INTERFACE "${RL_ROOT}")
target_link_libraries(rl_types INTERFACE donut_core)

# Reusable HLSL cores plus the C++ headers shared with them (e.g. DebugView_cb.h).
add_library(rl_algorithms INTERFACE)
target_include_directories(rl_algorithms INTERFACE "${RL_ALGORITHM_SHADERS}")

# NVRHI execution helpers: shader variants, named transient resources, GPU timing, readback,
# pipeline/draw helpers, geometry batch view.
add_library(rl_nvrhi STATIC
    framework/nvrhi/BufferPool.cpp
    framework/nvrhi/DebugView.cpp
    framework/nvrhi/Formats.cpp
    framework/nvrhi/GpuProfiler.cpp
    framework/nvrhi/PipelineUtils.cpp
    framework/nvrhi/RenderTargetPool.cpp
    framework/nvrhi/ResourceTable.cpp
    framework/nvrhi/ShaderLibrary.cpp
    framework/nvrhi/TextureReadback.cpp)
target_link_libraries(rl_nvrhi PUBLIC rl_types rl_algorithms donut_core nvrhi)

# Donut facilities: configuration, camera controller, scene sources, light and geometry conversion.
add_library(rl_donut STATIC
    framework/donut/CameraController.cpp
    framework/donut/HostConfig.cpp
    framework/donut/LightAdapter.cpp
    framework/donut/ProceduralScene.cpp
    framework/donut/SceneHost.cpp)
target_link_libraries(rl_donut PUBLIC rl_nvrhi donut_engine donut_app)

# Reusable pass wiring; the forward scene path is the baseline every lab can consume.
add_library(rl_pipelines STATIC
    framework/pipelines/SceneForwardPipeline.cpp)
target_link_libraries(rl_pipelines PUBLIC rl_types donut_render donut_engine)

# Application host: window, device, camera, scene, UI, timing, frame loop, command line, WinMain.
add_library(rl_host STATIC
    framework/host/Application.cpp
    framework/host/CommandLine.cpp
    framework/host/Entry.cpp
    framework/host/ImageReference.cpp
    framework/host/LabHost.cpp
    framework/host/Metrics.cpp
    framework/host/Params.cpp
    framework/host/UiOverlay.cpp)
target_link_libraries(rl_host PUBLIC rl_donut rl_pipelines rl_nvrhi donut_app donut_render donut_engine)

foreach(rl_target rl_nvrhi rl_donut rl_pipelines rl_host)
    if(MSVC)
        # /W4 for our own code, no warnings from Donut / NVRHI / imgui headers.
        target_compile_options(${rl_target} PRIVATE /W4 /utf-8 /external:anglebrackets /external:W0)
    endif()
endforeach()

# The framework's own shader (the shared debug view) is available to every experiment.
donut_compile_shaders(
    TARGET RenderLabShaders
    PROJECT_NAME "RenderLab framework shaders"
    CONFIG "${RL_ROOT}/algorithms/shaders/RenderLab/Debug/shaders.cfg"
    SOURCES "${RL_ROOT}/algorithms/shaders/RenderLab/Debug/DebugView.hlsl"
    DXIL "${RL_LAB_SHADER_OUTPUT}/dxil"
    INCLUDES "${RL_ALGORITHM_SHADERS}"
    FOLDER "RenderLab")

# --- rl_add_target ---------------------------------------------------------
# One declaration per experiment: target kind, sources, shaders and config.
#
#   rl_add_target(<name>
#       KIND     LIBRARY | EXECUTABLE      (default LIBRARY)
#       SOURCES  <list>                    (required, explicit, no globbing)
#       SHADERS  <list>  CFG <shaders.cfg> (compiled into bin/shaders/renderlab)
#       CONFIG   <config.json>             (default --config of the executable)
#       INCLUDES / LINK / PROJECT_NAME     (optional)
#   )
function(rl_add_target name)
    set(options)
    set(oneValueArgs KIND CFG CONFIG PROJECT_NAME)
    set(multiValueArgs SOURCES SHADERS INCLUDES LINK)
    cmake_parse_arguments(RL "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT RL_SOURCES)
        message(FATAL_ERROR "rl_add_target(${name}): SOURCES is required")
    endif()

    if(NOT RL_KIND)
        set(RL_KIND LIBRARY)
    endif()

    if(RL_KIND STREQUAL "EXECUTABLE")
        add_executable(${name} WIN32 ${RL_SOURCES})
        target_link_libraries(${name} PRIVATE rl_host)
    elseif(RL_KIND STREQUAL "LIBRARY")
        add_library(${name} STATIC ${RL_SOURCES})
        target_link_libraries(${name} PUBLIC rl_types rl_algorithms rl_nvrhi)
    else()
        message(FATAL_ERROR "rl_add_target(${name}): KIND must be LIBRARY or EXECUTABLE")
    endif()

    if(RL_INCLUDES)
        target_include_directories(${name} PUBLIC ${RL_INCLUDES})
    endif()

    if(RL_LINK)
        target_link_libraries(${name} PUBLIC ${RL_LINK})
    endif()

    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /utf-8 /external:anglebrackets /external:W0)
    endif()

    if(RL_CFG)
        if(NOT RL_SHADERS)
            message(FATAL_ERROR "rl_add_target(${name}): SHADERS is required when CFG is given")
        endif()

        if(NOT RL_PROJECT_NAME)
            set(RL_PROJECT_NAME "${name}")
        endif()

        # Relative paths are resolved against the caller's CMakeLists.
        get_filename_component(RL_CFG_ABS "${RL_CFG}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        set(RL_SHADERS_ABS "")
        foreach(shader_file IN LISTS RL_SHADERS)
            get_filename_component(shader_file_abs "${shader_file}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            list(APPEND RL_SHADERS_ABS "${shader_file_abs}")
        endforeach()

        donut_compile_shaders(
            TARGET ${name}Shaders
            PROJECT_NAME "${RL_PROJECT_NAME}"
            CONFIG "${RL_CFG_ABS}"
            SOURCES ${RL_SHADERS_ABS}
            DXIL "${RL_LAB_SHADER_OUTPUT}/dxil"
            INCLUDES "${RL_ALGORITHM_SHADERS}" ${RL_INCLUDES}
            FOLDER "RenderLab")

        add_dependencies(${name} ${name}Shaders)
        add_dependencies(${name} RenderLabShaders)
    endif()

    # The experiment's config.json is copied next to the executable, where the host picks it up as
    # <executable name>.json. The source of truth stays in the sample directory.
    if(RL_CONFIG)
        get_filename_component(RL_CONFIG_ABS "${RL_CONFIG}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RL_CONFIG_ABS}"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<TARGET_FILE_BASE_NAME:${name}>.json"
            COMMENT "Copying the ${name} config next to the executable")
    endif()
endfunction()

# --- experiments -----------------------------------------------------------
add_subdirectory(samples/starter)
add_subdirectory(samples/lab_forward)
add_subdirectory(samples/lab_contract)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT PrismLabForward)

# --- optional: Donut examples ----------------------------------------------
# The all-projects preset adds every Donut-Samples target this D3D12 configuration supports.
option(PRISMLAB_BUILD_ALL_PROJECTS "Build PrismLab and all applicable Donut examples" OFF)
if(PRISMLAB_BUILD_ALL_PROJECTS)
    set(donut_samples_dir "${CMAKE_CURRENT_SOURCE_DIR}/external/Donut-Samples")

    # Work Graphs needs the newer D3D12 headers and runtime from the Agility SDK.
    set(DONUT_D3D_AGILITY_SDK_URL
        "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.717.1-preview")
    set(DONUT_D3D_AGILITY_SDK_FETCH_DIR "" CACHE STRING "" FORCE)
    include("${DONUT_DIR}/cmake/FetchAgilitySDK.cmake")
    file(COPY
        "${DONUT_D3D_AGILITY_SDK_PATH}/build/native/bin/x64/D3D12Core.dll"
        "${DONUT_D3D_AGILITY_SDK_PATH}/build/native/bin/x64/D3D12SDKLayers.dll"
        DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/D3D12/")

    set(donut_example_dirs
        basic_triangle vertex_buffer deferred_shading headless bindless_rendering variable_shading
        rt_triangle rt_shadows rt_bindless rt_particles meshlets threaded_rendering async_compute
        work_graphs rt_reflections)

    add_custom_target(PrismLabAllProjects)
    add_dependencies(PrismLabAllProjects PrismLabStarter PrismLabForward PrismLabContract)

    add_subdirectory("${donut_samples_dir}/feature_demo" "${CMAKE_BINARY_DIR}/examples/feature_demo" EXCLUDE_FROM_ALL)
    add_dependencies(PrismLabAllProjects feature_demo)

    foreach(example IN LISTS donut_example_dirs)
        add_subdirectory(
            "${donut_samples_dir}/examples/${example}"
            "${CMAKE_BINARY_DIR}/examples/${example}"
            EXCLUDE_FROM_ALL)

        # Vulkan-only variants are not registered; the D3D12 one carries a suffix.
        if(TARGET ${example})
            add_dependencies(PrismLabAllProjects ${example})
        elseif(TARGET ${example}_d3d12)
            add_dependencies(PrismLabAllProjects ${example}_d3d12)
        endif()
    endforeach()
endif()
