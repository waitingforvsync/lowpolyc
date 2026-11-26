# ---- core static library ----

add_library(richc STATIC
    src/arena.c
    src/bitset.c
    src/file.c
    src/hash.c
    src/hash_index.c
    src/mstr.c
    src/mstr_float.c
    src/random.c
    src/str.c
    src/str_parse.c
    src/str_parse_float.c
    src/str_utf8.c
    src/zip/inflate.c
    src/math/acos_f32.c
    src/math/acos_f64.c
    src/math/asin_f32.c
    src/math/asin_f64.c
    src/math/atan2_f32.c
    src/math/atan2_f64.c
    src/math/atan_f32.c
    src/math/atan_f64.c
    src/math/atan_table.c
    src/math/cbrt_f32.c
    src/math/cbrt_f64.c
    src/math/cos_f32.c
    src/math/cos_f64.c
    src/math/exp_f32.c
    src/math/exp_f32_table.c
    src/math/exp_f64.c
    src/math/exp_f64_kernel.c
    src/math/log2_f32.c
    src/math/log2_f64.c
    src/math/log2_f64_table.c
    src/math/log_f32.c
    src/math/log_f32_table.c
    src/math/log_f64.c
    src/math/log_f64_table.c
    src/math/mat22f.c
    src/math/mat23f.c
    src/math/mat33f.c
    src/math/mat34f.c
    src/math/mat44f.c
    src/math/pow_f32.c
    src/math/pow_f64.c
    src/math/quatf.c
    src/math/rational.c
    src/math/sin_f32.c
    src/math/sin_f64.c
    src/math/solve.c
    src/math/tan_f32.c
    src/math/tan_f64.c
    src/math/trig_reduce.c
    src/math/trig_slow.c
    src/thread/cond.c
    src/thread/mutex.c
    src/thread/once.c
    src/thread/rwlock.c
    src/thread/semaphore.c
    src/thread/thread.c
    src/thread/tls.c
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
    include/richc/macros.h
    include/richc/mem.h
    include/richc/mstr.h
    include/richc/pool.template.h
    include/richc/print.h
    include/richc/random.h
    include/richc/str.h
    include/richc/time.h
    include/richc/algorithm/bitset_foreach.template.h
    include/richc/algorithm/genpool_foreach.template.h
    include/richc/algorithm/hash_index_foreach.template.h
    include/richc/algorithm/hash_trie_foreach.template.h
    include/richc/algorithm/lower_bound.template.h
    include/richc/algorithm/max_element.template.h
    include/richc/algorithm/min_element.template.h
    include/richc/algorithm/pool_foreach.template.h
    include/richc/algorithm/sort.template.h
    include/richc/algorithm/upper_bound.template.h
    include/richc/algorithm/private/begin.template.h
    include/richc/algorithm/private/end.template.h
    include/richc/array/bitset.h
    include/richc/array/f32.h
    include/richc/array/f64.h
    include/richc/array/i16.h
    include/richc/array/i32.h
    include/richc/array/i64.h
    include/richc/array/i8.h
    include/richc/array/mstr.h
    include/richc/array/str.h
    include/richc/array/u16.h
    include/richc/array/u32.h
    include/richc/array/u64.h
    include/richc/array/u8.h
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
    include/richc/thread/atomic.h
    include/richc/thread/cond.h
    include/richc/thread/mutex.h
    include/richc/thread/once.h
    include/richc/thread/rwlock.h
    include/richc/thread/semaphore.h
    include/richc/thread/spinlock.h
    include/richc/thread/thread.h
    include/richc/thread/tls.h
    include/richc/thread/private/atomic.template.h
    include/richc/thread/private/atomic_msvc.template.h
    include/richc/zip/inflate.h
    src/float_big.h
    src/str_parse.h
    src/math/arc.h
    src/math/exp_log_f32.h
    src/math/exp_log_f64.h
    src/math/trig.h
    src/platform/fs.h
    src/platform/futex.h
    src/platform/heap.h
    src/platform/os_thread.h
)

target_include_directories(richc
    PUBLIC  include
    PRIVATE src)

# One platform backend per target, chosen here rather than by #if
if(RICHC_WASM)
    target_sources(richc PRIVATE
        src/platform/wasm/fs.c
        src/platform/wasm/futex.c
        src/platform/wasm/heap.c
        src/platform/wasm/os_thread.c
        src/platform/wasm/print.c
        src/platform/wasm/runtime.c
        src/platform/wasm/time.c
        src/platform/wasm/wasm_thread.h)

    # The page scripts that supply the file, print and clock imports. The app's own page copies them
    # into its site, under richc/.
    set_property(TARGET richc PROPERTY RICHC_WEB_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm/fs.js
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm/print.js
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm/time.js)

    # A build with threads needs the shared memory and the pool of workers too
    if(RICHC_WASM_THREADS)
        set_property(TARGET richc APPEND PROPERTY RICHC_WEB_FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm/thread_worker.js
            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm/threads.js)
    endif()
elseif(WIN32)
    target_sources(richc PRIVATE
        src/platform/win32/fs.c
        src/platform/win32/futex.c
        src/platform/win32/heap.c
        src/platform/win32/os_thread.c
        src/platform/win32/print.c
        src/platform/win32/time.c)

    # WaitOnAddress, behind the futex
    target_link_libraries(richc PRIVATE synchronization)
else()
    target_sources(richc PRIVATE
        src/platform/posix/fs.c
        src/platform/posix/heap.c
        src/platform/posix/os_thread.c
        src/platform/posix/print.c
        src/platform/posix/time.c)

    # pthreads, which older glibc keeps out of libc
    find_package(Threads REQUIRED)
    target_link_libraries(richc PUBLIC Threads::Threads)

    # The futex is the one call Linux and macOS do differently
    if(APPLE)
        target_sources(richc PRIVATE src/platform/macos/futex.c)
    else()
        target_sources(richc PRIVATE src/platform/linux/futex.c)
    endif()
endif()

# ---- test framework ----

# Built whenever there are tests to build, so lowpoly's tests can use it even when richc's own are
# off. Leaving a test part-way through is setjmp natively, and a JavaScript throw on wasm.
add_library(richc_test STATIC
    src/test.c
    src/test_guard.h
    include/richc/test.h
)

target_link_libraries(richc_test PUBLIC richc)
target_include_directories(richc_test PRIVATE src)

# RC_CHECK's _Generic names its operand without evaluating it, but MSVC still makes any compound
# literal in it (every RC_STR) and then warns that it goes unused
if(MSVC AND NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(richc_test PUBLIC /wd4189)
endif()

if(RICHC_WASM)
    target_sources(richc_test PRIVATE src/platform/wasm/test.c)

    # test.js puts the stack pointer back after a throw. With the stack first, below the static
    # data, a test that overflows it traps instead of overwriting that data.
    target_link_options(richc_test PUBLIC -Wl,--export=__stack_pointer -Wl,--stack-first)
else()
    target_sources(richc_test PRIVATE src/test_setjmp.c)
endif()

# ---- tests ----

if(RICHC_BUILD_TESTS)
    add_subdirectory(test)
endif()
