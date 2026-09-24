prism_add_target(PrismContract
    KIND EXECUTABLE
    LINK prism_donut_scene
    SOURCES
        contract.cpp
        contract.h
        contract_check_cb.h
    SHADERS
        contract_check.hlsl:cs
    CONFIG
        config.json
    PROJECT_NAME "Prism ContractExperiment")
