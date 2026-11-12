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
