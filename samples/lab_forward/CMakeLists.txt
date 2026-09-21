# ForwardLab：共享前向管线 + 实验自己的调试视图 Pass。
# 没有相机、场景、UI、计时、shader 编译接线：这些分别由 rl_host 与 rl_add_target 提供。

rl_add_target(PrismLabForward
    KIND EXECUTABLE
    SOURCES
        forward_lab.cpp
        forward_lab.h
        debug_view_cb.h
    SHADERS
        debug_view.hlsl
    CFG
        shaders.cfg
    PROJECT_NAME "RenderLab ForwardLab")
