# RenderLab host sample (M0)
# This milestone reuses Donut's forward shading pass, so there is no HLSL of our own;
# framework shaders are compiled by the donut_shaders dependency of donut_render / donut_app.

add_executable(PrismLabHost WIN32
    main.cpp
    host_config.h
    host_config.cpp
    procedural_scene.h
    procedural_scene.cpp
)

target_link_libraries(PrismLabHost PRIVATE donut_render donut_app donut_engine)

if(MSVC)
    target_compile_options(PrismLabHost PRIVATE /W4 /utf-8)
endif()
