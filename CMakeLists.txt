cmake_minimum_required(VERSION 3.25)
project(lowpolyc C)

# A toolchain file only applies to a fresh build directory. Naming one for a directory configured
# without it would silently give a native build, so we stop instead.
if(CMAKE_TOOLCHAIN_FILE MATCHES "wasm32" AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32")
    message(FATAL_ERROR
        "${CMAKE_BINARY_DIR} names the wasm32 toolchain but is a native build. Delete it and "
        "configure again (in VS Code: CMake: Delete Cache and Reconfigure).")
endif()

# The wasm32 toolchain file (cmake/wasm32.cmake) sets the processor.
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32")
    set(RICHC_WASM ON)

    # Set here, not in the toolchain file: CMake's Generic platform resets it
    set(CMAKE_EXECUTABLE_SUFFIX ".wasm")

    # Threads need memory shared between the page and its workers, which a browser allows only on
    # a cross-origin isolated page, so they are a build of their own. The memory is imported, so
    # every worker can instantiate the module on the same one, and its maximum is fixed up front.
    option(RICHC_WASM_THREADS "Build wasm32 with threads, on shared memory" OFF)
    set(RICHC_WASM_MAX_MEMORY 1073741824 CACHE STRING "Most memory a threaded wasm32 build can grow to, in bytes")
    if(RICHC_WASM_THREADS)
        add_compile_options(-matomics -mbulk-memory)
        # rc_wasm_threads_init gives the main thread its thread-local storage, so it must be in the
        # module, and the host must call it, even when nothing starts a thread
        add_link_options(
            -Wl,--shared-memory -Wl,--import-memory -Wl,--export-memory
            -Wl,--max-memory=${RICHC_WASM_MAX_MEMORY}
            -Wl,--export=__stack_pointer -Wl,--undefined=rc_wasm_threads_init)
    endif()
else()
    set(RICHC_WASM OFF)
endif()

# compile_commands.json in the build directory, for editors and clangd.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- options shared by every target ----

# C17 everywhere: -std=c17 on clang and gcc, /std:c17 on MSVC.
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/Zc:preprocessor /fp:precise /W4 /Gy /Gw)
    add_link_options($<$<NOT:$<CONFIG:Debug>>:/OPT:REF>)

    # clang-cl takes richc's clang paths, whose floor, ceil, trunc and round-even builtins need
    # SSE4.1 to be single instructions; without it they call functions MSVC's runtime lacks
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
        add_compile_options(-march=x86-64-v2)
    endif()
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wdouble-promotion
        -ffp-contract=off -fno-math-errno)

    if(NOT RICHC_WASM)
        add_compile_options(-ffunction-sections -fdata-sections)
        if(APPLE)
            add_link_options(-Wl,-dead_strip)
        else()
            add_link_options(-Wl,--gc-sections)
        endif()
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            add_compile_options(-march=x86-64-v2)
        endif()
    endif()
endif()

add_subdirectory(src)
