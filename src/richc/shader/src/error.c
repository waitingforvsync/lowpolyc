// error.c
//
// Errors, placed by line and column.

#include "error.h"

// True if a line ends at i: at a line feed, or a carriage return with no line feed after it.
static bool rc_wgsl_ends_line(rc_str source, uint32_t i)
{
    if (source.data[i] != '\r') {
        return source.data[i] == '\n';
    }

    return i + 1 >= source.len || source.data[i + 1] != '\n';
}


rc_wgsl_error rc_wgsl_error_at(rc_str source, uint32_t offset, rc_str message)
{
    uint32_t line = 1;
    uint32_t line_start = 0;

    RC_ASSERT(offset <= source.len);
    for (uint32_t i = 0; i < offset; i++) {
        if (rc_wgsl_ends_line(source, i)) {
            line++;
            line_start = i + 1;
        }
    }

    return (rc_wgsl_error) {
        .message = message,
        .offset = offset,
        .line = line,
        .column = offset - line_start + 1
    };
}
