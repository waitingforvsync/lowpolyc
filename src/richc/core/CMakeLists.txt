# ---- core static library ----

add_library(richc STATIC
    src/arena.c
    src/bitset.c
    src/file.c
    src/hash.c
    src/hash_index.c
    src/mstr.c
    src/mstr_float.c
    src/str.c
    src/math/acos_f32.c
    src/math/acos_f64.c
    src/math/asin_f32.c
    src/math/asin_f64.c
    src/math/atan2_f32.c
    src/math/atan2_f64.c
    src/math/atan_f32.c
    src/math/atan_f64.c
    src/math/cbrt_f32.c
    src/math/cbrt_f64.c
    src/math/cos_f32.c
    src/math/cos_f64.c
    src/math/exp_data_f32.c
    src/math/exp_data_f64.c
    src/math/exp_f32.c
    src/math/exp_f64.c
    src/math/log2_data_f32.c
    src/math/log2_data_f64.c
    src/math/log2_f32.c
    src/math/log2_f64.c
    src/math/log_data_f32.c
    src/math/log_data_f64.c
    src/math/log_f32.c
    src/math/log_f64.c
    src/math/mat22f.c
    src/math/mat23f.c
    src/math/mat33f.c
    src/math/mat34f.c
    src/math/mat44f.c
    src/math/pow_data_f32.c
    src/math/pow_data_f64.c
    src/math/pow_f32.c
    src/math/pow_f64.c
    src/math/quatf.c
    src/math/rational.c
    src/math/sin_f32.c
    src/math/sin_f64.c
    src/math/solve.c
    src/math/tan_f32.c
    src/math/tan_f64.c
    src/math/trig_kernel_f32.c
    src/math/trig_kernel_f64.c
    src/math/trig_reduce_f32.c
    src/math/trig_reduce_f64.c
    src/math/trig_reduce_large.c
    include/richc/arena.h
    include/richc/array.template.h
    include/richc/bitset.h
    include/richc/bytes.h
    include/richc/file.h
    include/richc/genpool.template.h
    include/richc/handle.template.h
    include/richc/hash.h
    include/richc/hash_index.h
    include/richc/hash_index.template.h
    include/richc/hash_trie.template.h
    include/richc/hash_trie_foreach.template.h
    include/richc/lower_bound.template.h
    include/richc/macros.h
    include/richc/mem.h
    include/richc/mstr.h
    include/richc/pool.template.h
    include/richc/print.h
    include/richc/str.h
    include/richc/math/box2f.h
    include/richc/math/box2i.h
    include/richc/math/mat22f.h
    include/richc/math/mat23f.h
    include/richc/math/mat33f.h
    include/richc/math/mat34f.h
    include/richc/math/mat44f.h
    include/richc/math/math.h
    include/richc/math/quatf.h
    include/richc/math/rational.h
    include/richc/math/solve.h
    include/richc/math/vec2f.h
    include/richc/math/vec2i.h
    include/richc/math/vec3f.h
    include/richc/math/vec3i.h
    include/richc/math/vec4f.h
    include/richc/math/array/box2f.h
    include/richc/math/array/box2i.h
    include/richc/math/array/mat22f.h
    include/richc/math/array/mat23f.h
    include/richc/math/array/mat33f.h
    include/richc/math/array/mat34f.h
    include/richc/math/array/mat44f.h
    include/richc/math/array/quatf.h
    include/richc/math/array/rational.h
    include/richc/math/array/vec2f.h
    include/richc/math/array/vec2i.h
    include/richc/math/array/vec3f.h
    include/richc/math/array/vec3i.h
    include/richc/math/array/vec4f.h
    src/fs.h
    src/heap.h
    src/math/exp_log_f32.h
    src/math/exp_log_f64.h
    src/math/trig.h
)

target_include_directories(richc
    PUBLIC  include
    PRIVATE src)

# One platform backend per target, chosen here rather than by #if
if(RICHC_WASM)
    target_sources(richc PRIVATE
        src/platform/wasm/fs.c
        src/platform/wasm/heap.c
        src/platform/wasm/print.c
        src/platform/wasm/runtime.c)

    # The page scripts that supply the file and print imports. The app's own page copies them into
    # its site, under richc/.
    set_property(TARGET richc PROPERTY RICHC_WEB_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm/fs.js
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm/print.js)
elseif(WIN32)
    target_sources(richc PRIVATE
        src/platform/win32/fs.c
        src/platform/win32/heap.c
        src/platform/win32/print.c)
else()
    target_sources(richc PRIVATE
        src/platform/posix/fs.c
        src/platform/posix/heap.c
        src/platform/posix/print.c)
endif()

# ---- test framework (native only) ----

# Built whenever the target is native, so lowpoly's tests can use it even when
# richc's own tests are off.
if(NOT RICHC_WASM)
    add_library(richc_test STATIC
        src/test.c
        include/richc/test.h
    )

    target_link_libraries(richc_test PUBLIC richc)
    if(NOT WIN32)
        # libm is part of the CRT on Windows; elsewhere it is a separate library
        target_link_libraries(richc_test PRIVATE m)
    endif()
endif()

# ---- tests ----

if(RICHC_BUILD_TESTS)
    add_subdirectory(test)
endif()
