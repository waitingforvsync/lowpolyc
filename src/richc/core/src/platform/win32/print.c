// print.c
//
// Windows. A console takes UTF-16 through WriteConsoleW, so text bound for one is widened a piece
// at a time. Redirected to a file or a pipe, the stream gets the UTF-8 bytes as they are.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "richc/print.h"

// The most UTF-16 we widen at once. A piece never splits a character, so it holds at most this
// many UTF-8 bytes.
enum {
    RC_PRINT_PIECE = 1024
};

// Write text to the console behind handle.
static void rc_print_to_console(HANDLE handle, rc_str text)
{
    wchar_t wide[RC_PRINT_PIECE] = {0};

    while (text.len > 0) {
        // The piece ends where the character the cut falls in starts, which is never at 0, as a
        // character is at most four bytes
        uint32_t len = rc_str_utf8_floor(text, RC_PRINT_PIECE);
        int num = MultiByteToWideChar(CP_UTF8, 0, text.data, (int)len, wide, RC_PRINT_PIECE);
        DWORD written = 0;

        if (num > 0) {
            WriteConsoleW(handle, wide, (DWORD)num, &written, NULL);
        }

        text = rc_str_drop_first(text, len);
    }
}


// Write text to the stream behind handle, whatever it is.
static void rc_print_to(DWORD which, rc_str text)
{
    HANDLE handle = GetStdHandle(which);
    DWORD mode = 0;

    if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
        return;
    }

    if (GetConsoleMode(handle, &mode)) {
        rc_print_to_console(handle, text);
        return;
    }

    while (text.len > 0) {
        DWORD written = 0;

        if (!WriteFile(handle, text.data, text.len, &written, NULL)) {
            return;
        }

        text = rc_str_drop_first(text, written);
    }
}


void rc_print(rc_str text)
{
    rc_print_to(STD_OUTPUT_HANDLE, text);
}


void rc_print_err(rc_str text)
{
    rc_print_to(STD_ERROR_HANDLE, text);
}
