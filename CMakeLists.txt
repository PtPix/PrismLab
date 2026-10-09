cmake_minimum_required(VERSION 3.24)
project(Prism LANGUAGES C CXX)

# Only Windows is supported at the moment, as Prism is built on top of D3D12.
if(NOT WIN32)
    message(FATAL_ERROR "Prism currently targets Windows and D3D12.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(PRISM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(PRISM_RUNTIME_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PRISM_RUNTIME_DIR}")
set(PRISM_SHADER_INCLUDE "${PRISM_ROOT}/framework/render/shaders/include")
set(DONUT_DIR "${PRISM_ROOT}/external/Donut-Samples/donut")
if(NOT EXISTS "${DONUT_DIR}/nvrhi/CMakeLists.txt")
    message(FATAL_ERROR "Dependencies missing. See README.md.")
endif()

# Donut configuration
set(DONUT_WITH_DX11 OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_DX12 ON CACHE BOOL "" FORCE)
set(DONUT_WITH_VULKAN OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_TINYEXR OFF CACHE BOOL "" FORCE)
set(DONUT_SHADERS_OUTPUT_DIR "${PRISM_RUNTIME_DIR}/shaders/framework")
add_subdirectory("${DONUT_DIR}" "${CMAKE_BINARY_DIR}/donut" EXCLUDE_FROM_ALL)
include("${DONUT_DIR}/compileshaders.cmake")
include(cmake/ShaderPackages.cmake)
include(cmake/PrismTargets.cmake)

add_subdirectory(framework)

add_subdirectory(algorithms/Surface)
add_subdirectory(algorithms/shadows)

add_subdirectory(samples/Deferred)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT PrismDeferred)
