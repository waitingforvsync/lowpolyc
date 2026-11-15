// file.c
//
// Whole-file loads and saves, on the platform's files (fs.h).

#include "richc/file.h"

#include "fs.h"

// True if path holds no null byte, which would cut it short for the platform.
static bool rc_file_is_name(rc_str path)
{
    for (uint32_t i = 0; i < path.len; i++) {
        if (path.data[i] == '\0') {
            return false;
        }
    }
    return true;
}


// The open file's contents, with num the file's size and room for at least one byte more.
static rc_file_load_binary_result rc_file_load(rc_str path, uint32_t minimum_capacity, rc_arena *arena)
{
    RC_ASSERT(arena);
    if (!rc_file_is_name(path)) {
        return (rc_file_load_binary_result) {.error = RC_FILE_ERROR_BAD_NAME};
    }

    rc_fs_open_result file = rc_fs_open(path);
    if (file.error != RC_FILE_OK) {
        return (rc_file_load_binary_result) {.error = file.error};
    }
    if (file.size >= UINT32_MAX) {
        rc_fs_close(file.handle);
        return (rc_file_load_binary_result) {.error = RC_FILE_ERROR_TOO_LARGE};
    }

    // Allocate the spare byte as well as the contents
    uint32_t size = (uint32_t)file.size;
    uint32_t capacity = size + 1 > minimum_capacity ? size + 1 : minimum_capacity;
    rc_array_bytes contents = rc_array_bytes_make(capacity, arena);
    rc_array_bytes_resize(&contents, size, NULL);

    rc_file_error error = size > 0 ? rc_fs_read(file.handle, contents.data, size) : RC_FILE_OK;
    rc_fs_close(file.handle);
    if (error != RC_FILE_OK) {
        rc_array_bytes_release(&contents, arena);
        return (rc_file_load_binary_result) {.error = (uint8_t)error};
    }
    return (rc_file_load_binary_result) {.contents = contents};
}


rc_file_size_result rc_file_size(rc_str path)
{
    if (!rc_file_is_name(path)) {
        return (rc_file_size_result) {.error = RC_FILE_ERROR_BAD_NAME};
    }

    rc_fs_open_result file = rc_fs_open(path);
    if (file.error != RC_FILE_OK) {
        return (rc_file_size_result) {.error = file.error};
    }

    rc_fs_close(file.handle);
    if (file.size > UINT32_MAX) {
        return (rc_file_size_result) {.error = RC_FILE_ERROR_TOO_LARGE};
    }
    return (rc_file_size_result) {.size = (uint32_t)file.size};
}


bool rc_file_exists(rc_str path)
{
    return rc_file_is_name(path) && rc_fs_exists(path);
}


rc_file_load_text_result rc_file_load_text(rc_str path, uint32_t minimum_capacity, rc_arena *arena)
{
    rc_file_load_binary_result loaded = rc_file_load(path, minimum_capacity, arena);
    if (loaded.error != RC_FILE_OK) {
        return (rc_file_load_text_result) {.error = loaded.error};
    }

    // The spare byte takes the terminator, so this never grows
    return (rc_file_load_text_result) {.text = rc_array_bytes_to_mstr(&loaded.contents, NULL)};
}


rc_file_load_binary_result rc_file_load_binary(rc_str path, uint32_t minimum_capacity, rc_arena *arena)
{
    return rc_file_load(path, minimum_capacity, arena);
}


rc_file_error rc_file_save_text(rc_str path, rc_str text)
{
    RC_ASSERT(text.len == 0 || text.data);
    if (!rc_file_is_name(path)) {
        return RC_FILE_ERROR_BAD_NAME;
    }
    return rc_fs_write(path, text.data, text.len);
}


rc_file_error rc_file_save_binary(rc_str path, rc_view_bytes data)
{
    RC_ASSERT(data.num == 0 || data.data);
    if (!rc_file_is_name(path)) {
        return RC_FILE_ERROR_BAD_NAME;
    }
    return rc_fs_write(path, data.data, data.num);
}


rc_file_error rc_file_delete(rc_str path)
{
    if (!rc_file_is_name(path)) {
        return RC_FILE_ERROR_BAD_NAME;
    }
    return rc_fs_delete(path);
}
