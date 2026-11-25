// test_str_parse_float.c
//
// Tests for parsing floats from strings. The expected bits of the hard cases are glibc's strtod
// and strtof, which round correctly.

#include "richc/str.h"

#include "richc/arena.h"
#include "richc/math/math.h"
#include "richc/mem.h"
#include "richc/mstr.h"
#include "richc/random.h"
#include "richc/test.h"


// The bits of the double parsed from all of text in base 0, or a marker if that failed.
static uint64_t f64_bits(const char *text)
{
    rc_parsed_f64 parsed = rc_str_as_f64(rc_str_from_cstr(text), 0);

    return parsed.error == RC_PARSE_ERROR_NONE ? rc_bitcast_f64(parsed.value) : 0xdeadull;
}


// The same for a float.
static uint32_t f32_bits(const char *text)
{
    rc_parsed_f32 parsed = rc_str_as_f32(rc_str_from_cstr(text), 0);

    return parsed.error == RC_PARSE_ERROR_NONE ? rc_bitcast_f32(parsed.value) : 0xdeadu;
}


// True if a parse left exactly rest.
static bool rest_is(rc_str rest, const char *expected)
{
    return rc_str_is_equal(rest, rc_str_from_cstr(expected));
}

// ---- forms ----

RC_TEST(str_parse_float, decimal_forms)
{
    RC_CHECK(rc_str_parse_f64(RC_STR("1.5"), 10).value, ==, 1.5);
    RC_CHECK(rc_str_parse_f64(RC_STR(".5"), 10).value, ==, 0.5);
    RC_CHECK(rc_str_parse_f64(RC_STR("5."), 10).value, ==, 5.0);
    RC_CHECK(rc_str_parse_f64(RC_STR("1e10"), 10).value, ==, 1e10);
    RC_CHECK(rc_str_parse_f64(RC_STR("1E-5"), 10).value, ==, 1e-5);
    RC_CHECK(rc_str_parse_f64(RC_STR("1.e+5"), 10).value, ==, 1e5);
    RC_CHECK(rc_str_parse_f64(RC_STR("-2.5e-3"), 10).value, ==, -2.5e-3);
    RC_CHECK(rc_str_parse_f64(RC_STR("000123.4500"), 10).value, ==, 123.45);
}


RC_TEST(str_parse_float, zeros)
{
    RC_CHECK(f64_bits("0"), ==, 0u);
    RC_CHECK(f64_bits("-0"), ==, 0x8000000000000000ull);
    RC_CHECK(f64_bits("0.000e999999999"), ==, 0u);
    RC_CHECK(f32_bits("-0.0"), ==, 0x80000000u);
}


RC_TEST(str_parse_float, takes_the_longest_number)
{
    rc_parsed_f64 bare_e = rc_str_parse_f64(RC_STR("1e"), 10);
    rc_parsed_f64 signed_e = rc_str_parse_f64(RC_STR("1e+"), 10);
    rc_parsed_f64 trailing = rc_str_parse_f64(RC_STR("1.5e+3x"), 10);
    rc_parsed_f64 point = rc_str_parse_f64(RC_STR("7.x"), 10);

    // An exponent without digits is left behind
    RC_CHECK(bare_e.value, ==, 1.0);
    RC_CHECK_TRUE(rest_is(bare_e.rest, "e"));
    RC_CHECK_TRUE(rest_is(signed_e.rest, "e+"));
    RC_CHECK(trailing.value, ==, 1500.0);
    RC_CHECK_TRUE(rest_is(trailing.rest, "x"));
    RC_CHECK(point.value, ==, 7.0);
    RC_CHECK_TRUE(rest_is(point.rest, "x"));
}


RC_TEST(str_parse_float, no_number)
{
    rc_parsed_f64 point = rc_str_parse_f64(RC_STR("."), 10);
    rc_parsed_f64 plus = rc_str_parse_f64(RC_STR("+1"), 10);
    rc_parsed_f64 exponent_only = rc_str_parse_f64(RC_STR("e5"), 10);

    RC_CHECK(point.error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK_TRUE(rest_is(point.rest, "."));
    RC_CHECK(plus.error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK(exponent_only.error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK(rc_str_parse_f64(RC_STR(""), 10).error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK(rc_str_parse_f64(RC_STR("-"), 10).error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK(rc_str_parse_f32(RC_STR("x"), 0).error, ==, RC_PARSE_ERROR_SYNTAX);
}


RC_TEST(str_parse_float, infinity_and_nan)
{
    rc_parsed_f64 infinite = rc_str_parse_f64(RC_STR("infinite"), 10);
    rc_parsed_f64 nan_payload = rc_str_parse_f64(RC_STR("nan(1)"), 10);

    RC_CHECK(f64_bits("inf"), ==, 0x7ff0000000000000ull);
    RC_CHECK(f64_bits("-Infinity"), ==, 0xfff0000000000000ull);
    RC_CHECK(f64_bits("NaN"), ==, 0x7ff8000000000000ull);
    RC_CHECK(f32_bits("-INF"), ==, 0xff800000u);
    RC_CHECK(f32_bits("nan"), ==, 0x7fc00000u);

    // Only a whole word is taken
    RC_CHECK_TRUE(rest_is(infinite.rest, "inite"));
    RC_CHECK_TRUE(rest_is(nan_payload.rest, "(1)"));
}

// ---- hex ----

RC_TEST(str_parse_float, hex)
{
    rc_parsed_f64 given_base = rc_str_parse_f64(RC_STR("1.8p3"), 16);
    rc_parsed_f64 prefix_in_base_16 = rc_str_parse_f64(RC_STR("0x1p3"), 16);
    rc_parsed_f64 prefix_in_base_10 = rc_str_parse_f64(RC_STR("0x1p3"), 10);

    RC_CHECK(rc_str_parse_f64(RC_STR("0x1.8p3"), 0).value, ==, 12.0);
    RC_CHECK(rc_str_parse_f64(RC_STR("0X.8P1"), 0).value, ==, 1.0);
    RC_CHECK(rc_str_parse_f64(RC_STR("-0xAp-1"), 0).value, ==, -5.0);
    RC_CHECK(given_base.value, ==, 12.0);

    // With the base given there is no prefix, so only the 0 is read
    RC_CHECK(prefix_in_base_16.value, ==, 0.0);
    RC_CHECK_TRUE(rest_is(prefix_in_base_16.rest, "x1p3"));
    RC_CHECK_TRUE(rest_is(prefix_in_base_10.rest, "x1p3"));
}


RC_TEST(str_parse_float, hex_rounding)
{
    // Ties to even, with every digit counted
    RC_CHECK(f64_bits("0x1.00000000000008p0"), ==, 0x3ff0000000000000ull);
    RC_CHECK(f64_bits("0x1.0000000000000801p0"), ==, 0x3ff0000000000001ull);
    RC_CHECK(f64_bits("0x1.8p-1074"), ==, 0x2ull);
    RC_CHECK(f64_bits("0x1p-1075"), ==, 0u);
    RC_CHECK(f32_bits("0x1.000001p0"), ==, 0x3f800000u);
    RC_CHECK(f32_bits("0x1.0000018p0"), ==, 0x3f800001u);
}

// ---- doubles ----

RC_TEST(str_parse_float, double_values)
{
    RC_CHECK(f64_bits("0.1"), ==, 0x3fb999999999999aull);
    RC_CHECK(f64_bits("123.456"), ==, 0x405edd2f1a9fbe77ull);
    RC_CHECK(f64_bits("-0.000001234"), ==, 0xbeb4b3fd5942cd96ull);
    RC_CHECK(f64_bits("1e23"), ==, 0x44b52d02c7e14af6ull);
    RC_CHECK(f64_bits("8.98846567431158e307"), ==, 0x7fe0000000000000ull);
}


RC_TEST(str_parse_float, double_ties)
{
    // 2^53 + 1 and 2^53 + 3 are halfway, and go to the even neighbour
    RC_CHECK(f64_bits("9007199254740993"), ==, 0x4340000000000000ull);
    RC_CHECK(f64_bits("9007199254740995"), ==, 0x4340000000000002ull);
}


RC_TEST(str_parse_float, double_limits)
{
    rc_parsed_f64 past_max = rc_str_parse_f64(RC_STR("1.7976931348623159e308;"), 10);

    // Below halfway to 2^1024 is the largest double; at or above it overflows
    RC_CHECK(f64_bits("1.7976931348623157e308"), ==, 0x7fefffffffffffffull);
    RC_CHECK(f64_bits("1.7976931348623158e308"), ==, 0x7fefffffffffffffull);
    RC_CHECK(past_max.error, ==, RC_PARSE_ERROR_RANGE);
    RC_CHECK(past_max.value, ==, 0.0);
    RC_CHECK_TRUE(rest_is(past_max.rest, ";"));
    RC_CHECK(rc_str_parse_f64(RC_STR("1e309"), 10).error, ==, RC_PARSE_ERROR_RANGE);
    RC_CHECK(rc_str_parse_f64(RC_STR("-1e99999"), 10).error, ==, RC_PARSE_ERROR_RANGE);
}


RC_TEST(str_parse_float, double_subnormals)
{
    // The smallest normal, the smallest subnormal, and either side of half of it
    RC_CHECK(f64_bits("2.2250738585072014e-308"), ==, 0x0010000000000000ull);
    RC_CHECK(f64_bits("4.9406564584124654e-324"), ==, 0x1ull);
    RC_CHECK(f64_bits("2.4703282292062328e-324"), ==, 0x1ull);
    RC_CHECK(f64_bits("2.4703282292062327e-324"), ==, 0u);

    // Underflow is no error
    RC_CHECK(rc_str_parse_f64(RC_STR("1e-400"), 10).error, ==, RC_PARSE_ERROR_NONE);
    RC_CHECK(f64_bits("-1e-400"), ==, 0x8000000000000000ull);
}


// Write text then count zeros into buf, terminated.
static void with_zeros(char *buf, const char *text, uint32_t count)
{
    uint32_t len = (uint32_t)rc_str_from_cstr(text).len;

    rc_memcpy(buf, text, len);
    rc_memset(buf + len, '0', count);
    buf[len + count] = 0;
}


RC_TEST(str_parse_float, very_long_text)
{
    char one[1024] = {0};
    char tie[1024] = {0};
    char past_tie[1024] = {0};

    // 1 then 900 zeros, scaled back down to 1
    with_zeros(one, "1", 900);
    rc_memcpy(one + 901, "e-900", 6);
    RC_CHECK(f64_bits(one), ==, 0x3ff0000000000000ull);

    // 2^53 + 1 is a tie, which goes down, but a 1 900 zeros past it goes up
    with_zeros(tie, "9007199254740993.", 900);
    with_zeros(past_tie, "9007199254740993.", 900);
    past_tie[917] = '1';
    RC_CHECK(f64_bits(tie), ==, 0x4340000000000000ull);
    RC_CHECK(f64_bits(past_tie), ==, 0x4340000000000001ull);
}

// ---- floats ----

RC_TEST(str_parse_float, float_values)
{
    RC_CHECK(f32_bits("0.1"), ==, 0x3dcccccdu);
    RC_CHECK(f32_bits("3.4028235e38"), ==, 0x7f7fffffu);
    RC_CHECK(f32_bits("1.17549435e-38"), ==, 0x00800000u);
    RC_CHECK(f32_bits("1.4e-45"), ==, 0x1u);
    RC_CHECK(f32_bits("7.1e-46"), ==, 0x1u);
    RC_CHECK(f32_bits("7e-46"), ==, 0u);
    RC_CHECK(f32_bits("16777217"), ==, 0x4b800000u);
    RC_CHECK(rc_str_parse_f32(RC_STR("3.4028236e38"), 10).error, ==, RC_PARSE_ERROR_RANGE);
}


RC_TEST(str_parse_float, float_never_rounds_twice)
{
    // 1 + 2^-24 + 2^-80: just past halfway, so up. Through a double it would round to the
    // halfway point and then to even, down.
    RC_CHECK(f32_bits("1.000000059604644775390625827180612553027674871408692069962853565812110900878906250"), ==, 0x3f800001u);
    RC_CHECK(f32_bits("1.0000000596046447753906249"), ==, 0x3f800000u);
}

// ---- all of the string ----

RC_TEST(str_parse_float, as_takes_all_of_the_string)
{
    rc_parsed_f64 trailing = rc_str_as_f64(RC_STR("1.5 "), 10);
    rc_parsed_f32 whole = rc_str_as_f32(RC_STR("-2.25"), 10);

    RC_CHECK(trailing.error, ==, RC_PARSE_ERROR_SYNTAX);
    RC_CHECK(trailing.value, ==, 0.0);
    RC_CHECK(whole.value, ==, -2.25f);
    RC_CHECK(rc_str_as_f64(RC_STR("1e999"), 10).error, ==, RC_PARSE_ERROR_RANGE);
}

// ---- round trips ----

// The cases that need an arena run against a fresh default one.
RC_TEST_GROUP_DATA(str_parse_float) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(str_parse_float, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(str_parse_float, fix)
{
    rc_arena_release(&fix->a);
}


// A random double from its bits, NaN made 1.
static double random_double(rc_random *random)
{
    uint64_t high = rc_random_next(random);
    double value = rc_bitcast_u64(high << 32 | rc_random_next(random));

    return value == value ? value : 1.0;
}


RC_TEST_STEP(str_parse_float, double_round_trips, fix)
{
    // 17 significant digits always read back to the same double
    rc_random random = rc_random_make(17);
    rc_mstr text = rc_mstr_make(64, &fix->a);
    uint32_t differ = 0;

    for (uint32_t i = 0; i < 10000; i++) {
        double value = random_double(&random);

        rc_mstr_reset(&text);
        rc_mstr_append_f64(&text, value, (rc_float_format) {.precision = 17}, &fix->a);
        differ += rc_bitcast_f64(rc_str_as_f64(text.view, 10).value) != rc_bitcast_f64(value) ? 1u : 0u;
    }
    RC_CHECK(differ, ==, 0u);
}


RC_TEST_STEP(str_parse_float, float_round_trips, fix)
{
    // 9 significant digits always read back to the same float
    rc_random random = rc_random_make(9);
    rc_mstr text = rc_mstr_make(64, &fix->a);
    uint32_t differ = 0;

    for (uint32_t i = 0; i < 10000; i++) {
        float value = rc_bitcast_u32(rc_random_next(&random));
        float number = value == value ? value : 1.0f;

        rc_mstr_reset(&text);
        rc_mstr_append_f32(&text, number, (rc_float_format) {.precision = 9}, &fix->a);
        differ += rc_bitcast_f32(rc_str_as_f32(text.view, 10).value) != rc_bitcast_f32(number) ? 1u : 0u;
    }
    RC_CHECK(differ, ==, 0u);
}
