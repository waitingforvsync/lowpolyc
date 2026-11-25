// str_parse.c
//
// Parsing integers from strings. Every type reads the same way: an optional minus sign, then the
// digits of the base, all of them, gathered into a 64-bit magnitude that notes when it overflows.
// Each type then checks the magnitude against its own range. str_parse_float.c parses floats.

#include "richc/str.h"

#include "str_parse.h"

// Where an integer's digits start, and the base they are in.
typedef struct rc_parse_digits_at {
    rc_str digits;
    uint32_t base;
} rc_parse_digits_at;

// An integer's text, read but not yet fitted to a type.
typedef struct rc_parse_integer {
    uint64_t magnitude;
    rc_str rest;
    bool negative;
    bool too_big;  // the magnitude passed 2^64 - 1
    bool found;    // there was at least one digit
} rc_parse_integer;


uint32_t rc_parse_digit_value(char c)
{
    if (c >= '0' && c <= '9') {
        return (uint32_t)(c - '0');
    }

    if (c >= 'a' && c <= 'z') {
        return (uint32_t)(c - 'a') + 10u;
    }

    if (c >= 'A' && c <= 'Z') {
        return (uint32_t)(c - 'A') + 10u;
    }
    return 36u;
}


bool rc_parse_is_minus(rc_str s)
{
    return s.len > 0 && s.data[0] == '-';
}


bool rc_parse_has_prefix(rc_str s, char letter, uint32_t base)
{
    if (s.len < 3 || s.data[0] != '0') {
        return false;
    }

    if ((s.data[1] | 0x20) != letter) {
        return false;
    }
    return rc_parse_digit_value(s.data[2]) < base;
}


// The digits of s and their base, past any prefix that sets it: for base 0, a 0x or 0b prefix
// followed by a digit of its base, else decimal.
static rc_parse_digits_at rc_parse_find_digits(rc_str s, uint32_t base)
{
    if (base != 0) {
        return (rc_parse_digits_at) {.digits = s, .base = base};
    }

    if (rc_parse_has_prefix(s, 'x', 16)) {
        return (rc_parse_digits_at) {.digits = rc_str_drop_first(s, 2), .base = 16};
    }

    if (rc_parse_has_prefix(s, 'b', 2)) {
        return (rc_parse_digits_at) {.digits = rc_str_drop_first(s, 2), .base = 2};
    }
    return (rc_parse_digits_at) {.digits = s, .base = 10};
}


// The magnitude of the digits of base at the start of s, every one of them, and the text after.
static rc_parse_integer rc_parse_digits(rc_str s, uint32_t base)
{
    uint64_t magnitude = 0;
    bool too_big = false;
    uint32_t i = 0;

    while (i < s.len && rc_parse_digit_value(s.data[i]) < base) {
        uint32_t digit = rc_parse_digit_value(s.data[i]);

        too_big = too_big || magnitude > (UINT64_MAX - digit) / base;
        magnitude = magnitude * base + digit;
        i++;
    }

    return (rc_parse_integer) {
        .magnitude = magnitude,
        .rest = rc_str_drop_first(s, i),
        .too_big = too_big,
        .found = i > 0
    };
}


// Read an integer from the start of s: a minus sign if allowed, a prefix if base is 0, and the
// digits. Without a digit, nothing is read and rest is all of s.
static rc_parse_integer rc_parse_integer_text(rc_str s, uint32_t base, bool allow_minus)
{
    bool negative = allow_minus && rc_parse_is_minus(s);
    rc_parse_digits_at at = rc_parse_find_digits(rc_str_drop_first(s, negative ? 1 : 0), base);
    rc_parse_integer integer = rc_parse_digits(at.digits, at.base);

    RC_ASSERT(base == 0 || (base >= 2 && base <= 36));
    if (!integer.found) {
        return (rc_parse_integer) {.rest = s};
    }

    integer.negative = negative;
    return integer;
}


// The error for integer read as a type whose magnitudes reach max_positive, or one more when
// negative and the type is signed.
static uint8_t rc_parse_integer_error(rc_parse_integer integer, uint64_t max_positive, bool is_signed)
{
    uint64_t max_negative = is_signed ? max_positive + 1 : 0;

    if (!integer.found) {
        return RC_PARSE_ERROR_SYNTAX;
    }

    if (integer.too_big) {
        return RC_PARSE_ERROR_RANGE;
    }

    if (integer.negative) {
        return integer.magnitude <= max_negative ? RC_PARSE_ERROR_NONE : RC_PARSE_ERROR_RANGE;
    }
    return integer.magnitude <= max_positive ? RC_PARSE_ERROR_NONE : RC_PARSE_ERROR_RANGE;
}


// The signed value of integer, which fits in 64 bits, worked out so the minimum overflows nothing.
static int64_t rc_parse_integer_signed(rc_parse_integer integer)
{
    if (!integer.negative) {
        return (int64_t)integer.magnitude;
    }

    if (integer.magnitude == 0) {
        return 0;
    }
    return -(int64_t)(integer.magnitude - 1) - 1;
}


// A signed parse: the integer, and its error as a type up to max_positive.
static rc_parsed_i64 rc_parse_signed(rc_str s, uint32_t base, uint64_t max_positive)
{
    rc_parse_integer integer = rc_parse_integer_text(s, base, true);
    uint8_t error = rc_parse_integer_error(integer, max_positive, true);

    return (rc_parsed_i64) {
        .value = error == RC_PARSE_ERROR_NONE ? rc_parse_integer_signed(integer) : 0,
        .rest = integer.rest,
        .error = error
    };
}


// An unsigned parse: the integer, and its error as a type up to max.
static rc_parsed_u64 rc_parse_unsigned(rc_str s, uint32_t base, uint64_t max)
{
    rc_parse_integer integer = rc_parse_integer_text(s, base, false);
    uint8_t error = rc_parse_integer_error(integer, max, false);

    return (rc_parsed_u64) {
        .value = error == RC_PARSE_ERROR_NONE ? integer.magnitude : 0,
        .rest = integer.rest,
        .error = error
    };
}


uint8_t rc_parse_whole_error(uint8_t error, rc_str rest)
{
    return rest.len > 0 ? RC_PARSE_ERROR_SYNTAX : error;
}

// ---- signed ----

rc_parsed_i8 rc_str_parse_i8(rc_str s, uint32_t base)
{
    rc_parsed_i64 parsed = rc_parse_signed(s, base, INT8_MAX);

    return (rc_parsed_i8) {.value = (int8_t)parsed.value, .rest = parsed.rest, .error = parsed.error};
}


rc_parsed_i16 rc_str_parse_i16(rc_str s, uint32_t base)
{
    rc_parsed_i64 parsed = rc_parse_signed(s, base, INT16_MAX);

    return (rc_parsed_i16) {.value = (int16_t)parsed.value, .rest = parsed.rest, .error = parsed.error};
}


rc_parsed_i32 rc_str_parse_i32(rc_str s, uint32_t base)
{
    rc_parsed_i64 parsed = rc_parse_signed(s, base, INT32_MAX);

    return (rc_parsed_i32) {.value = (int32_t)parsed.value, .rest = parsed.rest, .error = parsed.error};
}


rc_parsed_i64 rc_str_parse_i64(rc_str s, uint32_t base)
{
    return rc_parse_signed(s, base, INT64_MAX);
}

// ---- unsigned ----

rc_parsed_u8 rc_str_parse_u8(rc_str s, uint32_t base)
{
    rc_parsed_u64 parsed = rc_parse_unsigned(s, base, UINT8_MAX);

    return (rc_parsed_u8) {.value = (uint8_t)parsed.value, .rest = parsed.rest, .error = parsed.error};
}


rc_parsed_u16 rc_str_parse_u16(rc_str s, uint32_t base)
{
    rc_parsed_u64 parsed = rc_parse_unsigned(s, base, UINT16_MAX);

    return (rc_parsed_u16) {.value = (uint16_t)parsed.value, .rest = parsed.rest, .error = parsed.error};
}


rc_parsed_u32 rc_str_parse_u32(rc_str s, uint32_t base)
{
    rc_parsed_u64 parsed = rc_parse_unsigned(s, base, UINT32_MAX);

    return (rc_parsed_u32) {.value = (uint32_t)parsed.value, .rest = parsed.rest, .error = parsed.error};
}


rc_parsed_u64 rc_str_parse_u64(rc_str s, uint32_t base)
{
    return rc_parse_unsigned(s, base, UINT64_MAX);
}

// ---- all of the string ----

rc_parsed_i8 rc_str_as_i8(rc_str s, uint32_t base)
{
    rc_parsed_i8 parsed = rc_str_parse_i8(s, base);
    uint8_t error = rc_parse_whole_error(parsed.error, parsed.rest);

    return (rc_parsed_i8) {.value = error == RC_PARSE_ERROR_NONE ? parsed.value : 0, .rest = parsed.rest, .error = error};
}


rc_parsed_i16 rc_str_as_i16(rc_str s, uint32_t base)
{
    rc_parsed_i16 parsed = rc_str_parse_i16(s, base);
    uint8_t error = rc_parse_whole_error(parsed.error, parsed.rest);

    return (rc_parsed_i16) {.value = error == RC_PARSE_ERROR_NONE ? parsed.value : 0, .rest = parsed.rest, .error = error};
}


rc_parsed_i32 rc_str_as_i32(rc_str s, uint32_t base)
{
    rc_parsed_i32 parsed = rc_str_parse_i32(s, base);
    uint8_t error = rc_parse_whole_error(parsed.error, parsed.rest);

    return (rc_parsed_i32) {.value = error == RC_PARSE_ERROR_NONE ? parsed.value : 0, .rest = parsed.rest, .error = error};
}


rc_parsed_i64 rc_str_as_i64(rc_str s, uint32_t base)
{
    rc_parsed_i64 parsed = rc_str_parse_i64(s, base);
    uint8_t error = rc_parse_whole_error(parsed.error, parsed.rest);

    return (rc_parsed_i64) {.value = error == RC_PARSE_ERROR_NONE ? parsed.value : 0, .rest = parsed.rest, .error = error};
}


rc_parsed_u8 rc_str_as_u8(rc_str s, uint32_t base)
{
    rc_parsed_u8 parsed = rc_str_parse_u8(s, base);
    uint8_t error = rc_parse_whole_error(parsed.error, parsed.rest);

    return (rc_parsed_u8) {.value = error == RC_PARSE_ERROR_NONE ? parsed.value : 0, .rest = parsed.rest, .error = error};
}


rc_parsed_u16 rc_str_as_u16(rc_str s, uint32_t base)
{
    rc_parsed_u16 parsed = rc_str_parse_u16(s, base);
    uint8_t error = rc_parse_whole_error(parsed.error, parsed.rest);

    return (rc_parsed_u16) {.value = error == RC_PARSE_ERROR_NONE ? parsed.value : 0, .rest = parsed.rest, .error = error};
}


rc_parsed_u32 rc_str_as_u32(rc_str s, uint32_t base)
{
    rc_parsed_u32 parsed = rc_str_parse_u32(s, base);
    uint8_t error = rc_parse_whole_error(parsed.error, parsed.rest);

    return (rc_parsed_u32) {.value = error == RC_PARSE_ERROR_NONE ? parsed.value : 0, .rest = parsed.rest, .error = error};
}


rc_parsed_u64 rc_str_as_u64(rc_str s, uint32_t base)
{
    rc_parsed_u64 parsed = rc_str_parse_u64(s, base);
    uint8_t error = rc_parse_whole_error(parsed.error, parsed.rest);

    return (rc_parsed_u64) {.value = error == RC_PARSE_ERROR_NONE ? parsed.value : 0, .rest = parsed.rest, .error = error};
}
