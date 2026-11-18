// bytes.h
//
// Byte containers: rc_view_bytes, rc_span_bytes and rc_array_bytes, the array template on uint8_t,
// plus bridges to and from strings.

#ifndef RC_BYTES_H_
#define RC_BYTES_H_

#include "richc/mstr.h"

#define RC_ARRAY_TYPE uint8_t
#define RC_ARRAY_NAME bytes
#include "richc/array.template.h"

// The bytes as a read-only string, with no copy. They need no terminator, since an rc_str carries
// its length.
static inline rc_str rc_view_bytes_as_str(rc_view_bytes bytes)
{
    return rc_str_from_ptr((const char *)bytes.data, bytes.num);
}

// The same for a span.
static inline rc_str rc_span_bytes_as_str(rc_span_bytes bytes)
{
    return rc_view_bytes_as_str(bytes.view);
}

// Move bytes whose last is a terminator into a string, resetting *bytes to {0}.
static inline rc_mstr rc_array_bytes_take_terminated_(rc_array_bytes *bytes)
{
    rc_mstr s = {
        .data = (char *)bytes->data,
        .len = bytes->num - 1,
        .cap = bytes->cap
    };

    *bytes = (rc_array_bytes) {0};
    return s;
}

// Move the bytes into a string: we append a terminator (growing only if the buffer is full) and
// reset *bytes to {0}, so the string is left as the buffer's only writer.
static inline rc_mstr rc_array_bytes_to_mstr(rc_array_bytes *bytes, rc_arena *arena)
{
    rc_array_bytes_push(bytes, 0, arena);
    return rc_array_bytes_take_terminated_(bytes);
}

#endif // RC_BYTES_H_
