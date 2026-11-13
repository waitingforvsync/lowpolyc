# ---- core test executable ----

add_executable(richc_core_test
    main.c
    test_arena.c
    test_array.c
    test_bitset.c
    test_genpool.c
    test_hash.c
    test_hash_trie.c
    test_hash_trie_foreach.c
    test_mstr.c
    test_ops.c
    test_pool.c
    test_str.c
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
