# ---- lowpoly ----

# Compile lowpoly's tests into the executable and run them (lowpoly --test)
# after each build, so a failing test fails the build. The tests sit at the
# bottom of each source file under #ifdef LOWPOLY_TESTS. Native only: off for
# wasm32, and an error to turn on there.
if(RICHC_WASM)
    option(LOWPOLY_BUILD_TESTS "Build lowpoly's tests into the executable and run them" OFF)
    if(LOWPOLY_BUILD_TESTS)
        message(FATAL_ERROR "LOWPOLY_BUILD_TESTS: the tests are native only and cannot be built for wasm32")
    endif()
else()
    option(LOWPOLY_BUILD_TESTS "Build lowpoly's tests into the executable and run them" ON)
endif()

if(NOT RICHC_BUILD_APP)
    message(FATAL_ERROR "lowpoly needs the richc app layer: configure with RICHC_BUILD_APP=ON")
endif()

add_executable(lowpoly
    editor.c
    editor.h
)

target_include_directories(lowpoly PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lowpoly PRIVATE richc_app)

# One entry point per target, chosen here rather than by #if
if(RICHC_WASM)
    target_sources(lowpoly PRIVATE wasm/main.c)
else()
    target_sources(lowpoly PRIVATE native/main.c)
endif()

# ---- tests ----

if(LOWPOLY_BUILD_TESTS)
    target_compile_definitions(lowpoly PRIVATE LOWPOLY_TESTS)
    target_link_libraries(lowpoly PRIVATE richc_test)

    add_custom_command(
        TARGET lowpoly POST_BUILD
        COMMAND $<TARGET_FILE:lowpoly> --test
        WORKING_DIRECTORY $<TARGET_FILE_DIR:lowpoly>
        COMMENT "Running lowpoly tests"
        VERBATIM
    )
endif()

# ---- web page (wasm only) ----

# The site GitHub Pages serves: lowpoly.wasm, the files in web/ and richc_app's
# scripts (in richc/), gathered in <build>/site. tools/serve_site.sh serves
# build/wasm/site over http to try it locally.
if(RICHC_WASM)
    set(LOWPOLY_SITE_DIR ${CMAKE_BINARY_DIR}/site)
    set_target_properties(lowpoly PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LOWPOLY_SITE_DIR})

    set(LOWPOLY_SITE_FILES)
    foreach(file index.html lowpoly.js)
        add_custom_command(
            OUTPUT ${LOWPOLY_SITE_DIR}/${file}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/web/${file} ${LOWPOLY_SITE_DIR}/${file}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/${file}
            VERBATIM
        )
        list(APPEND LOWPOLY_SITE_FILES ${LOWPOLY_SITE_DIR}/${file})
    endforeach()

    # richc_app's scripts, which lowpoly.js imports from richc/
    get_target_property(RICHC_WEB_FILES richc_app RICHC_WEB_FILES)
    foreach(source ${RICHC_WEB_FILES})
        get_filename_component(file ${source} NAME)
        add_custom_command(
            OUTPUT ${LOWPOLY_SITE_DIR}/richc/${file}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${source} ${LOWPOLY_SITE_DIR}/richc/${file}
            DEPENDS ${source}
            VERBATIM
        )
        list(APPEND LOWPOLY_SITE_FILES ${LOWPOLY_SITE_DIR}/richc/${file})
    endforeach()

    add_custom_target(lowpoly_site ALL DEPENDS ${LOWPOLY_SITE_FILES})
endif()
