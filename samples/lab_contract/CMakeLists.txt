# ContractLab: M1 契约自检（矩阵约定、深度重建、CPU/GPU 一致性）。
# 与 ForwardLab 一样，没有相机/场景/UI/计时代码。

file(GLOB lab_shaders "${CMAKE_CURRENT_SOURCE_DIR}/*.hlsl")

donut_compile_shaders(
    TARGET ContractLabShaders
    PROJECT_NAME "RenderLab ContractLab"
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/shaders.cfg"
    SOURCES ${lab_shaders}
    DXIL "${RL_LAB_SHADER_OUTPUT}/dxil"
    INCLUDES "${RL_ALGORITHM_SHADERS}"
    FOLDER "RenderLab")

add_executable(PrismLabContract WIN32
    contract_lab.cpp
    contract_lab.h
    contract_check_cb.h
)
target_link_libraries(PrismLabContract PRIVATE rl_host)
add_dependencies(PrismLabContract ContractLabShaders)

if(MSVC)
    target_compile_options(PrismLabContract PRIVATE /W4 /utf-8)
endif()
