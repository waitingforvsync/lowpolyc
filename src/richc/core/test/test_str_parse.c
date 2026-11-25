// test_str_parse.c
//
// Tests for parsing integers from strings.

#include "richc/str.h"

#include "richc/test.h"


// True if a parse left exactly rest.
static bool rest_is(rc_str rest, const char *expected)
{
    return rc_str_is_equal(rest, rc_str_from_cstr(expected));
}

// ---- limits ----

RC_TEST(str_parse, signed_limits)
{
    RC_CHECK(rc_str_parse_i8(RC_STR("127"), 10).value, ==, 127);
    RC_CHECK(rc_str_parse_i8(RC_STR("-128"), 10).value, ==, -128);
    RC_CHECK(rc_str_parse_i16(RC_STR("32767"), 10).value, ==, 32767);
    RC_CHECK(rc_str_parse_i16(RC_STR("-32768"), 10).value, ==, -32768);
    RC_CHECK(rc_str_parse_i32(RC_STR("2147483647"), 10).value, ==, INT32_MAX);
    RC_CHECK(rc_str_parse_i32(RC_STR("-2147483648"), 10).value, ==, INT32_MIN);
    RC_CHECK(rc_str_parse_i64(RC_STR("9223372036854775807"), 10).value, ==, INT64_MAX);
    RC_CHECK(rc_str_parse_i64(RC_STR("-9223372036854775808"), 10).value, ==, INT64_MIN);
    RC_CHECK(rc_str_parse_i64(RC_STR("-9223372036854775808"), 10).error, ==, RC_PARSE_ERROR_NONE);
}


RC_TEST(str_parse, signed_past_the_limits)
{
    rc_parsed_i8 over = rc_str_parse_i8(RC_STR("128;"), 10);
    rc_parsed_i8 under = rc_str_parse_i8(RC_STR("-129"), 10);
    rc_parsed_i64 over64 = rc_str_parse_i64(RC_STR("9223372036854775808"), 10);
    rc_parsed_i64 under64 = rc_str_parse_i64(RC_STR("-9223372036854775809"), 10);

    // RANGE gives 0, with the number's text read
    RC_CHECK(over.error, ==, RC_PARSE_ERROR_RANGE);
    RC_CHECK(over.value, ==, 0);
    RC_CHECK_TRUE(rest_is(over.rest, ";"));
    RC_CHECK(under.error, ==, RC_PARSE_ERROR_RANGE);
    RC_CHECK(over64.error, ==, RC_PARSE_ERROR_RANGE);
    RC_CHECK(under64.error, ==, RC_PARSE_ERROR_RANGE);
    RC_CHECK(rc_str_parse_i16(RC_STR("32768"), 10).error, ==, RC_PARSE_ERROR_RANGE);
    RC_CHECK(rc_str_parse_i32(RC_STR("-2147483649"), 10).error, ==, RC_PARSE_ERROR_RANGE);
}


RC_TEST(str_parse, unsigned_limits)
{
    RC_CHECK(rc_str_parse_u8(RC_STR("255"), 10).value, ==, 255);
    RC_CHECK(rc_str_parse_u16(RC_STR("65535"), 10).value, ==, 65535);
    RC_CHECK(rc_str_parse_u32(RC_STR("4294967295"), 10).value, ==, UINT32_MAX);
    RC_CHECK(rc_str_parse_u64(RC_STR("18446744073709551615"), 10).value, ==, UINT64_MAX);
    RC_CHECK(rc_str_parse_u8(RC_STR("256"), 10).error, ==, RC_PARSE_ERROR_RANGE);
    RC_CHECK(rc_str_parse_u16(RC_STR("65536"), 10).error, ==, RC_PARSE_ERROR_RANGE);
    RC_CHECK(rc_str_parse_u32(RC_STR("4294967296"), 10).error, ==, RC_PARSE_ERROR_RANGE);
    RC_CHECK(rc_str_parse_u64(RC_STR("18446744073709551616"), 10).error, ==, RC_PARSE_ERROR_RANGE);
}


RC_TEST(str_parse, far_past_64_bits)
{
    // Every digit is read, however many
    rc_parsed_u64 parsed = rc_str_parse_u64(RC_STR("123456789012345678901234567890 next"), 10);

    RC_CHECK(parsed.error, ==, RC_PARSE_ERROR_RANGE);
    RC_CHECK(parsed.value, ==, 0u);
    RC_CHECK_TRUE(rest_is(parsed.rest, " next"));
}

// ---- signs ----

RC_TEST(str_parse, signs)
{
    rc_parsed_u32 minus_unsigned = rc_str_parse_u32(RC_STR("-1"), 10);
    rc_parsed_i32 plus = rc_str_parse_i32(RC_STR("+1"), 10);
    rc_parsed_i32 alone = rc_str_parse_i32(RC_STR("-"), 10);
    rc_parsed_i32 minus_zero = rc_str_parse_i32(RC_STR("-0"), 10);

    // A minus sign is for signed types only, and a plus sign is never read
    RC_CHECK(minus_unsigned.error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK_TRUE(rest_is(minus_unsigned.rest, "-1"));
    RC_CHECK(plus.error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK_TRUE(rest_is(plus.rest, "+1"));
    RC_CHECK(alone.error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK_TRUE(rest_is(alone.rest, "-"));
    RC_CHECK(minus_zero.error, ==, RC_PARSE_ERROR_NONE);
    RC_CHECK(minus_zero.value, ==, 0);
}

// ---- bases ----

RC_TEST(str_parse, bases)
{
    RC_CHECK(rc_str_parse_u32(RC_STR("1010"), 2).value, ==, 10u);
    RC_CHECK(rc_str_parse_u32(RC_STR("777"), 8).value, ==, 511u);
    RC_CHECK(rc_str_parse_u32(RC_STR("ff"), 16).value, ==, 255u);
    RC_CHECK(rc_str_parse_u32(RC_STR("FF"), 16).value, ==, 255u);
    RC_CHECK(rc_str_parse_u32(RC_STR("zZ"), 36).value, ==, 1295u);
    RC_CHECK(rc_str_parse_i32(RC_STR("-7f"), 16).value, ==, -127);
}


RC_TEST(str_parse, base_zero_reads_prefixes)
{
    rc_parsed_u32 hex = rc_str_parse_u32(RC_STR("0x1F"), 0);
    rc_parsed_u32 binary = rc_str_parse_u32(RC_STR("0B101"), 0);
    rc_parsed_u32 not_octal = rc_str_parse_u32(RC_STR("017"), 0);
    rc_parsed_i32 negative_hex = rc_str_parse_i32(RC_STR("-0x10"), 0);

    RC_CHECK(hex.value, ==, 31u);
    RC_CHECK(binary.value, ==, 5u);
    RC_CHECK(not_octal.value, ==, 17u);
    RC_CHECK(negative_hex.value, ==, -16);
}


RC_TEST(str_parse, prefixes_without_digits)
{
    // A prefix needs a digit of its base after it, or only the 0 is read
    rc_parsed_u32 bare = rc_str_parse_u32(RC_STR("0x"), 0);
    rc_parsed_u32 not_hex = rc_str_parse_u32(RC_STR("0xg"), 0);
    rc_parsed_u32 not_binary = rc_str_parse_u32(RC_STR("0b2"), 0);
    rc_parsed_u32 explicit_base = rc_str_parse_u32(RC_STR("0x1F"), 16);

    RC_CHECK(bare.value, ==, 0u);
    RC_CHECK_TRUE(rest_is(bare.rest, "x"));
    RC_CHECK_TRUE(rest_is(not_hex.rest, "xg"));
    RC_CHECK_TRUE(rest_is(not_binary.rest, "b2"));

    // With the base given, there is no prefix
    RC_CHECK(explicit_base.value, ==, 0u);
    RC_CHECK_TRUE(rest_is(explicit_base.rest, "x1F"));
}

// ---- greed ----

RC_TEST(str_parse, takes_the_longest_number)
{
    rc_parsed_i32 spaced = rc_str_parse_i32(RC_STR("123 456"), 10);
    rc_parsed_i32 lettered = rc_str_parse_i32(RC_STR("12ab"), 10);
    rc_parsed_i32 hex_letters = rc_str_parse_i32(RC_STR("12ab"), 16);

    RC_CHECK(spaced.value, ==, 123);
    RC_CHECK_TRUE(rest_is(spaced.rest, " 456"));
    RC_CHECK(lettered.value, ==, 12);
    RC_CHECK_TRUE(rest_is(lettered.rest, "ab"));
    RC_CHECK(hex_letters.value, ==, 0x12ab);
    RC_CHECK_TRUE(rest_is(hex_letters.rest, ""));
}


RC_TEST(str_parse, no_number)
{
    rc_parsed_i32 empty = rc_str_parse_i32(RC_STR(""), 10);
    rc_parsed_i32 space = rc_str_parse_i32(RC_STR(" 1"), 10);
    rc_parsed_i32 letters = rc_str_parse_i32(RC_STR("abc"), 10);
    rc_parsed_i32 none = rc_str_parse_i32((rc_str) {0}, 10);

    RC_CHECK(empty.error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK(space.error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK_TRUE(rest_is(space.rest, " 1"));
    RC_CHECK(letters.error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK(none.error, ==, RC_PARSE_ERROR_SYNTAX);
}

// ---- all of the string ----

RC_TEST(str_parse, as_takes_all_of_the_string)
{
    rc_parsed_i32 whole = rc_str_as_i32(RC_STR("-42"), 10);
    rc_parsed_i32 trailing = rc_str_as_i32(RC_STR("42x"), 10);
    rc_parsed_u8 too_big = rc_str_as_u8(RC_STR("999"), 10);
    rc_parsed_u8 too_big_and_trailing = rc_str_as_u8(RC_STR("999x"), 10);

    RC_CHECK(whole.value, ==, -42);
    RC_CHECK(whole.error, ==, RC_PARSE_ERROR_NONE);
    RC_CHECK(trailing.error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK(trailing.value, ==, 0);
    RC_CHECK_TRUE(rest_is(trailing.rest, "x"));
    RC_CHECK(too_big.error, ==, RC_PARSE_ERROR_RANGE);
    RC_CHECK(too_big_and_trailing.error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK(rc_str_as_u64(RC_STR(""), 10).error, ==, RC_PARSE_ERROR_SYNTAX);
}


RC_TEST(str_parse, as_for_every_type)
{
    RC_CHECK(rc_str_as_i8(RC_STR("-8"), 10).value, ==, -8);
    RC_CHECK(rc_str_as_i16(RC_STR("-16"), 10).value, ==, -16);
    RC_CHECK(rc_str_as_i32(RC_STR("-32"), 10).value, ==, -32);
    RC_CHECK(rc_str_as_i64(RC_STR("-64"), 10).value, ==, -64);
    RC_CHECK(rc_str_as_u8(RC_STR("0x8"), 0).value, ==, 8u);
    RC_CHECK(rc_str_as_u16(RC_STR("16"), 10).value, ==, 16u);
    RC_CHECK(rc_str_as_u32(RC_STR("32"), 10).value, ==, 32u);
    RC_CHECK(rc_str_as_u64(RC_STR("64"), 10).value, ==, 64u);
}
