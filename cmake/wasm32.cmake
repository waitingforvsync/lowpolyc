# wasm32.cmake - toolchain file for freestanding wasm32 with clang: no WASI, no
# emscripten, no libc. Use with:
#   cmake -B build-wasm -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/wasm32.cmake
# Linking needs LLVM's wasm-ld beside clang (on Ubuntu: the lld-21 package).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

# Use the real clang binary so its LLVM tools can be found next to it
find_program(RICHC_WASM_CLANG NAMES clang REQUIRED)
file(REAL_PATH "${RICHC_WASM_CLANG}" RICHC_WASM_CLANG_REAL)
get_filename_component(RICHC_LLVM_BIN "${RICHC_WASM_CLANG_REAL}" DIRECTORY)

set(CMAKE_C_COMPILER "${RICHC_WASM_CLANG_REAL}")
set(CMAKE_C_COMPILER_TARGET wasm32)

# GNU ar cannot index wasm objects, and wasm-ld needs the index
find_program(CMAKE_AR NAMES llvm-ar HINTS "${RICHC_LLVM_BIN}" REQUIRED)
find_program(CMAKE_RANLIB NAMES llvm-ranlib HINTS "${RICHC_LLVM_BIN}" REQUIRED)

set(CMAKE_C_FLAGS_INIT "-ffreestanding -msimd128")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib -Wl,--no-entry")

# There is nothing to link a test program against yet at configure time
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
