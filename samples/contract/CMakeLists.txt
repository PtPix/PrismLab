# ContractExperiment：M1 契约自检（矩阵约定、深度重建、CPU/GPU 一致性）。
# 与 ForwardExperiment 一样，没有相机/场景/UI/计时代码。

prism_add_target(PrismContract
    KIND EXECUTABLE
    SOURCES
        contract.cpp
        contract.h
        contract_check_cb.h
    SHADERS
        contract_check.hlsl:cs
    CONFIG
        config.json
    PROJECT_NAME "Prism ContractExperiment")
