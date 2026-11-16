// error.c
//
// Errors, placed by line and column.

#include "error.h"

rc_wgsl_error rc_wgsl_error_at(rc_str source, uint32_t offset, rc_str message)
{
    uint32_t line = 1;
    uint32_t line_start = 0;

    // A line ends at a line feed, or a carriage return with no line feed after it
    RC_ASSERT(offset <= source.len);
    for (uint32_t i = 0; i < offset; i++) {
        char c = source.data[i];
        bool crlf = c == '\r' && i + 1 < source.len && source.data[i + 1] == '\n';

        if ((c == '\n' || c == '\r') && !crlf) {
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
