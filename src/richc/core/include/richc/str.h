// str.h
//
// A non-owning string view: a pointer and a length.
// It never allocates, and the text need not be null-terminated.
//
// A view has two distinct empty states. {NULL, 0} is invalid, our "no string" or "not found"
// sentinel; {ptr, 0} is a valid string that happens to be empty. Both count as empty.
//
// Slicing never fails: out-of-range counts and indices clamp to the string. Searching returns
// RC_INDEX_NONE when there is no match, and an empty needle is found everywhere, so find_first
// gives 0 and find_last gives the length.

#ifndef RC_STR_H_
#define RC_STR_H_

#include "richc/macros.h"

typedef struct rc_str {
    const char *data;
    uint32_t len;
} rc_str;

// The two halves of a split, without the delimiter.
typedef struct rc_str_pair {
    rc_str first;
    rc_str second;
} rc_str_pair;

// ---- construction ----

// A view of a string literal or char array, built at compile time. Never pass a char *: sizeof
// would give the pointer's size, not the string's.
#define RC_STR(s) ((rc_str) RC_STR_INIT(s))

// RC_STR as a bare brace list, for static initialisers: MSVC rejects the compound literal there
// because it is not a constant expression.
#define RC_STR_INIT(s) {.data = (s), .len = (uint32_t)(sizeof(s) - 1)}

// A view of len bytes at data.
static inline rc_str rc_str_make(const char *data, uint32_t len)
{
    return (rc_str) {.data = data, .len = len};
}

// A view of a null-terminated string; invalid if s is NULL.
rc_str rc_str_from_cstr(const char *s);

// ---- predicates ----

// True unless s is the invalid {NULL, 0}.
static inline bool rc_str_is_valid(rc_str s)
{
    return s.data != NULL;
}

// True when s has no characters, which includes the invalid state.
static inline bool rc_str_is_empty(rc_str s)
{
    return s.len == 0;
}

// True when a and b hold the same bytes. Both must be valid.
bool rc_str_is_equal(rc_str a, rc_str b);

// Like rc_str_is_equal, ignoring ASCII case.
bool rc_str_is_equal_insensitive(rc_str a, rc_str b);

// Byte-wise ordering: negative, zero or positive. A prefix sorts first.
int32_t rc_str_compare(rc_str a, rc_str b);

// Like rc_str_compare, ignoring ASCII case.
int32_t rc_str_compare_insensitive(rc_str a, rc_str b);

// ---- slicing ----

// The first count characters.
static inline rc_str rc_str_left(rc_str s, uint32_t count)
{
    if (count > s.len) {
        count = s.len;
    }
    return rc_str_make(s.data, count);
}

// The last count characters.
static inline rc_str rc_str_right(rc_str s, uint32_t count)
{
    if (count > s.len) {
        count = s.len;
    }
    return rc_str_make(s.data + (s.len - count), count);
}

// count characters from start.
static inline rc_str rc_str_substr(rc_str s, uint32_t start, uint32_t count)
{
    if (start > s.len) {
        start = s.len;
    }
    if (count > s.len - start) {
        count = s.len - start;
    }
    return rc_str_make(s.data + start, count);
}

// Everything from start onwards.
static inline rc_str rc_str_skip(rc_str s, uint32_t start)
{
    if (start > s.len) {
        start = s.len;
    }
    return rc_str_make(s.data + start, s.len - start);
}

// ---- searching ----

// True when s begins with prefix.
bool rc_str_starts_with(rc_str s, rc_str prefix);

// True when s ends with suffix.
bool rc_str_ends_with(rc_str s, rc_str suffix);

// Index of the first occurrence of needle, or RC_INDEX_NONE.
uint32_t rc_str_find_first(rc_str haystack, rc_str needle);

// Index of the last occurrence of needle, or RC_INDEX_NONE.
uint32_t rc_str_find_last(rc_str haystack, rc_str needle);

// True when needle occurs anywhere in haystack.
bool rc_str_contains(rc_str haystack, rc_str needle);

// ---- trimming ----

// s without prefix, or s unchanged if it does not start with it.
rc_str rc_str_remove_prefix(rc_str s, rc_str prefix);

// s without suffix, or s unchanged if it does not end with it.
rc_str rc_str_remove_suffix(rc_str s, rc_str suffix);

// ---- splitting ----

// Split around the first split_by. With no match, first is all of s and second is invalid.
rc_str_pair rc_str_first_split(rc_str s, rc_str split_by);

// Split around the last split_by. With no match, first is all of s and second is invalid.
rc_str_pair rc_str_last_split(rc_str s, rc_str split_by);

// ---- conversion ----

// s as a C string. If a null already follows the text, we hand back s.data itself; otherwise we
// copy what fits into buf, terminate it and return buf (NULL if there is no buf). Reads the byte
// after the text.
const char *rc_str_as_cstr(rc_str s, char *buf, uint32_t buf_size);

#endif // RC_STR_H_
