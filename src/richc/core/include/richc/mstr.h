// mstr.h
//
// A growable, arena-backed string that keeps a null terminator just past its text, so it is always
// ready to hand to anything wanting a C string.
//
// The zero value {0} is the invalid "no string", and the appends and reserve accept it, allocating
// on first use. Once valid, cap is the real size of the allocation in bytes, and it always has room
// for the text plus its terminator, so cap >= len + 1.
//
// data and len overlay an rc_str, so s.view is a read-only view of the current text at no cost, and
// rc_str_as_cstr(s.view, ...) never copies. Growth is geometric, like rc_array.

#ifndef RC_MSTR_H_
#define RC_MSTR_H_

#include "richc/str.h"

// We only ever take an arena by pointer, so a forward declaration will do.
typedef struct rc_arena rc_arena;

typedef struct rc_mstr {
    union {
        struct {
            char *data;
            uint32_t len;
        };
        rc_str view;
    };
    uint32_t cap;
} rc_mstr;

// ---- construction ----

// An empty string with a capacity-byte buffer; invalid if capacity is 0.
rc_mstr rc_mstr_make(uint32_t capacity, rc_arena *arena);

// A copy of a null-terminated string, with at least minimum_capacity bytes of buffer.
// Invalid if s is NULL.
rc_mstr rc_mstr_from_cstr(const char *s, uint32_t minimum_capacity, rc_arena *arena);

// A copy of a view, with at least minimum_capacity bytes of buffer; invalid if s is NULL.
rc_mstr rc_mstr_from_str(rc_str s, uint32_t minimum_capacity, rc_arena *arena);

// ---- predicates ----

// True once the string has a buffer.
static inline bool rc_mstr_is_valid(const rc_mstr *s)
{
    return s->data != NULL;
}

// True when the string has no text, which includes the invalid state.
static inline bool rc_mstr_is_empty(const rc_mstr *s)
{
    return s->len == 0;
}

// ---- mutation ----

// Empty the string, keeping its buffer.
void rc_mstr_reset(rc_mstr *s);

// Make the buffer at least capacity bytes, exactly; never shrinks it.
void rc_mstr_reserve(rc_mstr *s, uint32_t capacity, rc_arena *arena);

// Append a view, which must be valid.
void rc_mstr_append(rc_mstr *s, rc_str str, rc_arena *arena);

// Append one character.
void rc_mstr_append_char(rc_mstr *s, char c, rc_arena *arena);

// Append n copies of c, for padding and rules.
void rc_mstr_append_n(rc_mstr *s, char c, uint32_t n, rc_arena *arena);

// Append a number in decimal.
void rc_mstr_append_i32(rc_mstr *s, int32_t value, rc_arena *arena);
void rc_mstr_append_i64(rc_mstr *s, int64_t value, rc_arena *arena);
void rc_mstr_append_u32(rc_mstr *s, uint32_t value, rc_arena *arena);
void rc_mstr_append_u64(rc_mstr *s, uint64_t value, rc_arena *arena);

// Append a number in upper-case hex, zero-padded to its full width (two digits for hex8, up to
// sixteen for hex64).
void rc_mstr_append_hex8(rc_mstr *s, uint8_t value, rc_arena *arena);
void rc_mstr_append_hex16(rc_mstr *s, uint16_t value, rc_arena *arena);
void rc_mstr_append_hex32(rc_mstr *s, uint32_t value, rc_arena *arena);
void rc_mstr_append_hex64(rc_mstr *s, uint64_t value, rc_arena *arena);

// Replace every non-overlapping occurrence of find, left to right, in place.
// An empty find does nothing.
void rc_mstr_replace(rc_mstr *s, rc_str find, rc_str replacement, rc_arena *arena);

// ---- teardown ----

// Free the buffer (if it is the arena's last allocation) and zero the string.
void rc_mstr_release(rc_mstr *s, rc_arena *arena);

#endif // RC_MSTR_H_
