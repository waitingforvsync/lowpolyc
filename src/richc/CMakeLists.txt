# ---- richc ----

# Build the app layer (windowing, input, gfx, ...) as well as core.
option(RICHC_BUILD_APP "Build the richc app libraries" ON)

# Build each layer's test executable and run it as a post-build step, so a
# failing test fails the build. Tests are native only: off for wasm32, and an
# error to turn on there.
if(RICHC_WASM)
    option(RICHC_BUILD_TESTS "Build and run the richc tests" OFF)
    if(RICHC_BUILD_TESTS)
        message(FATAL_ERROR "RICHC_BUILD_TESTS: the tests are native only and cannot be built for wasm32")
    endif()
else()
    option(RICHC_BUILD_TESTS "Build and run the richc tests" ON)
endif()

add_subdirectory(core)
if(RICHC_BUILD_APP)
    add_subdirectory(app)
endif()
