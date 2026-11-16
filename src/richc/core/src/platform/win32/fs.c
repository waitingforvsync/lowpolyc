// fs.c
//
// Windows. Files through the wide-character Win32 calls, with each UTF-8 path converted to UTF-16
// on the stack. Paths past MAX_PATH (260 characters) need the program to be marked long-path aware
// in its manifest.

#include "fs.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// The longest path we take in UTF-16 units, terminator included.
enum {
    RC_FS_MAX_PATH = 4096
};

// Convert path to UTF-16 in name, with a terminator: false if it does not fit or is not UTF-8.
static bool rc_fs_widen(rc_str path, wchar_t name[RC_FS_MAX_PATH])
{
    int len = 0;

    // MultiByteToWideChar refuses an empty string. UTF-16 never takes more units than UTF-8 takes
    // bytes, so a short enough path always fits.
    if (path.len > 0) {
        if (path.len >= RC_FS_MAX_PATH) {
            return false;
        }
        len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data, (int)path.len, name, RC_FS_MAX_PATH - 1);
        if (len == 0) {
            return false;
        }
    }
    name[len] = L'\0';
    return true;
}


// The rc_file_error for GetLastError after a failed call.
static rc_file_error rc_fs_error_from_last(void)
{
    switch (GetLastError()) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
        return RC_FILE_ERROR_NOT_FOUND;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_WRITE_PROTECT:
        return RC_FILE_ERROR_ACCESS_DENIED;
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_INVALID_NAME:
        return RC_FILE_ERROR_BAD_NAME;
    default:
        return RC_FILE_ERROR_IO;
    }
}


// True if name is a directory.
static bool rc_fs_is_directory(const wchar_t *name)
{
    DWORD attributes = GetFileAttributesW(name);

    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}


rc_fs_open_result rc_fs_open(rc_str path)
{
    wchar_t name[RC_FS_MAX_PATH];

    if (!rc_fs_widen(path, name)) {
        return (rc_fs_open_result) {.error = RC_FILE_ERROR_BAD_NAME};
    }

    // Opening a directory is refused as access denied; we call it not found, as POSIX does
    HANDLE file = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        rc_file_error error = rc_fs_error_from_last();

        if (error == RC_FILE_ERROR_ACCESS_DENIED && rc_fs_is_directory(name)) {
            error = RC_FILE_ERROR_NOT_FOUND;
        }
        return (rc_fs_open_result) {.error = (uint8_t)error};
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size)) {
        CloseHandle(file);
        return (rc_fs_open_result) {.error = RC_FILE_ERROR_IO};
    }
    return (rc_fs_open_result) {
        .handle = (uintptr_t)file,
        .size = (uint64_t)size.QuadPart
    };
}


rc_file_error rc_fs_read(uintptr_t handle, void *dst, uint32_t size)
{
    uint8_t *at = dst;

    // A read may return less than asked
    while (size > 0) {
        DWORD got = 0;

        if (!ReadFile((HANDLE)handle, at, size, &got, NULL) || got == 0) {
            return RC_FILE_ERROR_IO;
        }
        at += got;
        size -= got;
    }
    return RC_FILE_ERROR_NONE;
}


void rc_fs_close(uintptr_t handle)
{
    CloseHandle((HANDLE)handle);
}


rc_file_error rc_fs_write(rc_str path, const void *data, uint32_t size)
{
    wchar_t name[RC_FS_MAX_PATH];

    if (!rc_fs_widen(path, name)) {
        return RC_FILE_ERROR_BAD_NAME;
    }

    HANDLE file = CreateFileW(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return rc_fs_error_from_last();
    }

    // As with reads, a write may take less than offered
    const uint8_t *at = data;
    while (size > 0) {
        DWORD put = 0;

        if (!WriteFile(file, at, size, &put, NULL) || put == 0) {
            CloseHandle(file);
            return RC_FILE_ERROR_IO;
        }
        at += put;
        size -= put;
    }

    if (!CloseHandle(file)) {
        return RC_FILE_ERROR_IO;
    }
    return RC_FILE_ERROR_NONE;
}


rc_file_error rc_fs_delete(rc_str path)
{
    wchar_t name[RC_FS_MAX_PATH];

    if (!rc_fs_widen(path, name)) {
        return RC_FILE_ERROR_BAD_NAME;
    }
    if (!DeleteFileW(name)) {
        return rc_fs_error_from_last();
    }
    return RC_FILE_ERROR_NONE;
}


bool rc_fs_exists(rc_str path)
{
    wchar_t name[RC_FS_MAX_PATH];

    if (!rc_fs_widen(path, name)) {
        return false;
    }

    DWORD attributes = GetFileAttributesW(name);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
