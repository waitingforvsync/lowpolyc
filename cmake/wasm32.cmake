# wasm32.cmake
# 
# Toolchain file for freestanding wasm32 with clang: no WASI, no
# emscripten, no libc.
#
# Use with:
#   cmake -B build-wasm -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/wasm32.cmake
#
# Linking needs LLVM's wasm-ld beside clang (on Ubuntu: the lld-21 package).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

# Keep a compiler given on the command line (CMake Tools passes its kit's) exactly as given:
# changing it on a later configure makes CMake throw the cache away, toolchain and all. Its LLVM
# tools sit beside the real binary, which /usr/bin/clang is usually a link to.
if(NOT CMAKE_C_COMPILER)
    find_program(RICHC_WASM_CLANG NAMES clang REQUIRED)
    set(CMAKE_C_COMPILER "${RICHC_WASM_CLANG}")
endif()
get_filename_component(RICHC_WASM_CLANG_NAME "${CMAKE_C_COMPILER}" NAME)
if(NOT RICHC_WASM_CLANG_NAME MATCHES "^clang")
    message(FATAL_ERROR "wasm32 builds need clang, not ${CMAKE_C_COMPILER} (in VS Code, pick a Clang kit)")
endif()
file(REAL_PATH "${CMAKE_C_COMPILER}" RICHC_WASM_CLANG_REAL)
get_filename_component(RICHC_LLVM_BIN "${RICHC_WASM_CLANG_REAL}" DIRECTORY)

set(CMAKE_C_COMPILER_TARGET wasm32)

# GNU ar cannot index wasm objects, and wasm-ld needs the index
find_program(CMAKE_AR NAMES llvm-ar HINTS "${RICHC_LLVM_BIN}" REQUIRED)
find_program(CMAKE_RANLIB NAMES llvm-ranlib HINTS "${RICHC_LLVM_BIN}" REQUIRED)

set(CMAKE_C_FLAGS_INIT "-ffreestanding -msimd128")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib -Wl,--no-entry")

# There is nothing to link a test program against yet at configure time
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
