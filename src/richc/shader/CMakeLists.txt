# ---- shader library ----

# The WGSL translator. It depends only on core, so the shader tool can be built for the host
# without the app layer's window system.
add_library(richc_shader STATIC
    src/ast.c
    src/builtins.c
    src/check.c
    src/check_builtin.c
    src/check_call.c
    src/check_decl.c
    src/check_entry.c
    src/check_expr.c
    src/check_stmt.c
    src/check_type.c
    src/error.c
    src/glsl.c
    src/glsl_builtin.c
    src/glsl_expr.c
    src/glsl_names.c
    src/glsl_stmt.c
    src/layout.c
    src/lexer.c
    src/package.c
    src/package_c.c
    src/parser.c
    src/types.c
    include/richc/shader/shader.h
    include/richc/shader/wgsl.h
    src/ast.h
    src/builtins.h
    src/check.h
    src/checker.h
    src/error.h
    src/glsl.h
    src/glsl_writer.h
    src/layout.h
    src/lexer.h
    src/parser.h
    src/types.h
)

target_include_directories(richc_shader
    PUBLIC  include
    PRIVATE src)

target_link_libraries(richc_shader PUBLIC richc)

# ---- the shader tool, for the machine doing the building ----

if(RICHC_WASM)
    # A wasm build can't run what it builds, so a native build of richc alone, with the host's own
    # compiler, makes the tool
    include(ExternalProject)
    set(RICHC_SHADER_TOOL_DIR ${CMAKE_BINARY_DIR}/shader_tool)
    if(CMAKE_HOST_WIN32)
        set(RICHC_SHADER_TOOL_SUFFIX .exe)
    endif()
    set(RICHC_SHADER_TOOL
        ${RICHC_SHADER_TOOL_DIR}/src/richc/shader/tool/richc_shader_tool${RICHC_SHADER_TOOL_SUFFIX}
        CACHE INTERNAL "The shader tool")
    ExternalProject_Add(richc_shader_tool_host
        SOURCE_DIR ${PROJECT_SOURCE_DIR}
        BINARY_DIR ${RICHC_SHADER_TOOL_DIR}
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DRICHC_BUILD_APP=OFF -DRICHC_BUILD_TESTS=OFF
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target richc_shader_tool
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS ${RICHC_SHADER_TOOL}
        BUILD_ALWAYS ON
        EXCLUDE_FROM_ALL ON)
    set(RICHC_SHADER_TOOL_TARGET richc_shader_tool_host CACHE INTERNAL "The shader tool's target")
else()
    add_subdirectory(tool)
    set(RICHC_SHADER_TOOL $<TARGET_FILE:richc_shader_tool> CACHE INTERNAL "The shader tool")
    set(RICHC_SHADER_TOOL_TARGET richc_shader_tool CACHE INTERNAL "The shader tool's target")
endif()

# Translate WGSL files for target. Each becomes a header holding its shader package, which the
# target includes by the file's name: basic.wgsl as "shaders/basic.wgsl.h", defining basic_wgsl.
function(rc_add_shaders target)
    set(directory ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    foreach(source ${ARGN})
        get_filename_component(path ${source} ABSOLUTE)
        get_filename_component(file ${source} NAME)
        set(header ${directory}/${file}.h)
        add_custom_command(
            OUTPUT ${header}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${directory}
            COMMAND ${RICHC_SHADER_TOOL} ${path} ${header}
            DEPENDS ${path} ${RICHC_SHADER_TOOL_TARGET}
            COMMENT "Translating ${file}"
            VERBATIM)
        target_sources(${target} PRIVATE ${header})
    endforeach()
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${target} PRIVATE richc_shader)
endfunction()

if(RICHC_BUILD_TESTS)
    add_subdirectory(test)
endif()
