# RenderLab 构建辅助：把"新增一个功能"的构建样板压到一行。
#
#   rl_add_target(<target>
#       KIND          LIBRARY | EXECUTABLE      （默认 LIBRARY）
#       SOURCES       <list>                    （必填，显式列出，不做 glob）
#       SHADERS       <list>                    （可选；与 CFG 一起给出时编译 shader）
#       CFG           <path>                    （可选，shaders.cfg，入口点由作者决定）
#       INCLUDES      <list>                    （可选；算法核心目录总会自动加入）
#       LINK          <list>                    （可选，额外的链接库）
#       PROJECT_NAME  <string>                  （可选，shader 编译日志里的名字）
#   )
#
# 它做四件事：创建目标并把惯例 include/警告选项设好、链接框架库、编译 shader 到统一输出目录、
# 建立依赖关系（新增 shader 或缺 add_dependencies 导致的"首次构建读不到 .bin"由此消失）。
# 它不做：不 glob 源文件、不生成 shaders.cfg、不引入插件或动态注册。

function(rl_add_target name)
    set(options)
    set(oneValueArgs KIND CFG PROJECT_NAME)
    set(multiValueArgs SOURCES SHADERS INCLUDES LINK)
    cmake_parse_arguments(RL "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT RL_SOURCES)
        message(FATAL_ERROR "rl_add_target(${name}): SOURCES is required")
    endif()

    if(NOT RL_KIND)
        set(RL_KIND LIBRARY)
    endif()

    if(RL_KIND STREQUAL "EXECUTABLE")
        add_executable(${name} WIN32 ${RL_SOURCES})
        target_link_libraries(${name} PRIVATE rl_host)
    elseif(RL_KIND STREQUAL "LIBRARY")
        add_library(${name} STATIC ${RL_SOURCES})
        target_link_libraries(${name} PUBLIC rl_contracts rl_algorithms rl_nvrhi_common)
    else()
        message(FATAL_ERROR "rl_add_target(${name}): KIND must be LIBRARY or EXECUTABLE")
    endif()

    target_include_directories(${name} PUBLIC "${RL_ROOT}" "${RL_CONTRACTS_INCLUDE}")

    if(RL_INCLUDES)
        target_include_directories(${name} PUBLIC ${RL_INCLUDES})
    endif()

    if(RL_LINK)
        target_link_libraries(${name} PUBLIC ${RL_LINK})
    endif()

    if(MSVC)
        # /W4 for our own code, no warnings from Donut / NVRHI / imgui headers.
        target_compile_options(${name} PRIVATE /W4 /utf-8 /external:anglebrackets /external:W0)
    endif()

    if(RL_CFG)
        if(NOT RL_SHADERS)
            message(FATAL_ERROR "rl_add_target(${name}): SHADERS is required when CFG is given")
        endif()

        if(NOT RL_PROJECT_NAME)
            set(RL_PROJECT_NAME "${name}")
        endif()

        # 允许调用方写相对路径（相对于自己的 CMakeLists）
        get_filename_component(RL_CFG_ABS "${RL_CFG}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        set(RL_SHADERS_ABS "")
        foreach(shader_file IN LISTS RL_SHADERS)
            get_filename_component(shader_file_abs "${shader_file}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            list(APPEND RL_SHADERS_ABS "${shader_file_abs}")
        endforeach()

        set(shader_target "${name}Shaders")
        donut_compile_shaders(
            TARGET ${shader_target}
            PROJECT_NAME "${RL_PROJECT_NAME}"
            CONFIG "${RL_CFG_ABS}"
            SOURCES ${RL_SHADERS_ABS}
            DXIL "${RL_LAB_SHADER_OUTPUT}/dxil"
            INCLUDES "${RL_ALGORITHM_SHADERS}" ${RL_INCLUDES}
            FOLDER "RenderLab")

        add_dependencies(${name} ${shader_target})
    endif()

    # 框架自带的 shader（公共调试视图）对所有实验可用。
    add_dependencies(${name} RenderLabShaders)
endfunction()
