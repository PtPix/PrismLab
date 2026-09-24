# The compute shader belongs to a dependency, proving transitive package reload.
prism_add_shader_package(PrismTestCompute SHADERS TestPasses.hlsl:cs)
add_library(prism_test_compute INTERFACE)
set_property(TARGET prism_test_compute PROPERTY PRISM_SHADER_PACKAGES PrismTestCompute)
prism_add_target(PrismInfrastructureTests KIND EXECUTABLE
    SOURCES InfrastructureTests.cpp
    SHADERS TestPasses.hlsl:vs TestPasses.hlsl:ps
    LINK prism_test_compute CONFIG config.json)
target_compile_definitions(PrismInfrastructureTests PRIVATE
    PRISM_TEST_BIN="${PRISM_RUNTIME_DIR}" PRISM_TEST_DXC="${SHADERMAKE_DXC_PATH}")
donut_compile_shaders(TARGET PrismIncompatibleShaders
    CONFIG "${CMAKE_CURRENT_BINARY_DIR}/PrismTestCompute.shaders.cfg"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/TestPasses.hlsl"
    DXIL "${PRISM_RUNTIME_DIR}/test-incompatible"
    SHADERMAKE_OPTIONS_DXIL "--flatten -D TEST_WIDE=1")
add_dependencies(PrismInfrastructureTests PrismIncompatibleShaders)
add_test(NAME PrismInfrastructure COMMAND ${CMAKE_COMMAND}
    "-DEXE=$<TARGET_FILE:PrismInfrastructureTests>" -P "${CMAKE_CURRENT_SOURCE_DIR}/RunLifecycle.cmake")
add_test(NAME PrismInitializeFailure COMMAND ${CMAKE_COMMAND}
    "-DEXE=$<TARGET_FILE:PrismInfrastructureTests>" -DFAIL_INITIALIZE=ON
    "-DCONFIG_FILE=${CMAKE_CURRENT_SOURCE_DIR}/init-failure.json" -P "${CMAKE_CURRENT_SOURCE_DIR}/RunLifecycle.cmake")
set_tests_properties(PrismInfrastructure PrismInitializeFailure PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
