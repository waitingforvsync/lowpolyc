// test_file.c
//
// Tests for whole-file loads and saves. They work on files named richc_test_file_* in the current
// directory, deleting each one when done.

#include "richc/file.h"

#include "richc/test.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(file) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(file, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(file, fix)
{
    rc_arena_release(&fix->a);
}

// ---- saving and loading ----

RC_TEST_STEP(file, text_round_trip, fix)
{
    // Binary I/O: the CR LF comes back as it went
    rc_str path = RC_STR("richc_test_file_text.txt");
    rc_str text = RC_STR("hello\nworld\r\n");
    rc_file_error saved = rc_file_save_text(path, text);
    rc_file_load_text_result loaded = rc_file_load_text(path, 0, &fix->a);

    RC_CHECK(saved, ==, RC_FILE_ERROR_NONE);
    RC_CHECK(loaded.error, ==, RC_FILE_ERROR_NONE);
    RC_CHECK(loaded.text.view, ==, text);
    RC_CHECK_TRUE(loaded.text.data[loaded.text.len] == '\0');
    RC_CHECK(loaded.text.cap, >=, loaded.text.len + 1);
    RC_CHECK(rc_file_delete(path), ==, RC_FILE_ERROR_NONE);
}


RC_TEST_STEP(file, binary_round_trip_keeps_zero_bytes, fix)
{
    static uint8_t data[] = {0, 1, 0, 255, 0};
    rc_view_bytes bytes = RC_VIEW(data);
    rc_str path = RC_STR("richc_test_file_binary.bin");
    rc_file_error saved = rc_file_save_binary(path, bytes);
    rc_file_load_binary_result loaded = rc_file_load_binary(path, 0, &fix->a);

    RC_CHECK(saved, ==, RC_FILE_ERROR_NONE);
    RC_CHECK(loaded.error, ==, RC_FILE_ERROR_NONE);
    RC_CHECK(loaded.contents.num, ==, 5u);
    RC_CHECK(rc_memcmp(loaded.contents.data, data, 5), ==, 0);
    RC_CHECK(rc_file_delete(path), ==, RC_FILE_ERROR_NONE);
}


// size bytes of a fixed pseudo-random sequence.
static rc_array_bytes noise(uint32_t size, rc_arena *arena)
{
    rc_array_bytes data = rc_array_bytes_make(size, arena);
    uint32_t state = 1;

    for (uint32_t i = 0; i < size; i++) {
        state = state * 1664525u + 1013904223u;
        rc_array_bytes_push(&data, (uint8_t)(state >> 24), NULL);
    }
    return data;
}


RC_TEST_STEP(file, a_large_file_round_trips, fix)
{
    // A megabyte, enough that the OS may hand reads and writes back in pieces
    uint32_t size = 1u << 20;
    rc_array_bytes data = noise(size, &fix->a);
    rc_str path = RC_STR("richc_test_file_large.bin");
    rc_file_error saved = rc_file_save_binary(path, data.view);
    rc_file_load_binary_result loaded = rc_file_load_binary(path, 0, &fix->a);

    RC_CHECK(saved, ==, RC_FILE_ERROR_NONE);
    RC_CHECK(loaded.error, ==, RC_FILE_ERROR_NONE);
    RC_CHECK(loaded.contents.num, ==, size);
    RC_CHECK(rc_memcmp(loaded.contents.data, data.data, size), ==, 0);
    RC_CHECK(rc_file_size(path).size, ==, size);
    RC_CHECK(rc_file_delete(path), ==, RC_FILE_ERROR_NONE);
}


RC_TEST_STEP(file, empty_file, fix)
{
    rc_str path = RC_STR("richc_test_file_empty.txt");
    rc_file_error saved = rc_file_save_text(path, RC_STR(""));
    rc_file_size_result size = rc_file_size(path);
    rc_file_load_text_result text = rc_file_load_text(path, 0, &fix->a);
    rc_file_load_binary_result binary = rc_file_load_binary(path, 0, &fix->a);

    RC_CHECK(saved, ==, RC_FILE_ERROR_NONE);
    RC_CHECK(size.size, ==, 0u);

    // Still a valid, terminated string, and valid bytes
    RC_CHECK(text.error, ==, RC_FILE_ERROR_NONE);
    RC_CHECK_TRUE(rc_mstr_is_valid(&text.text));
    RC_CHECK(text.text.len, ==, 0u);
    RC_CHECK_TRUE(text.text.data[0] == '\0');
    RC_CHECK(binary.error, ==, RC_FILE_ERROR_NONE);
    RC_CHECK_TRUE(rc_array_bytes_is_valid(&binary.contents));
    RC_CHECK(binary.contents.num, ==, 0u);
    RC_CHECK(rc_file_delete(path), ==, RC_FILE_ERROR_NONE);
}


RC_TEST_STEP(file, capacity, fix)
{
    // One spare byte by default, or the minimum asked for
    rc_str path = RC_STR("richc_test_file_capacity.bin");

    RC_CHECK(rc_file_save_text(path, RC_STR("0123456789")), ==, RC_FILE_ERROR_NONE);
    RC_CHECK(rc_file_load_binary(path, 0, &fix->a).contents.cap, ==, 11u);
    RC_CHECK(rc_file_load_binary(path, 1000, &fix->a).contents.cap, ==, 1000u);
    RC_CHECK(rc_file_load_text(path, 0, &fix->a).text.cap, ==, 11u);
    RC_CHECK(rc_file_load_text(path, 64, &fix->a).text.cap, ==, 64u);
    RC_CHECK(rc_file_delete(path), ==, RC_FILE_ERROR_NONE);
}


RC_TEST_STEP(file, saving_truncates, fix)
{
    rc_str path = RC_STR("richc_test_file_truncate.txt");

    RC_CHECK(rc_file_save_text(path, RC_STR("a much longer first version")), ==, RC_FILE_ERROR_NONE);
    RC_CHECK(rc_file_save_text(path, RC_STR("short")), ==, RC_FILE_ERROR_NONE);
    RC_CHECK(rc_file_load_text(path, 0, &fix->a).text.view, ==, RC_STR("short"));
    RC_CHECK(rc_file_delete(path), ==, RC_FILE_ERROR_NONE);
}


RC_TEST_STEP(file, paths_need_no_terminator, fix)
{
    // The path is the front of a longer string, so the byte after it is not a null
    rc_str longer = RC_STR("richc_test_file_unterminated.txtXYZ");
    rc_str path = rc_str_drop_last(longer, 3);

    RC_CHECK(rc_file_save_text(path, RC_STR("abc")), ==, RC_FILE_ERROR_NONE);
    RC_CHECK_TRUE(rc_file_exists(RC_STR("richc_test_file_unterminated.txt")));
    RC_CHECK_FALSE(rc_file_exists(longer));
    RC_CHECK(rc_file_load_text(path, 0, &fix->a).text.view, ==, RC_STR("abc"));
    RC_CHECK(rc_file_delete(path), ==, RC_FILE_ERROR_NONE);
}

// ---- measuring, existence and deleting ----

RC_TEST(file, size_and_exists)
{
    rc_str path = RC_STR("richc_test_file_size.txt");
    rc_file_error saved = rc_file_save_text(path, RC_STR("12345"));
    rc_file_size_result size = rc_file_size(path);

    RC_CHECK(saved, ==, RC_FILE_ERROR_NONE);
    RC_CHECK(size.error, ==, RC_FILE_ERROR_NONE);
    RC_CHECK(size.size, ==, 5u);
    RC_CHECK_TRUE(rc_file_exists(path));
    RC_CHECK(rc_file_delete(path), ==, RC_FILE_ERROR_NONE);
    RC_CHECK_FALSE(rc_file_exists(path));
}


RC_TEST_STEP(file, missing_files, fix)
{
    rc_str path = RC_STR("richc_test_file_missing.txt");
    rc_file_load_text_result text = rc_file_load_text(path, 0, &fix->a);
    rc_file_load_binary_result binary = rc_file_load_binary(path, 0, &fix->a);

    RC_CHECK(text.error, ==, RC_FILE_ERROR_NOT_FOUND);
    RC_CHECK_FALSE(rc_mstr_is_valid(&text.text));
    RC_CHECK(binary.error, ==, RC_FILE_ERROR_NOT_FOUND);
    RC_CHECK_FALSE(rc_array_bytes_is_valid(&binary.contents));

    RC_CHECK(rc_file_size(path).error, ==, RC_FILE_ERROR_NOT_FOUND);
    RC_CHECK_FALSE(rc_file_exists(path));
    RC_CHECK(rc_file_delete(path), ==, RC_FILE_ERROR_NOT_FOUND);
}


RC_TEST_STEP(file, missing_directories, fix)
{
    rc_str path = RC_STR("richc_test_file_no_such_dir/file.txt");

    RC_CHECK(rc_file_load_text(path, 0, &fix->a).error, ==, RC_FILE_ERROR_NOT_FOUND);
    RC_CHECK(rc_file_save_text(path, RC_STR("x")), ==, RC_FILE_ERROR_NOT_FOUND);
}


RC_TEST_STEP(file, directories_are_not_files, fix)
{
    rc_str path = RC_STR(".");

    RC_CHECK_FALSE(rc_file_exists(path));
    RC_CHECK(rc_file_size(path).error, ==, RC_FILE_ERROR_NOT_FOUND);
    RC_CHECK(rc_file_load_text(path, 0, &fix->a).error, ==, RC_FILE_ERROR_NOT_FOUND);
}

// ---- bad names ----

RC_TEST_STEP(file, a_null_byte_is_a_bad_name, fix)
{
    // The OS would see only "richc_test_file_null", so we refuse rather than touch that file
    static const char name[] = "richc_test_file_null\0.txt";
    rc_str path = rc_str_from_ptr(name, sizeof(name) - 1);

    RC_CHECK(rc_file_save_text(path, RC_STR("x")), ==, RC_FILE_ERROR_BAD_NAME);
    RC_CHECK(rc_file_save_binary(path, (rc_view_bytes) {0}), ==, RC_FILE_ERROR_BAD_NAME);
    RC_CHECK(rc_file_load_text(path, 0, &fix->a).error, ==, RC_FILE_ERROR_BAD_NAME);
    RC_CHECK(rc_file_load_binary(path, 0, &fix->a).error, ==, RC_FILE_ERROR_BAD_NAME);
    RC_CHECK(rc_file_size(path).error, ==, RC_FILE_ERROR_BAD_NAME);
    RC_CHECK(rc_file_delete(path), ==, RC_FILE_ERROR_BAD_NAME);
    RC_CHECK_FALSE(rc_file_exists(path));
    RC_CHECK_FALSE(rc_file_exists(RC_STR("richc_test_file_null")));
}


RC_TEST_STEP(file, an_overlong_path_is_a_bad_name, fix)
{
    // Longer than any platform takes, rather than quietly cut short
    rc_mstr long_path = rc_mstr_make(8192, &fix->a);

    for (uint32_t i = 0; i < 8000; i++) {
        rc_mstr_append_char(&long_path, 'a', &fix->a);
    }
    RC_CHECK(rc_file_save_text(long_path.view, RC_STR("x")), ==, RC_FILE_ERROR_BAD_NAME);
    RC_CHECK(rc_file_load_text(long_path.view, 0, &fix->a).error, ==, RC_FILE_ERROR_BAD_NAME);
    RC_CHECK_FALSE(rc_file_exists(long_path.view));
}
