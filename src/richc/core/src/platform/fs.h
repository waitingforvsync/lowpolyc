// platform/fs.h
//
// The platform's view of files: open one to read, write one whole, delete one, and ask whether one
// exists. Private to core, and implemented once per target in platform/<target>/fs.c, chosen by the
// build. file.c builds richc/file.h on top.
//
// Paths are UTF-8 with no terminator, and hold no null byte: file.c turns those away. A path too
// long for the platform is RC_FILE_ERROR_BAD_NAME. Errors are rc_file_error values.

#ifndef RC_PLATFORM_FS_H_
#define RC_PLATFORM_FS_H_

#include "richc/file.h"

// A file opened for reading: its handle and size, or the error.
typedef struct rc_fs_open_result {
    uintptr_t handle;
    uint64_t size;
    uint8_t error;  // rc_file_error
} rc_fs_open_result;

// Open a file to read. Only a successful open needs closing.
rc_fs_open_result rc_fs_open(rc_str path);

// Read exactly size bytes from where the last read ended.
// A file ending sooner is RC_FILE_ERROR_IO.
rc_file_error rc_fs_read(uintptr_t handle, void *dst, uint32_t size);

// Close a file opened by rc_fs_open.
void rc_fs_close(uintptr_t handle);

// Create or truncate a file, and write size bytes to it.
rc_file_error rc_fs_write(rc_str path, const void *data, uint32_t size);

// Delete a file.
rc_file_error rc_fs_delete(rc_str path);

// True if path names a file, not a directory.
bool rc_fs_exists(rc_str path);

#endif // RC_PLATFORM_FS_H_
