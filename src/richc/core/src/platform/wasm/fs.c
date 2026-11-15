// fs.c
//
// wasm32. The page keeps the files, in memory, by name: fs.js beside this supplies the imports and
// the store. The page fills it before the module reads and takes saved files from it, so no call
// here ever waits.

#include "fs.h"

// The handle of an opened file (above 0), or minus its rc_file_error.
__attribute__((import_module("richc"), import_name("fs_open")))
int32_t rc_wasm_fs_open_(const char *path, uint32_t len);

// An opened file's size in bytes.
__attribute__((import_module("richc"), import_name("fs_size")))
double rc_wasm_fs_size_(int32_t handle);

// Copy the next size bytes of an opened file to dst, returning an rc_file_error.
__attribute__((import_module("richc"), import_name("fs_read")))
int32_t rc_wasm_fs_read_(int32_t handle, void *dst, uint32_t size);

// Forget an opened file.
__attribute__((import_module("richc"), import_name("fs_close")))
void rc_wasm_fs_close_(int32_t handle);

// Store a copy of the bytes under path, returning an rc_file_error.
__attribute__((import_module("richc"), import_name("fs_write")))
int32_t rc_wasm_fs_write_(const char *path, uint32_t len, const void *data, uint32_t size);

// Forget the file under path, returning an rc_file_error.
__attribute__((import_module("richc"), import_name("fs_delete")))
int32_t rc_wasm_fs_delete_(const char *path, uint32_t len);

// 1 if a file is stored under path, otherwise 0.
__attribute__((import_module("richc"), import_name("fs_exists")))
int32_t rc_wasm_fs_exists_(const char *path, uint32_t len);

rc_fs_open_result rc_fs_open(rc_str path)
{
    int32_t handle = rc_wasm_fs_open_(path.data, path.len);

    if (handle < 0) {
        return (rc_fs_open_result) {.error = (uint8_t)-handle};
    }

    // A whole number of bytes below 2^53, so the conversion is exact
    return (rc_fs_open_result) {
        .handle = (uintptr_t)handle,
        .size = (uint64_t)rc_wasm_fs_size_(handle)
    };
}


rc_file_error rc_fs_read(uintptr_t handle, void *dst, uint32_t size)
{
    return (rc_file_error)rc_wasm_fs_read_((int32_t)handle, dst, size);
}


void rc_fs_close(uintptr_t handle)
{
    rc_wasm_fs_close_((int32_t)handle);
}


rc_file_error rc_fs_write(rc_str path, const void *data, uint32_t size)
{
    return (rc_file_error)rc_wasm_fs_write_(path.data, path.len, data, size);
}


rc_file_error rc_fs_delete(rc_str path)
{
    return (rc_file_error)rc_wasm_fs_delete_(path.data, path.len);
}


bool rc_fs_exists(rc_str path)
{
    return rc_wasm_fs_exists_(path.data, path.len) != 0;
}
