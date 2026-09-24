donut_compile_shaders_all_platforms(
    TARGET PrismDonutTriangleShaders
    PROJECT_NAME "Prism Starter"
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/shaders.cfg"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/triangle.hlsl"
    OUTPUT_BASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/starter"
)

add_executable(PrismDonutTriangle WIN32 main.cpp)
target_link_libraries(PrismDonutTriangle PRIVATE donut_app donut_engine)
add_dependencies(PrismDonutTriangle PrismDonutTriangleShaders)
if(MSVC)
    # /W4 for our own code, no warnings from Donut / NVRHI / imgui headers.
    target_compile_options(PrismDonutTriangle PRIVATE /W4 /utf-8 /external:anglebrackets /external:W0)
endif()
