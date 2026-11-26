# ---- core test executable ----

add_executable(richc_core_test
    main.c
    test_arena.c
    test_array.c
    test_array_headers.c
    test_atomic.c
    test_bitset.c
    test_bitset_foreach.c
    test_box2f.c
    test_box2i.c
    test_bytes.c
    test_cond.c
    test_file.c
    test_genpool.c
    test_genpool_foreach.c
    test_hash.c
    test_hash_index.c
    test_hash_index_foreach.c
    test_hash_trie.c
    test_hash_trie_foreach.c
    test_lower_bound.c
    test_mat22f.c
    test_mat23f.c
    test_mat33f.c
    test_mat34f.c
    test_mat44f.c
    test_max_element.c
    test_math.c
    test_math_arc.c
    test_math_cbrt.c
    test_math_exp_log_f32.c
    test_math_exp_log_f64.c
    test_math_trig.c
    test_min_element.c
    test_mstr.c
    test_mutex.c
    test_once.c
    test_pool.c
    test_pool_foreach.c
    test_quatf.c
    test_random.c
    test_rational.c
    test_rwlock.c
    test_semaphore.c
    test_solve.c
    test_sort.c
    test_spinlock.c
    test_str.c
    test_str_parse.c
    test_str_parse_float.c
    test_str_utf8.c
    test_thread.c
    test_time.c
    test_tls.c
    test_upper_bound.c
    test_vec2f.c
    test_vec2i.c
    test_vec3f.c
    test_vec3i.c
    test_vec4f.c
    test_zip_inflate.c
)

target_link_libraries(richc_core_test PRIVATE richc_test)

# These compare against the C library, which wasm has none of
if(NOT RICHC_WASM)
    target_sources(richc_core_test PRIVATE test_mstr_printf.c)
endif()

# Bundled fixtures are staged next to the test binary, before the tests run
add_custom_command(
    TARGET richc_core_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:richc_core_test>/data
    VERBATIM
)

# ---- run tests as a post-build step ----

richc_run_tests(richc_core_test "Running richc core tests")

# ---- accuracy tools ----

# Measures the transcendentals against libm, every f32 input included. It takes minutes, so it is
# built but not run: run richc_math_accuracy by hand after changing one. POSIX only, for pthreads.
if(NOT WIN32 AND NOT RICHC_WASM)
    find_package(Threads REQUIRED)
    add_executable(richc_math_accuracy math_accuracy.c)
    target_link_libraries(richc_math_accuracy PRIVATE richc m Threads::Threads)

    # Checks the float parser against strtod and strtof, every float included; run it by hand
    # after changing str_parse_float.c.
    add_executable(richc_parse_accuracy parse_accuracy.c)
    target_link_libraries(richc_parse_accuracy PRIVATE richc m Threads::Threads)
endif()
