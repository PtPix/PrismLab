cmake_minimum_required(VERSION 3.24)
project(PrismLab LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This starter currently targets Windows and D3D12.")
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

# Default to our starter; upstream demos are enabled explicitly below.
set(DONUT_WITH_DX11 OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_DX12 ON CACHE BOOL "" FORCE)
set(DONUT_WITH_VULKAN OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_TINYEXR OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(DONUT_SHADERS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/framework")
add_subdirectory("${DONUT_DIR}" "${CMAKE_BINARY_DIR}/donut" EXCLUDE_FROM_ALL)
include("${DONUT_DIR}/compileshaders.cmake")
add_subdirectory(samples/starter)
add_subdirectory(samples/hostlab)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT PrismLabHost)

# The all-projects preset adds every Donut-Samples target supported by this
# D3D12 configuration. Vulkan-only and optional Aftermath targets are omitted.
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
        PrismLabHost
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
