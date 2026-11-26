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

// A view of len bytes at data, which it does not own.
static inline rc_str rc_str_from_ptr(const char *data, uint32_t len)
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

// The first n characters.
static inline rc_str rc_str_first(rc_str s, uint32_t n)
{
    n = n > s.len ? s.len : n;
    return rc_str_from_ptr(s.data, n);
}

// The last n characters.
static inline rc_str rc_str_last(rc_str s, uint32_t n)
{
    n = n > s.len ? s.len : n;
    return rc_str_from_ptr(s.data + (s.len - n), n);
}

// All but the first n characters.
static inline rc_str rc_str_drop_first(rc_str s, uint32_t n)
{
    n = n > s.len ? s.len : n;
    return rc_str_from_ptr(s.data + n, s.len - n);
}

// All but the last n characters.
static inline rc_str rc_str_drop_last(rc_str s, uint32_t n)
{
    n = n > s.len ? s.len : n;
    return rc_str_from_ptr(s.data, s.len - n);
}

// The characters from start up to end, clamped to the string (an end before start gives nothing).
static inline rc_str rc_str_substr(rc_str s, uint32_t start, uint32_t end)
{
    start = start > s.len ? s.len : start;
    end = end > s.len ? s.len : end;
    end = end < start ? start : end;
    return rc_str_from_ptr(s.data + start, end - start);
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

// ---- UTF-8 ----
//
// The text is UTF-8, walked a codepoint at a time by a cursor: a byte index, from 0 to the length.
// Stepping either way gives the codepoint stepped over and where the cursor lands. Stepping
// forward reads only the text after the cursor, and stepping back only the text before it, each as
// if the string ended there, so a cursor in the middle of a character is never mended: the bytes
// either side of it are just malformed. rc_str_utf8_floor and _ceil move a cursor to a character
// boundary, for one that comes from elsewhere, such as a search.
//
// Malformed text never traps. Each maximal subpart of an ill-formed sequence (the longest start of
// a well-formed one, or else a single byte) is one step, giving RC_CODEPOINT_NONE, as the Unicode
// Standard recommends, so any text decodes to what browsers and Python give, once each
// RC_CODEPOINT_NONE is U+FFFD. Overlong forms, surrogates and anything past U+10FFFF are malformed.

// Where there is no codepoint: malformed text, or the end of the string.
#define RC_CODEPOINT_NONE 0xFFFFFFFFu

// U+FFFD, the codepoint that stands in for malformed text when it is shown.
#define RC_CODEPOINT_REPLACEMENT 0xFFFDu

// One step of a cursor through UTF-8.
typedef struct rc_str_utf8_char {
    uint32_t codepoint;  // the codepoint stepped over, or RC_CODEPOINT_NONE
    uint32_t cursor;     // where the cursor lands
} rc_str_utf8_char;

// Step forward from cursor: the codepoint that starts there, and the index after it. At or past
// the end: {RC_CODEPOINT_NONE, s.len}. Anywhere else, the cursor moves on by at least one byte.
rc_str_utf8_char rc_str_utf8_next(rc_str s, uint32_t cursor);

// Step back from cursor (clamped to the string): the codepoint that ends there, and the index it
// starts at. At the start: {RC_CODEPOINT_NONE, 0}. Anywhere else, the cursor moves back by at
// least one byte.
rc_str_utf8_char rc_str_utf8_prev(rc_str s, uint32_t cursor);

// The character boundary at or before index: the start of the character index falls in, or the
// length if it is at or past the end.
uint32_t rc_str_utf8_floor(rc_str s, uint32_t index);

// The character boundary at or after index: the end of the character index falls in, if it is not
// at its start, or the length if it is at or past the end.
uint32_t rc_str_utf8_ceil(rc_str s, uint32_t index);

// True if index is a character boundary, which the end of the string is, and past it is not.
bool rc_str_utf8_is_boundary(rc_str s, uint32_t index);

// True if all of s is well-formed UTF-8.
bool rc_str_utf8_is_valid(rc_str s);

// How many steps it takes to walk s: its codepoints, with each malformed part as one.
uint32_t rc_str_utf8_count(rc_str s);

// ---- conversion ----

// s as a C string. If a null already follows the text, we hand back s.data itself; otherwise we
// copy what fits into buf, terminate it and return buf (NULL if there is no buf). Reads the byte
// after the text.
const char *rc_str_as_cstr(rc_str s, char *buf, uint32_t buf_size);

// ---- numbers ----

// Parsing reads a number from the start of a string and hands back the text after it, as C++'s
// std::from_chars does: no whitespace is skipped, the text never depends on a locale, and the
// longest number there is taken, so "12e" gives 12 with "e" left. The as_ functions take all of
// the string or fail. Nothing is written through a pointer: each returns an rc_parsed_<type>.
//
// Integers take base 2 to 36, with digits past 9 as letters in either case, or base 0, which
// reads a 0x or 0b prefix as hex or binary and anything else as decimal (a leading 0 is never
// octal). Floats take base 10, base 16 (hex digits with a p exponent of two, as in 0x1.8p3 without
// the 0x), or base 0, which reads a 0x prefix as hex. Floats also read inf, infinity and nan in
// any case. A minus sign may lead, except for unsigned types; a plus sign never may, though an
// exponent's may.
//
// Floats round correctly, to nearest with ties to even, so the same text gives the same bits on
// every target. Underflow rounds to a subnormal or zero, which is no error; overflow is.

// Why a parse failed. The zero is no error.
typedef enum rc_parse_error {
    RC_PARSE_ERROR_NONE,
    RC_PARSE_ERROR_SYNTAX,  // no number at the start, which rest is then all of
    RC_PARSE_ERROR_RANGE    // a number too big for the type, whose text rest is after
} rc_parse_error;

// What a parse found: the value, 0 on any error, and the text after it.
typedef struct rc_parsed_i8 {
    int8_t value;
    rc_str rest;
    uint8_t error;  // rc_parse_error
} rc_parsed_i8;

typedef struct rc_parsed_i16 {
    int16_t value;
    rc_str rest;
    uint8_t error;  // rc_parse_error
} rc_parsed_i16;

typedef struct rc_parsed_i32 {
    int32_t value;
    rc_str rest;
    uint8_t error;  // rc_parse_error
} rc_parsed_i32;

typedef struct rc_parsed_i64 {
    int64_t value;
    rc_str rest;
    uint8_t error;  // rc_parse_error
} rc_parsed_i64;

typedef struct rc_parsed_u8 {
    uint8_t value;
    rc_str rest;
    uint8_t error;  // rc_parse_error
} rc_parsed_u8;

typedef struct rc_parsed_u16 {
    uint16_t value;
    rc_str rest;
    uint8_t error;  // rc_parse_error
} rc_parsed_u16;

typedef struct rc_parsed_u32 {
    uint32_t value;
    rc_str rest;
    uint8_t error;  // rc_parse_error
} rc_parsed_u32;

typedef struct rc_parsed_u64 {
    uint64_t value;
    rc_str rest;
    uint8_t error;  // rc_parse_error
} rc_parsed_u64;

typedef struct rc_parsed_f32 {
    float value;
    rc_str rest;
    uint8_t error;  // rc_parse_error
} rc_parsed_f32;

typedef struct rc_parsed_f64 {
    double value;
    rc_str rest;
    uint8_t error;  // rc_parse_error
} rc_parsed_f64;

// Parse an int8_t from the start of s in base, and the text after it.
rc_parsed_i8 rc_str_parse_i8(rc_str s, uint32_t base);

// Parse all of s as an int8_t in base: RC_PARSE_ERROR_SYNTAX if anything follows the number.
rc_parsed_i8 rc_str_as_i8(rc_str s, uint32_t base);

// Parse an int16_t from the start of s in base, and the text after it.
rc_parsed_i16 rc_str_parse_i16(rc_str s, uint32_t base);

// Parse all of s as an int16_t in base: RC_PARSE_ERROR_SYNTAX if anything follows the number.
rc_parsed_i16 rc_str_as_i16(rc_str s, uint32_t base);

// Parse an int32_t from the start of s in base, and the text after it.
rc_parsed_i32 rc_str_parse_i32(rc_str s, uint32_t base);

// Parse all of s as an int32_t in base: RC_PARSE_ERROR_SYNTAX if anything follows the number.
rc_parsed_i32 rc_str_as_i32(rc_str s, uint32_t base);

// Parse an int64_t from the start of s in base, and the text after it.
rc_parsed_i64 rc_str_parse_i64(rc_str s, uint32_t base);

// Parse all of s as an int64_t in base: RC_PARSE_ERROR_SYNTAX if anything follows the number.
rc_parsed_i64 rc_str_as_i64(rc_str s, uint32_t base);

// Parse an uint8_t from the start of s in base, and the text after it.
rc_parsed_u8 rc_str_parse_u8(rc_str s, uint32_t base);

// Parse all of s as an uint8_t in base: RC_PARSE_ERROR_SYNTAX if anything follows the number.
rc_parsed_u8 rc_str_as_u8(rc_str s, uint32_t base);

// Parse an uint16_t from the start of s in base, and the text after it.
rc_parsed_u16 rc_str_parse_u16(rc_str s, uint32_t base);

// Parse all of s as an uint16_t in base: RC_PARSE_ERROR_SYNTAX if anything follows the number.
rc_parsed_u16 rc_str_as_u16(rc_str s, uint32_t base);

// Parse an uint32_t from the start of s in base, and the text after it.
rc_parsed_u32 rc_str_parse_u32(rc_str s, uint32_t base);

// Parse all of s as an uint32_t in base: RC_PARSE_ERROR_SYNTAX if anything follows the number.
rc_parsed_u32 rc_str_as_u32(rc_str s, uint32_t base);

// Parse an uint64_t from the start of s in base, and the text after it.
rc_parsed_u64 rc_str_parse_u64(rc_str s, uint32_t base);

// Parse all of s as an uint64_t in base: RC_PARSE_ERROR_SYNTAX if anything follows the number.
rc_parsed_u64 rc_str_as_u64(rc_str s, uint32_t base);

// Parse a float from the start of s in base, and the text after it.
rc_parsed_f32 rc_str_parse_f32(rc_str s, uint32_t base);

// Parse all of s as a float in base: RC_PARSE_ERROR_SYNTAX if anything follows the number.
rc_parsed_f32 rc_str_as_f32(rc_str s, uint32_t base);

// Parse a double from the start of s in base, and the text after it.
rc_parsed_f64 rc_str_parse_f64(rc_str s, uint32_t base);

// Parse all of s as a double in base: RC_PARSE_ERROR_SYNTAX if anything follows the number.
rc_parsed_f64 rc_str_as_f64(rc_str s, uint32_t base);

#endif // RC_STR_H_
