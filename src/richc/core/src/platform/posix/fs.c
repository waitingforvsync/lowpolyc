// fs.c
//
// Linux and macOS. Files through the POSIX calls, with each path copied to a terminated buffer on
// the stack.

// O_CLOEXEC is POSIX 2008
#define _POSIX_C_SOURCE 200809L

#include "platform/fs.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "richc/mem.h"

// The longest path we take, terminator included: Linux's PATH_MAX.
enum {
    RC_FS_MAX_PATH = 4096
};

// Copy path into name with a terminator: false if it does not fit.
static bool rc_fs_terminate(rc_str path, char name[RC_FS_MAX_PATH])
{
    if (path.len >= RC_FS_MAX_PATH) {
        return false;
    }

    rc_memcpy(name, path.data, path.len);
    name[path.len] = '\0';
    return true;
}


// The rc_file_error for errno after a failed call.
static rc_file_error rc_fs_error_from_errno(void)
{
    switch (errno) {
    case ENOENT:
    case ENOTDIR:
        return RC_FILE_ERROR_NOT_FOUND;
    case EACCES:
    case EPERM:
    case EROFS:
        return RC_FILE_ERROR_ACCESS_DENIED;
    case ENAMETOOLONG:
        return RC_FILE_ERROR_BAD_NAME;
    default:
        return RC_FILE_ERROR_IO;
    }
}


// The file just opened as fd, if it opened. Anything but a regular file is closed again: a
// directory opens, but it is not a file.
static rc_fs_open_result rc_fs_open_result_of(int fd)
{
    struct stat info = {0};

    if (fd < 0) {
        return (rc_fs_open_result) {.error = (uint8_t)rc_fs_error_from_errno()};
    }

    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
        close(fd);
        return (rc_fs_open_result) {.error = RC_FILE_ERROR_NOT_FOUND};
    }

    return (rc_fs_open_result) {
        .handle = (uintptr_t)fd,
        .size = (uint64_t)info.st_size
    };
}


// Write all size bytes at data to fd. As with reads, a write may take less than offered.
static rc_file_error rc_fs_write_all(int fd, const void *data, uint32_t size)
{
    const uint8_t *at = data;

    while (size > 0) {
        ssize_t put = write(fd, at, size);

        if (put < 0 && errno == EINTR) {
            continue;
        }

        if (put <= 0) {
            return RC_FILE_ERROR_IO;
        }

        at += put;
        size -= (uint32_t)put;
    }

    return RC_FILE_ERROR_NONE;
}


// Write size bytes to the file just opened as fd, if it opened, and close it.
static rc_file_error rc_fs_write_opened(int fd, const void *data, uint32_t size)
{
    if (fd < 0) {
        return rc_fs_error_from_errno();
    }

    if (rc_fs_write_all(fd, data, size) != RC_FILE_ERROR_NONE) {
        close(fd);
        return RC_FILE_ERROR_IO;
    }

    // Some file systems only report a failed write when the file closes
    if (close(fd) != 0) {
        return RC_FILE_ERROR_IO;
    }

    return RC_FILE_ERROR_NONE;
}


rc_fs_open_result rc_fs_open(rc_str path)
{
    char name[RC_FS_MAX_PATH] = {0};

    if (!rc_fs_terminate(path, name)) {
        return (rc_fs_open_result) {.error = RC_FILE_ERROR_BAD_NAME};
    }

    return rc_fs_open_result_of(open(name, O_RDONLY | O_CLOEXEC));
}


rc_file_error rc_fs_read(uintptr_t handle, void *dst, uint32_t size)
{
    uint8_t *at = dst;

    // A read may return less than asked, or be interrupted before it starts
    while (size > 0) {
        ssize_t got = read((int)handle, at, size);

        if (got < 0 && errno == EINTR) {
            continue;
        }

        if (got <= 0) {
            return RC_FILE_ERROR_IO;
        }

        at += got;
        size -= (uint32_t)got;
    }

    return RC_FILE_ERROR_NONE;
}


void rc_fs_close(uintptr_t handle)
{
    close((int)handle);
}


rc_file_error rc_fs_write(rc_str path, const void *data, uint32_t size)
{
    char name[RC_FS_MAX_PATH] = {0};

    if (!rc_fs_terminate(path, name)) {
        return RC_FILE_ERROR_BAD_NAME;
    }

    return rc_fs_write_opened(open(name, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666), data, size);
}


rc_file_error rc_fs_delete(rc_str path)
{
    char name[RC_FS_MAX_PATH] = {0};

    if (!rc_fs_terminate(path, name)) {
        return RC_FILE_ERROR_BAD_NAME;
    }

    if (unlink(name) != 0) {
        return rc_fs_error_from_errno();
    }

    return RC_FILE_ERROR_NONE;
}


bool rc_fs_exists(rc_str path)
{
    char name[RC_FS_MAX_PATH] = {0};
    struct stat info = {0};

    if (!rc_fs_terminate(path, name)) {
        return false;
    }

    return stat(name, &info) == 0 && S_ISREG(info.st_mode);
}
