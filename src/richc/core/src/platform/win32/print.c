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

// How many leading bytes of text, at most max, end on a whole UTF-8 character.
static uint32_t rc_print_whole_characters(rc_str text, uint32_t max)
{
    uint32_t len = text.len < max ? text.len : max;

    // Back up over continuation bytes (10xxxxxx) to the start of the character the cut falls in
    if (len < text.len) {
        while (len > 0 && ((uint8_t)text.data[len] & 0xc0u) == 0x80u) {
            len--;
        }
    }

    return len;
}


// Write text to the console behind handle.
static void rc_print_to_console(HANDLE handle, rc_str text)
{
    wchar_t wide[RC_PRINT_PIECE] = {0};

    while (text.len > 0) {
        // A malformed character would leave nothing whole to write, so take it alone
        uint32_t whole = rc_print_whole_characters(text, RC_PRINT_PIECE);
        uint32_t len = whole > 0 ? whole : 1;
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
