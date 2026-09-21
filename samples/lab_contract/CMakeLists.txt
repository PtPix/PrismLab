# ContractLab：M1 契约自检（矩阵约定、深度重建、CPU/GPU 一致性）。
# 与 ForwardLab 一样，没有相机/场景/UI/计时代码。

rl_add_target(PrismLabContract
    KIND EXECUTABLE
    SOURCES
        contract_lab.cpp
        contract_lab.h
        contract_check_cb.h
    SHADERS
        contract_check.hlsl
    CFG
        shaders.cfg
    PROJECT_NAME "RenderLab ContractLab")
