prism_add_target(PrismForward
    KIND EXECUTABLE
    LINK prism_donut_scene
    SOURCES
        forward.cpp
        forward.h
        debug_view_cb.h
    SHADERS
        debug_view.hlsl:ps
    CONFIG
        presets/default.json)
