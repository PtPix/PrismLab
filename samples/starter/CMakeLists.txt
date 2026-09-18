donut_compile_shaders_all_platforms(
    TARGET PrismLabStarterShaders
    PROJECT_NAME "PrismLab Starter"
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/shaders.cfg"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/triangle.hlsl"
    OUTPUT_BASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/starter"
)

add_executable(PrismLabStarter WIN32 main.cpp)
target_link_libraries(PrismLabStarter PRIVATE donut_app donut_engine)
add_dependencies(PrismLabStarter PrismLabStarterShaders)
if(MSVC)
    target_compile_options(PrismLabStarter PRIVATE /W4 /utf-8)
endif()
