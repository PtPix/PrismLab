# ForwardExperiment：共享前向管线 + 实验自己的调试视图 Pass。
# 没有相机、场景、UI、计时、shader 编译接线：这些分别由 prism_host 与 prism_add_target 提供。

prism_add_target(PrismForward
    KIND EXECUTABLE
    SOURCES
        forward.cpp
        forward.h
        debug_view_cb.h
    SHADERS
        debug_view.hlsl:ps
    CONFIG
        config.json
    PROJECT_NAME "Prism ForwardExperiment")
