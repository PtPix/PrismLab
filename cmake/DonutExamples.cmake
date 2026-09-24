# --- optional: Donut examples ----------------------------------------------
# The all-projects preset adds every Donut-Samples target this D3D12 configuration supports.
option(PRISM_BUILD_ALL_PROJECTS "Build Prism and all applicable Donut examples" OFF)
if(PRISM_BUILD_ALL_PROJECTS)
    set(donut_samples_dir "${CMAKE_CURRENT_SOURCE_DIR}/external/Donut-Samples")

    # Work Graphs needs the newer D3D12 headers and runtime from the Agility SDK.
    set(DONUT_D3D_AGILITY_SDK_URL
        "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.717.1-preview")
    set(DONUT_D3D_AGILITY_SDK_FETCH_DIR "" CACHE STRING "" FORCE)
    include("${DONUT_DIR}/cmake/FetchAgilitySDK.cmake")
    add_custom_target(PrismAgilityRuntime
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PRISM_RUNTIME_DIR}/D3D12"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DONUT_D3D_AGILITY_SDK_PATH}/build/native/bin/x64/D3D12Core.dll"
            "${DONUT_D3D_AGILITY_SDK_PATH}/build/native/bin/x64/D3D12SDKLayers.dll"
            "${PRISM_RUNTIME_DIR}/D3D12/")

    set(donut_example_dirs
        basic_triangle vertex_buffer deferred_shading headless bindless_rendering variable_shading
        rt_triangle rt_shadows rt_bindless rt_particles meshlets threaded_rendering async_compute
        work_graphs rt_reflections)

    add_custom_target(PrismAllProjects)
    add_dependencies(PrismAllProjects PrismStarter PrismForward PrismContract PrismAgilityRuntime)

    add_subdirectory("${donut_samples_dir}/feature_demo" "${CMAKE_BINARY_DIR}/examples/feature_demo" EXCLUDE_FROM_ALL)
    add_dependencies(PrismAllProjects feature_demo)

    foreach(example IN LISTS donut_example_dirs)
        add_subdirectory(
            "${donut_samples_dir}/examples/${example}"
            "${CMAKE_BINARY_DIR}/examples/${example}"
            EXCLUDE_FROM_ALL)

        # Vulkan-only variants are not registered; the D3D12 one carries a suffix.
        if(TARGET ${example})
            add_dependencies(PrismAllProjects ${example})
        elseif(TARGET ${example}_d3d12)
            add_dependencies(PrismAllProjects ${example}_d3d12)
        endif()
    endforeach()
endif()

