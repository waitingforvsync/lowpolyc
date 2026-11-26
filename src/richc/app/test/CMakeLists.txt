# ---- app test executable ----

# Tests for what needs no window or GPU: the platform code is checked by running it.
add_executable(richc_app_test
    main.c
    test_keys.c
)

# The tests reach richc_app's private headers
target_include_directories(richc_app_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(richc_app_test PRIVATE richc_app richc_test)

# ---- run tests as a post-build step ----

richc_run_tests(richc_app_test "Running richc app tests")
