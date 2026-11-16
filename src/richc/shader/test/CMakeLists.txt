# ---- shader test executable ----

add_executable(richc_shader_test
    main.c
    test_check.c
    test_corpus.c
    test_glsl.c
    test_lexer.c
    test_package.c
    test_parser.c
)

# The package tests compare a package the shader tool built with one made while they run
rc_add_shaders(richc_shader_test data/package_test.wgsl)

# Bundled fixtures are staged next to the test binary
add_custom_command(
    TARGET richc_shader_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:richc_shader_test>/data
    VERBATIM
)

# The tests reach richc_shader's private headers, to check each stage on its own
target_include_directories(richc_shader_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(richc_shader_test PRIVATE richc_shader richc_test)

# ---- run tests as a post-build step ----

# The tests save the GLSL they write in glsl_out, emptied first so nothing stale is checked
add_custom_command(
    TARGET richc_shader_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:richc_shader_test>/glsl_out
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:richc_shader_test>/glsl_out
    COMMAND $<TARGET_FILE:richc_shader_test>
    WORKING_DIRECTORY $<TARGET_FILE_DIR:richc_shader_test>
    COMMENT "Running richc shader tests"
    VERBATIM
)

# With glslang to hand, every program they wrote must compile. Without it, the check is skipped:
# on Ubuntu it comes in glslang-tools.
find_program(RICHC_GLSLANG glslangValidator)
if(RICHC_GLSLANG)
    add_custom_command(
        TARGET richc_shader_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DGLSLANG=${RICHC_GLSLANG}
            -DDIRECTORY=$<TARGET_FILE_DIR:richc_shader_test>/glsl_out
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_glsl.cmake
        COMMENT "Checking the GLSL the shader tests wrote"
        VERBATIM
    )
endif()
