donut_compile_shaders_all_platforms(
    TARGET PrismStarterShaders
    PROJECT_NAME "Prism Starter"
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/shaders.cfg"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/triangle.hlsl"
    OUTPUT_BASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/starter"
)

add_executable(PrismStarter WIN32 main.cpp)
target_link_libraries(PrismStarter PRIVATE donut_app donut_engine)
add_dependencies(PrismStarter PrismStarterShaders)
if(MSVC)
    # /W4 for our own code, no warnings from Donut / NVRHI / imgui headers.
    target_compile_options(PrismStarter PRIVATE /W4 /utf-8 /external:anglebrackets /external:W0)
endif()
