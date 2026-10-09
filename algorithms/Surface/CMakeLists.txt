add_library(PrismSurface STATIC
    DepthRenderer.h
    DepthRenderer.cpp
    DepthInputs.h
    DepthOutputs.h
    DepthSettings.h
    shaders/SurfaceDepthCb.h)
target_include_directories(PrismSurface PUBLIC "${PRISM_ROOT}")
target_link_libraries(PrismSurface PUBLIC nvrhi)
prism_target_defaults(PrismSurface)

prism_add_shader_package(PrismSurface
    SHADERS
        shaders/SurfaceDepth.hlsl:vs
    INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set_property(TARGET PrismSurface PROPERTY PRISM_SHADER_PACKAGES PrismSurface)
add_dependencies(PrismSurface PrismSurfaceShaders)
