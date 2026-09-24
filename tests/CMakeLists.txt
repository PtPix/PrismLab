add_subdirectory(cpu)
add_subdirectory(gpu/infrastructure)
add_test(NAME PrismContract COMMAND PrismContract --smoke-test=12 --no-vsync)
set_tests_properties(PrismContract PROPERTIES TIMEOUT 60 WORKING_DIRECTORY "$<TARGET_FILE_DIR:PrismContract>")
