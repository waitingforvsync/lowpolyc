// test_mstr_printf.c
//
// Checks rc_mstr_append_f64 against the C library's printf, which wasm has none of, so these run
// natively only.

#include "richc/mstr.h"

#include <stdio.h>

#include "richc/arena.h"
#include "richc/math/math.h"
#include "richc/test.h"

RC_TEST_GROUP_DATA(mstr_printf) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(mstr_printf, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(mstr_printf, fix)
{
    rc_arena_release(&fix->a);
}


// value written with fmt, as a fresh string.
static rc_str float_text(double value, rc_float_format fmt, rc_arena *arena)
{
    rc_mstr s = {0};

    rc_mstr_append_f64(&s, value, fmt, arena);
    return s.view;
}


// True if value with fmt matches what the C library's printf gives.
static bool float_matches_printf(double value, rc_float_format fmt, rc_arena *arena)
{
    static char expected[1024];
    int32_t precision = fmt.precision != 0 ? fmt.precision : 6;
    int32_t len = snprintf(expected, sizeof expected, fmt.fixed ? "%.*f" : "%.*g", precision, value);
    rc_str got = float_text(value, fmt, arena);

    if (!rc_str_is_equal(got, rc_str_from_ptr(expected, (uint32_t)len))) {
        printf("    %.17g with %s%d: expected %s, got %.*s\n", value, fmt.fixed ? "f" : "g", precision, expected, (int)got.len, got.data);

        // The runner prints unbuffered, so this must be out before it carries on
        fflush(stdout);
        return false;
    }

    return true;
}


RC_TEST_STEP(mstr_printf, append_f64_extremes_match_printf, fix)
{
    // The largest and smallest, normal and subnormal, at every precision
    static const uint64_t patterns[] = {
        0x7fefffffffffffffull, 0x0010000000000000ull, 0x000fffffffffffffull,
        0x0000000000000001ull, 0x3ff0000000000000ull, 0x3fefffffffffffffull,
        0x4340000000000000ull, 0x433fffffffffffffull, 0x3fb999999999999aull
    };
    bool ok = true;

    for (uint32_t i = 0; i < RC_COUNT(patterns); i++) {
        double value = rc_bitcast_u64(patterns[i]);

        for (uint32_t precision = 1; precision <= 255; precision++) {
            rc_arena_mark mark = rc_arena_save(&fix->a);

            ok &= float_matches_printf(value, (rc_float_format) {.precision = (uint8_t)precision}, &fix->a);
            ok &= float_matches_printf(value, (rc_float_format) {.precision = (uint8_t)precision, .fixed = true}, &fix->a);
            rc_arena_restore(&fix->a, mark);
        }
    }
    RC_CHECK_TRUE(ok);
}


// The next 64 bits from a simple generator, for the randomised tests.
static uint64_t next_random_bits(uint64_t *state)
{
    *state = *state * 6364136223846793005ull + 1442695040888963407ull;
    return *state;
}


// True unless value is an infinity or a NaN, which give NaN when subtracted from themselves.
static bool is_finite(double value)
{
    return value - value == 0.0;
}


// True if value matches printf by default, at precision and at 17, and fixed at precision where
// the fixed text stays a sensible length.
static bool random_value_matches_printf(double value, uint8_t precision, rc_arena *arena)
{
    bool ok = float_matches_printf(value, (rc_float_format) {0}, arena);

    ok &= float_matches_printf(value, (rc_float_format) {.precision = precision}, arena);
    ok &= float_matches_printf(value, (rc_float_format) {.precision = 17}, arena);
    if (value < 1e30 && value > -1e30) {
        ok &= float_matches_printf(value, (rc_float_format) {.precision = precision, .fixed = true}, arena);
    }

    return ok;
}


RC_TEST_STEP(mstr_printf, append_f64_random_matches_printf, fix)
{
    // Random bit patterns cover every exponent; random short decimals land on ties
    uint64_t state = 0x9e3779b97f4a7c15ull;
    bool ok = true;

    for (uint32_t i = 0; i < 20000 && ok; i++) {
        uint64_t bits = next_random_bits(&state);
        double value = i % 2 == 1 ? (double)(int64_t)(bits >> 44) / 1000.0 : rc_bitcast_u64(bits);
        uint8_t precision = (uint8_t)(1 + (bits >> 20) % 20);
        rc_arena_mark mark = rc_arena_save(&fix->a);

        if (!is_finite(value)) {
            continue;
        }

        ok &= random_value_matches_printf(value, precision, &fix->a);
        rc_arena_restore(&fix->a, mark);
    }
    RC_CHECK_TRUE(ok);
}
