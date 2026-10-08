prism_add_target(PrismDeferred
    KIND EXECUTABLE
    LINK PrismSurface prism_donut_scene
    SOURCES
        Deferred.h
        Deferred.cpp
        DepthPreview.h
        DepthPreview.cpp
        DepthDebugCb.h
    SHADERS
        DepthDebug.hlsl:ps
    INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}"
    CONFIG
        presets/default.json
)