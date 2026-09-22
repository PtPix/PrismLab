cmake_minimum_required(VERSION 3.24)
project(Prism LANGUAGES C CXX)

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
# algorithms/   decoupled core: HLSL that reaches its host only through PRISM_* bridge functions.
#               It must not include framework headers.
# framework/    everything that owns Donut / NVRHI: types, execution helpers, Donut facilities,
#               pipelines and the application host. This half uses donut::math directly.
# samples/      experiments: an Experiment subclass, its shaders and its own config.json.
#
# Libraries: prism_types <- prism_nvrhi <- prism_donut <- prism_host, plus prism_pipelines; prism_host <- samples.
# ---------------------------------------------------------------------------
set(PRISM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(PRISM_ALGORITHM_SHADERS "${PRISM_ROOT}/algorithms/shaders")
set(PRISM_SHADER_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/prism")

# Data algorithms consume, shared with the framework; also the include root for <framework/...>.
add_library(prism_types INTERFACE)
target_include_directories(prism_types INTERFACE "${PRISM_ROOT}")
target_link_libraries(prism_types INTERFACE donut_core)

# Reusable HLSL cores plus the C++ headers shared with them (e.g. DebugView_cb.h).
add_library(prism_algorithms INTERFACE)
target_include_directories(prism_algorithms INTERFACE "${PRISM_ALGORITHM_SHADERS}")

# NVRHI execution helpers: shader variants, named transient resources, GPU timing, readback,
# pipeline/draw helpers, geometry batch view.
add_library(prism_nvrhi STATIC
    framework/nvrhi/BufferPool.cpp
    framework/nvrhi/DebugView.cpp
    framework/nvrhi/Formats.cpp
    framework/nvrhi/GpuProfiler.cpp
    framework/nvrhi/PipelineUtils.cpp
    framework/nvrhi/RenderTargetPool.cpp
    framework/nvrhi/ResourceTable.cpp
    framework/nvrhi/ShaderLibrary.cpp
    framework/nvrhi/TextureReadback.cpp)
target_link_libraries(prism_nvrhi PUBLIC prism_types prism_algorithms donut_core nvrhi)

# Donut facilities: configuration, camera controller, scene sources, light and geometry conversion.
add_library(prism_donut STATIC
    framework/donut/CameraController.cpp
    framework/donut/HostConfig.cpp
    framework/donut/LightAdapter.cpp
    framework/donut/ProceduralScene.cpp
    framework/donut/SceneHost.cpp)
target_link_libraries(prism_donut PUBLIC prism_nvrhi donut_engine donut_app)

# Reusable pass wiring; the forward scene path is the baseline every experiment can consume.
add_library(prism_pipelines STATIC
    framework/pipelines/SceneForwardPipeline.cpp)
target_link_libraries(prism_pipelines PUBLIC prism_types donut_render donut_engine)

# Application host: window, device, camera, scene, UI, timing, frame loop, command line, WinMain.
add_library(prism_host STATIC
    framework/host/Application.cpp
    framework/host/CommandLine.cpp
    framework/host/Entry.cpp
    framework/host/ImageReference.cpp
    framework/host/ExperimentHost.cpp
    framework/host/Metrics.cpp
    framework/host/Params.cpp
    framework/host/UiOverlay.cpp)
target_link_libraries(prism_host PUBLIC prism_donut prism_pipelines prism_nvrhi donut_app donut_render donut_engine)

foreach(prism_target prism_nvrhi prism_donut prism_pipelines prism_host)
    if(MSVC)
        # /W4 for our own code, no warnings from Donut / NVRHI / imgui headers.
        target_compile_options(${prism_target} PRIVATE /W4 /utf-8 /external:anglebrackets /external:W0)
    endif()
endforeach()

# The framework's own shader (the shared debug view) is available to every experiment.
donut_compile_shaders(
    TARGET PrismShaders
    PROJECT_NAME "Prism framework shaders"
    CONFIG "${PRISM_ROOT}/algorithms/shaders/Prism/Debug/shaders.cfg"
    SOURCES "${PRISM_ROOT}/algorithms/shaders/Prism/Debug/DebugView.hlsl"
    DXIL "${PRISM_SHADER_OUTPUT}/dxil"
    INCLUDES "${PRISM_ALGORITHM_SHADERS}"
    FOLDER "Prism")

# --- prism_add_target ---------------------------------------------------------
# One declaration per experiment: target kind, sources, shaders and config.
#
#   prism_add_target(<name>
#       KIND     LIBRARY | EXECUTABLE    (default LIBRARY)
#       SOURCES  <list>                  (required, explicit, no globbing)
#       SHADERS  <list>                  <file.hlsl>:<stage>; entry point is main_<stage>
#       CONFIG   <config.json>           (default --config of the executable)
#       INCLUDES / LINK / PROJECT_NAME   (optional)
#   )
function(prism_add_target name)
    set(options)
    set(oneValueArgs KIND CONFIG PROJECT_NAME)
    set(multiValueArgs SOURCES SHADERS INCLUDES LINK)
    cmake_parse_arguments(PRISM "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT PRISM_SOURCES)
        message(FATAL_ERROR "prism_add_target(${name}): SOURCES is required")
    endif()

    if(NOT PRISM_KIND)
        set(PRISM_KIND LIBRARY)
    endif()

    if(PRISM_KIND STREQUAL "EXECUTABLE")
        add_executable(${name} WIN32 ${PRISM_SOURCES})
        target_link_libraries(${name} PRIVATE prism_host)
    elseif(PRISM_KIND STREQUAL "LIBRARY")
        add_library(${name} STATIC ${PRISM_SOURCES})
        target_link_libraries(${name} PUBLIC prism_types prism_algorithms prism_nvrhi)
    else()
        message(FATAL_ERROR "prism_add_target(${name}): KIND must be LIBRARY or EXECUTABLE")
    endif()

    if(PRISM_INCLUDES)
        target_include_directories(${name} PUBLIC ${PRISM_INCLUDES})
    endif()

    if(PRISM_LINK)
        target_link_libraries(${name} PUBLIC ${PRISM_LINK})
    endif()

    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /utf-8 /external:anglebrackets /external:W0)
    endif()

    if(PRISM_SHADERS)
        if(NOT PRISM_PROJECT_NAME)
            set(PRISM_PROJECT_NAME "${name}")
        endif()

        # shaders.cfg is generated from the SHADERS list instead of being maintained by hand:
        # each entry is <file.hlsl>:<stage> and expands to "<file> -T <stage> -E main_<stage>".
        set(cfg_path "${CMAKE_CURRENT_BINARY_DIR}/${name}.shaders.cfg")
        set(cfg_lines "")
        set(PRISM_SHADERS_ABS "")
        foreach(entry IN LISTS PRISM_SHADERS)
            string(REPLACE ":" ";" parts "${entry}")
            list(LENGTH parts part_count)
            if(NOT part_count EQUAL 2)
                message(FATAL_ERROR "prism_add_target(${name}): SHADERS entry '${entry}' must be <file.hlsl>:<stage>")
            endif()

            list(GET parts 0 shader_name)
            list(GET parts 1 stage)

            get_filename_component(shader_abs "${shader_name}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            list(APPEND PRISM_SHADERS_ABS "${shader_abs}")
            list(APPEND cfg_lines "${shader_abs} -T ${stage} -E main_${stage}")
        endforeach()

        file(WRITE "${cfg_path}" "")
        foreach(line IN LISTS cfg_lines)
            file(APPEND "${cfg_path}" "${line}\n")
        endforeach()

        # ShaderMake builds each output path as "<output dir>/<config entry>". The generated entries
        # are absolute, and filesystem path joining drops the left side when the right one is
        # absolute, so the names have to be flattened to the file name.
        donut_compile_shaders(
            TARGET ${name}Shaders
            PROJECT_NAME "${PRISM_PROJECT_NAME}"
            CONFIG "${cfg_path}"
            SOURCES ${PRISM_SHADERS_ABS}
            DXIL "${PRISM_SHADER_OUTPUT}/dxil"
            INCLUDES "${PRISM_ALGORITHM_SHADERS}" ${PRISM_INCLUDES}
            SHADERMAKE_OPTIONS_DXIL "--flatten"
            FOLDER "Prism")

        add_dependencies(${name} ${name}Shaders)
        add_dependencies(${name} PrismShaders)
    endif()

    # The experiment's config.json is copied next to the executable, where the host picks it up as
    # <executable name>.json. The source of truth stays in the sample directory.
    if(PRISM_CONFIG)
        get_filename_component(PRISM_CONFIG_ABS "${PRISM_CONFIG}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PRISM_CONFIG_ABS}"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<TARGET_FILE_BASE_NAME:${name}>.json"
            COMMENT "Copying the ${name} config next to the executable")
    endif()
endfunction()

# --- experiments -----------------------------------------------------------
add_subdirectory(samples/starter)
add_subdirectory(samples/forward)
add_subdirectory(samples/contract)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT PrismForward)

# --- optional: Donut examples ----------------------------------------------
# The all-projects preset adds every Donut-Samples target this D3D12 configuration supports.
option(PRISM_BUILD_ALL_PROJECTS "Build Prism and all applicable Donut examples" OFF)
if(PRISM_BUILD_ALL_PROJECTS)
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

    add_custom_target(PrismAllProjects)
    add_dependencies(PrismAllProjects PrismStarter PrismForward PrismContract)

    add_subdirectory("${donut_samples_dir}/feature_demo" "${CMAKE_BINARY_DIR}/examples/feature_demo" EXCLUDE_FROM_ALL)
    add_dependencies(PrismAllProjects feature_demo)

    foreach(example IN LISTS donut_example_dirs)
        add_subdirectory(
            "${donut_samples_dir}/examples/${example}"
            "${CMAKE_BINARY_DIR}/examples/${example}"
            EXCLUDE_FROM_ALL)

        # Vulkan-only variants are not registered; the D3D12 one carries a suffix.
        if(TARGET ${example})
            add_dependencies(PrismAllProjects ${example})
        elseif(TARGET ${example}_d3d12)
            add_dependencies(PrismAllProjects ${example}_d3d12)
        endif()
    endforeach()
endif()
