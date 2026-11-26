# ---- richc ----

# Build the app layer (windowing, input, gfx, ...) as well as core.
option(RICHC_BUILD_APP "Build the richc app libraries" ON)

# Build each layer's test executable and run it as a post-build step, so a failing test fails the
# build. On wasm32, node runs them (tools/run_wasm_tests.mjs), so they are on when node is found.
if(RICHC_WASM)
    find_program(RICHC_NODE NAMES node nodejs)
    if(RICHC_NODE)
        option(RICHC_BUILD_TESTS "Build and run the richc tests" ON)
    else()
        option(RICHC_BUILD_TESTS "Build and run the richc tests" OFF)
    endif()
    if(RICHC_BUILD_TESTS AND NOT RICHC_NODE)
        message(FATAL_ERROR "RICHC_BUILD_TESTS: running the tests on wasm32 needs node, which was not found")
    endif()
else()
    option(RICHC_BUILD_TESTS "Build and run the richc tests" ON)
endif()

# richc_run_tests(<target> <comment>)
#
# Runs a test executable from its own directory after each build, so a failing test fails the
# build: natively the executable itself, and on wasm32 the module under node.
function(richc_run_tests target comment)
    if(RICHC_WASM)
        set(command ${RICHC_NODE} ${PROJECT_SOURCE_DIR}/tools/run_wasm_tests.mjs $<TARGET_FILE:${target}>)
    else()
        set(command $<TARGET_FILE:${target}>)
    endif()

    add_custom_command(
        TARGET ${target} POST_BUILD
        COMMAND ${command}
        WORKING_DIRECTORY $<TARGET_FILE_DIR:${target}>
        COMMENT "${comment}"
        VERBATIM
    )
endfunction()

add_subdirectory(core)
add_subdirectory(shader)
if(RICHC_BUILD_APP)
    add_subdirectory(app)
endif()
