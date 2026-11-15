// file.h
//
// Whole files: measure, load, save and delete them.
//
// Paths are rc_str in UTF-8 and need no terminator. All I/O is binary, with no line-ending
// translation. Loads allocate from the arena they are given.
//
// Every function reports an rc_file_error, where RC_FILE_OK (0) is success. A failed load returns
// the invalid {0} contents alongside its error. A path too long for the platform, or holding a
// null byte, is RC_FILE_ERROR_BAD_NAME, never quietly cut short.
//
// A load returns a growable result, since loaded data is often changed after reading (parsed in
// place, appended to). The arena owns it, so its lifetime is the arena's; take .view for read-only
// use. minimum_capacity is a floor on its capacity, to leave room for growing. There is always a
// spare byte past the contents, and a text load puts the mstr's terminator in it.
//
// Saving creates the file, or truncates one already there.
//
// On wasm there is no file system for us to reach without waiting, so the page keeps one: the
// richc/fs.js script holds files by name in memory. The page puts files there (dropped, picked or
// fetched) before the module asks for them, and takes saved files from there.

#ifndef RC_FILE_H_
#define RC_FILE_H_

#include "richc/bytes.h"

typedef enum rc_file_error {
    RC_FILE_OK = 0,
    RC_FILE_ERROR_NOT_FOUND,
    RC_FILE_ERROR_ACCESS_DENIED,
    RC_FILE_ERROR_TOO_LARGE,
    RC_FILE_ERROR_IO,
    RC_FILE_ERROR_BAD_NAME
} rc_file_error;

typedef struct rc_file_size_result {
    uint32_t size;
    uint8_t error;  // rc_file_error
} rc_file_size_result;

typedef struct rc_file_load_text_result {
    rc_mstr text;
    uint8_t error;  // rc_file_error
} rc_file_load_text_result;

typedef struct rc_file_load_binary_result {
    rc_array_bytes contents;
    uint8_t error;  // rc_file_error
} rc_file_load_binary_result;

// ---- measuring ----

// The file's size in bytes, without reading it.
// Files of 4 GiB or more are RC_FILE_ERROR_TOO_LARGE.
rc_file_size_result rc_file_size(rc_str path);

// True if path names a file (not a directory). Needs no permission to read it.
bool rc_file_exists(rc_str path);

// ---- loading ----

// The file's contents as a string, terminated just past the text.
rc_file_load_text_result rc_file_load_text(rc_str path, uint32_t minimum_capacity, rc_arena *arena);

// The file's contents as bytes.
rc_file_load_binary_result rc_file_load_binary(rc_str path, uint32_t minimum_capacity, rc_arena *arena);

// ---- saving and deleting ----

// Write text to the file, creating or truncating it.
rc_file_error rc_file_save_text(rc_str path, rc_str text);

// Write data to the file, creating or truncating it.
rc_file_error rc_file_save_binary(rc_str path, rc_view_bytes data);

// Delete the file. RC_FILE_ERROR_NOT_FOUND if there is none.
rc_file_error rc_file_delete(rc_str path);

#endif // RC_FILE_H_
