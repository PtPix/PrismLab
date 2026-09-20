# ForwardLab: 用共享前向管线渲染场景，并加一个实验自己的调试视图 Pass。
# 没有相机、场景、UI、计时代码：这些都在 rl_host 里。

file(GLOB lab_shaders "${CMAKE_CURRENT_SOURCE_DIR}/*.hlsl")

donut_compile_shaders(
    TARGET ForwardLabShaders
    PROJECT_NAME "RenderLab ForwardLab"
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/shaders.cfg"
    SOURCES ${lab_shaders}
    DXIL "${RL_LAB_SHADER_OUTPUT}/dxil"
    INCLUDES "${RL_ALGORITHM_SHADERS}"
    FOLDER "RenderLab")

add_executable(PrismLabForward WIN32
    forward_lab.cpp
    forward_lab.h
    debug_view_cb.h
)
target_link_libraries(PrismLabForward PRIVATE rl_host)
add_dependencies(PrismLabForward ForwardLabShaders)

if(MSVC)
    target_compile_options(PrismLabForward PRIVATE /W4 /utf-8)
endif()
