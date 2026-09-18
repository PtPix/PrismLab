donut_compile_shaders_all_platforms(
    TARGET RenderLabStarterShaders
    PROJECT_NAME "RenderLab Starter"
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/shaders.cfg"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/triangle.hlsl"
    OUTPUT_BASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/starter"
)

add_executable(RenderLabStarter WIN32 main.cpp)
target_link_libraries(RenderLabStarter PRIVATE donut_app donut_engine)
add_dependencies(RenderLabStarter RenderLabStarterShaders)
if(MSVC)
    target_compile_options(RenderLabStarter PRIVATE /W4 /utf-8)
endif()
