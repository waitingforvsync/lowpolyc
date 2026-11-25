# ---- core test executable ----

add_executable(richc_core_test
    main.c
    test_arena.c
    test_array.c
    test_array_headers.c
    test_bitset.c
    test_bitset_foreach.c
    test_box2f.c
    test_box2i.c
    test_bytes.c
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
    test_pool.c
    test_pool_foreach.c
    test_quatf.c
    test_random.c
    test_rational.c
    test_solve.c
    test_sort.c
    test_str.c
    test_time.c
    test_upper_bound.c
    test_vec2f.c
    test_vec2i.c
    test_vec3f.c
    test_vec3i.c
    test_vec4f.c
)

target_link_libraries(richc_core_test PRIVATE richc_test)

# ---- run tests as a post-build step ----

add_custom_command(
    TARGET richc_core_test POST_BUILD
    COMMAND $<TARGET_FILE:richc_core_test>
    WORKING_DIRECTORY $<TARGET_FILE_DIR:richc_core_test>
    COMMENT "Running richc core tests"
    VERBATIM
)

# ---- math accuracy tool ----

# Measures the transcendentals against libm, every f32 input included. It takes minutes, so it is
# built but not run: run richc_math_accuracy by hand after changing one. POSIX only, for pthreads.
if(NOT WIN32)
    find_package(Threads REQUIRED)
    add_executable(richc_math_accuracy math_accuracy.c)
    target_link_libraries(richc_math_accuracy PRIVATE richc m Threads::Threads)
endif()
