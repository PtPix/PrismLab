# The current shadow module contains contracts and a reusable HLSL kernel.
# Add algorithm-owned C++ passes here when implementing the shadow pipeline.
add_library(prism_shadows INTERFACE)
target_link_libraries(prism_shadows INTERFACE prism_scene)
target_include_directories(prism_shadows INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
