// test_math.c
//
// Tests for richc/math/math.h: the integer operations, overflow checks, angles, bits, and the
// float functions, bit for bit.

#include "richc/math/math.h"

#include "richc/test.h"

// True if x is of the given type, to see which width a dispatcher picked.
#define IS_TYPE(x, type) _Generic((x), type: true, default: false)


RC_TEST(math, min_max)
{
    RC_CHECK(rc_min_i32(3, 7), ==, 3);
    RC_CHECK(rc_max_i32(3, 7), ==, 7);
    RC_CHECK(rc_min_i32(INT32_MIN, INT32_MAX), ==, INT32_MIN);
    RC_CHECK(rc_max_i32(INT32_MIN, INT32_MAX), ==, INT32_MAX);
    RC_CHECK(rc_min_i64(-5, 2), ==, -5);
    RC_CHECK(rc_max_i64(-5, 2), ==, 2);
    RC_CHECK(rc_min_i64(INT64_MIN, INT64_MAX), ==, INT64_MIN);
    RC_CHECK(rc_max_i64(INT64_MIN, INT64_MAX), ==, INT64_MAX);
    RC_CHECK(rc_min_f32(-1.5f, 2.0f), ==, -1.5f);
    RC_CHECK(rc_max_f32(-1.5f, 2.0f), ==, 2.0f);
    RC_CHECK(rc_min_f64(3.0, 3.0), ==, 3.0);
    RC_CHECK(rc_max_f64(-2.0, -7.0), ==, -2.0);
}


RC_TEST(math, min_max_nan)
{
    float nan_f32 = rc_bitcast_u32(0x7fc00000u);
    double nan_f64 = rc_bitcast_u64(0x7ff8000000000000ull);
    float min_with_nan_b = rc_min_f32(1.0f, nan_f32);
    double max_with_nan_b = rc_max_f64(1.0, nan_f64);

    // A NaN fails every comparison, so b comes back whichever is the NaN
    RC_CHECK(rc_min_f32(nan_f32, 1.0f), ==, 1.0f);
    RC_CHECK(rc_max_f32(nan_f32, 1.0f), ==, 1.0f);
    RC_CHECK(rc_min_f64(nan_f64, 1.0), ==, 1.0);
    RC_CHECK(rc_max_f64(nan_f64, 1.0), ==, 1.0);
    RC_CHECK_TRUE(min_with_nan_b != min_with_nan_b);
    RC_CHECK_TRUE(max_with_nan_b != max_with_nan_b);
}


RC_TEST(math, sgn)
{
    RC_CHECK(rc_sgn_i32(-9), ==, -1);
    RC_CHECK(rc_sgn_i32(0), ==, 0);
    RC_CHECK(rc_sgn_i32(9), ==, 1);
    RC_CHECK(rc_sgn_i32(INT32_MIN), ==, -1);
    RC_CHECK(rc_sgn_i32(INT32_MAX), ==, 1);
    RC_CHECK(rc_sgn_i64(-1), ==, -1);
    RC_CHECK(rc_sgn_i64(0), ==, 0);
    RC_CHECK(rc_sgn_i64(INT64_MIN), ==, -1);
    RC_CHECK(rc_sgn_i64(INT64_MAX), ==, 1);
}


RC_TEST(math, gcd)
{
    RC_CHECK(rc_gcd_i32(12, 8), ==, 4);
    RC_CHECK(rc_gcd_i32(17, 5), ==, 1);
    RC_CHECK(rc_gcd_i32(7, 7), ==, 7);
    RC_CHECK(rc_gcd_i64(INT64_C(1000000000000), INT64_C(500000000000)), ==, INT64_C(500000000000));

    // Never negative, whatever the signs
    RC_CHECK(rc_gcd_i32(-12, 8), ==, 4);
    RC_CHECK(rc_gcd_i32(12, -8), ==, 4);
    RC_CHECK(rc_gcd_i32(-12, -8), ==, 4);
    RC_CHECK(rc_gcd_i64(-12, -8), ==, 4);

    // Zero divides by nothing, so the other operand is the answer
    RC_CHECK(rc_gcd_i32(0, 5), ==, 5);
    RC_CHECK(rc_gcd_i32(5, 0), ==, 5);
    RC_CHECK(rc_gcd_i32(0, -5), ==, 5);
    RC_CHECK(rc_gcd_i32(0, 0), ==, 0);
    RC_CHECK(rc_gcd_i64(0, 0), ==, 0);
    RC_CHECK(rc_gcd_i32(INT32_MAX, 0), ==, INT32_MAX);
    RC_CHECK(rc_gcd_i64(INT64_MAX, 0), ==, INT64_MAX);
}


RC_TEST(math, gcd_of_the_most_negative)
{
    // INT32_MIN's magnitude doesn't fit an int32_t, but the gcd of it and anything nonzero does
    RC_CHECK(rc_gcd_i32(INT32_MIN, -1), ==, 1);
    RC_CHECK(rc_gcd_i32(-1, INT32_MIN), ==, 1);
    RC_CHECK(rc_gcd_i32(INT32_MIN, 6), ==, 2);
    RC_CHECK(rc_gcd_i32(INT32_MIN, INT32_MAX), ==, 1);
    RC_CHECK(rc_gcd_i32(INT32_MIN, INT32_MIN / 2), ==, 1 << 30);
    RC_CHECK(rc_gcd_i64(INT64_MIN, -1), ==, 1);
    RC_CHECK(rc_gcd_i64(INT64_MIN, 3), ==, 1);
    RC_CHECK(rc_gcd_i64(INT64_MIN, INT64_MIN / 2), ==, INT64_C(1) << 62);
}


RC_TEST(math, picking_integer_types)
{
    int64_t big = INT64_C(1) << 40;

    RC_CHECK(rc_min(3, 7), ==, 3);
    RC_CHECK(rc_max(3, 7), ==, 7);
    RC_CHECK(rc_min(-big, big), ==, -big);
    RC_CHECK(rc_max(-big, big), ==, big);
    RC_CHECK(rc_min(-1.5f, 2.0f), ==, -1.5f);
    RC_CHECK(rc_max(-1.5f, 2.0f), ==, 2.0f);
    RC_CHECK(rc_min(-2.0, -7.0), ==, -7.0);
    RC_CHECK(rc_max(-2.0, -7.0), ==, -2.0);
    RC_CHECK(rc_sgn(-9), ==, -1);
    RC_CHECK(rc_sgn(-big), ==, -1);
    RC_CHECK(rc_gcd(12, -8), ==, 4);
    RC_CHECK(rc_gcd(big * 3, big * 2), ==, big);

    // Each picks the width of its first argument
    RC_CHECK_TRUE(IS_TYPE(rc_min(3, 7), int32_t));
    RC_CHECK_TRUE(IS_TYPE(rc_max(big, big), int64_t));
    RC_CHECK_TRUE(IS_TYPE(rc_min(1.0f, 2.0f), float));
    RC_CHECK_TRUE(IS_TYPE(rc_max(1.0, 2.0), double));
    RC_CHECK_TRUE(IS_TYPE(rc_sgn(big), int64_t));
    RC_CHECK_TRUE(IS_TYPE(rc_gcd(12, 8), int32_t));
    RC_CHECK_TRUE(IS_TYPE(rc_gcd(big, big), int64_t));
}


RC_TEST(math, add_overflows_u64)
{
    RC_CHECK_FALSE(rc_add_overflows_u64(UINT64_MAX, 0));
    RC_CHECK_FALSE(rc_add_overflows_u64(0, UINT64_MAX));
    RC_CHECK_FALSE(rc_add_overflows_u64(UINT64_MAX - 1, 1));
    RC_CHECK_TRUE(rc_add_overflows_u64(UINT64_MAX, 1));
    RC_CHECK_TRUE(rc_add_overflows_u64(1, UINT64_MAX));
    RC_CHECK_TRUE(rc_add_overflows_u64(UINT64_MAX, UINT64_MAX));
}


RC_TEST(math, mul_overflows_u64)
{
    RC_CHECK_FALSE(rc_mul_overflows_u64(0, UINT64_MAX));
    RC_CHECK_FALSE(rc_mul_overflows_u64(UINT64_MAX, 0));
    RC_CHECK_FALSE(rc_mul_overflows_u64(UINT64_MAX, 1));
    RC_CHECK_FALSE(rc_mul_overflows_u64(1, UINT64_MAX));
    RC_CHECK_TRUE(rc_mul_overflows_u64(UINT64_MAX, 2));

    // 2^32 squared is one past the top; 2^32 times 2^32 - 1 fits
    RC_CHECK_TRUE(rc_mul_overflows_u64(0x100000000ull, 0x100000000ull));
    RC_CHECK_FALSE(rc_mul_overflows_u64(0x100000000ull, 0xffffffffull));
}


RC_TEST(math, add_overflows_i64)
{
    RC_CHECK_FALSE(rc_add_overflows_i64(INT64_MAX, 0));
    RC_CHECK_TRUE(rc_add_overflows_i64(INT64_MAX, 1));
    RC_CHECK_TRUE(rc_add_overflows_i64(1, INT64_MAX));
    RC_CHECK_FALSE(rc_add_overflows_i64(INT64_MIN, 0));
    RC_CHECK_TRUE(rc_add_overflows_i64(INT64_MIN, -1));
    RC_CHECK_TRUE(rc_add_overflows_i64(-1, INT64_MIN));
    RC_CHECK_FALSE(rc_add_overflows_i64(INT64_MIN, INT64_MAX));
    RC_CHECK_TRUE(rc_add_overflows_i64(INT64_MIN, INT64_MIN));
    RC_CHECK_TRUE(rc_add_overflows_i64(INT64_MAX, INT64_MAX));
}


RC_TEST(math, sub_overflows_i64)
{
    RC_CHECK_FALSE(rc_sub_overflows_i64(INT64_MIN, 0));
    RC_CHECK_TRUE(rc_sub_overflows_i64(INT64_MIN, 1));
    RC_CHECK_FALSE(rc_sub_overflows_i64(INT64_MAX, 0));
    RC_CHECK_TRUE(rc_sub_overflows_i64(INT64_MAX, -1));
    RC_CHECK_FALSE(rc_sub_overflows_i64(INT64_MAX, INT64_MAX));
    RC_CHECK_FALSE(rc_sub_overflows_i64(INT64_MIN, INT64_MIN));

    // Negating INT64_MIN overflows, but -1 - INT64_MIN is exactly INT64_MAX
    RC_CHECK_TRUE(rc_sub_overflows_i64(0, INT64_MIN));
    RC_CHECK_FALSE(rc_sub_overflows_i64(-1, INT64_MIN));
}


RC_TEST(math, mul_overflows_i64)
{
    RC_CHECK_FALSE(rc_mul_overflows_i64(1000, 1000));
    RC_CHECK_FALSE(rc_mul_overflows_i64(0, INT64_MIN));
    RC_CHECK_FALSE(rc_mul_overflows_i64(INT64_MIN, 0));
    RC_CHECK_FALSE(rc_mul_overflows_i64(INT64_MIN, 1));
    RC_CHECK_FALSE(rc_mul_overflows_i64(INT64_MAX, -1));
    RC_CHECK_TRUE(rc_mul_overflows_i64(INT64_MAX, 2));
    RC_CHECK_TRUE(rc_mul_overflows_i64(INT64_MIN, -1));
    RC_CHECK_TRUE(rc_mul_overflows_i64(-1, INT64_MIN));
    RC_CHECK_TRUE(rc_mul_overflows_i64(INT64_MIN, 2));

    // 3037000499 is the largest square root that fits
    RC_CHECK_FALSE(rc_mul_overflows_i64(3037000499, 3037000499));
    RC_CHECK_TRUE(rc_mul_overflows_i64(3037000500, 3037000500));
    RC_CHECK_TRUE(rc_mul_overflows_i64(-3037000500, 3037000500));

    // 2^63 is one past INT64_MAX, but -2^63 is INT64_MIN, whichever operand carries the sign
    RC_CHECK_TRUE(rc_mul_overflows_i64(INT64_C(1) << 32, INT64_C(1) << 31));
    RC_CHECK_TRUE(rc_mul_overflows_i64(-(INT64_C(1) << 32), -(INT64_C(1) << 31)));
    RC_CHECK_FALSE(rc_mul_overflows_i64(-(INT64_C(1) << 32), INT64_C(1) << 31));
    RC_CHECK_FALSE(rc_mul_overflows_i64(INT64_C(1) << 32, -(INT64_C(1) << 31)));
}


RC_TEST(math, deg_to_rad)
{
    RC_CHECK(rc_deg_to_rad_f32(0.0f), ==, 0.0f);
    RC_CHECK(rc_deg_to_rad_f32(180.0f), ~=, 3.14159265f);
    RC_CHECK(rc_deg_to_rad_f32(-90.0f), ~=, -1.57079633f);
    RC_CHECK(rc_deg_to_rad_f64(0.0), ==, 0.0);
    RC_CHECK(rc_deg_to_rad_f64(180.0), ==, 3.141592653589793);

    // The dispatcher keeps the width it is given
    RC_CHECK(rc_deg_to_rad(180.0f), ==, rc_deg_to_rad_f32(180.0f));
    RC_CHECK(rc_deg_to_rad(180.0), ==, rc_deg_to_rad_f64(180.0));
    RC_CHECK_TRUE(IS_TYPE(rc_deg_to_rad(180.0f), float));
    RC_CHECK_TRUE(IS_TYPE(rc_deg_to_rad(180.0), double));
}


RC_TEST(math, clz)
{
    RC_CHECK(rc_clz_u32(0), ==, 32u);
    RC_CHECK(rc_clz_u32(1), ==, 31u);
    RC_CHECK(rc_clz_u32(0x80000000u), ==, 0u);
    RC_CHECK(rc_clz_u32(0x00010000u), ==, 15u);
    RC_CHECK(rc_clz_u32(UINT32_MAX), ==, 0u);
    RC_CHECK(rc_clz_u64(0), ==, 64u);
    RC_CHECK(rc_clz_u64(1), ==, 63u);
    RC_CHECK(rc_clz_u64(0x8000000000000000ull), ==, 0u);
    RC_CHECK(rc_clz_u64(0x0000000100000000ull), ==, 31u);
}


RC_TEST(math, ctz)
{
    RC_CHECK(rc_ctz_u32(0), ==, 32u);
    RC_CHECK(rc_ctz_u32(1), ==, 0u);
    RC_CHECK(rc_ctz_u32(0x80000000u), ==, 31u);
    RC_CHECK(rc_ctz_u32(0x00010000u), ==, 16u);
    RC_CHECK(rc_ctz_u32(UINT32_MAX), ==, 0u);
    RC_CHECK(rc_ctz_u64(0), ==, 64u);
    RC_CHECK(rc_ctz_u64(1), ==, 0u);
    RC_CHECK(rc_ctz_u64(0x8000000000000000ull), ==, 63u);
    RC_CHECK(rc_ctz_u64(0x0000000100000000ull), ==, 32u);
}


RC_TEST(math, every_single_bit)
{
    for (uint32_t i = 0; i < 32; i++) {
        RC_CHECK(rc_clz_u32(1u << i), ==, 31 - i);
        RC_CHECK(rc_ctz_u32(1u << i), ==, i);
        RC_CHECK(rc_popcount_u32(1u << i), ==, 1u);
    }
    for (uint32_t i = 0; i < 64; i++) {
        RC_CHECK(rc_clz_u64(1ull << i), ==, 63 - i);
        RC_CHECK(rc_ctz_u64(1ull << i), ==, i);
    }
}


RC_TEST(math, popcount)
{
    RC_CHECK(rc_popcount_u32(0), ==, 0u);
    RC_CHECK(rc_popcount_u32(UINT32_MAX), ==, 32u);
    RC_CHECK(rc_popcount_u32(0x55555555u), ==, 16u);
    RC_CHECK(rc_popcount_u32(0x80000001u), ==, 2u);
}


RC_TEST(math, bitcast)
{
    RC_CHECK(rc_bitcast_f32(1.0f), ==, 0x3f800000u);
    RC_CHECK(rc_bitcast_f32(-0.0f), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f64(1.0), ==, 0x3ff0000000000000ull);
    RC_CHECK(rc_bitcast_f64(-2.0), ==, 0xc000000000000000ull);
    RC_CHECK_TRUE(rc_bitcast_u32(0x3f800000u) == 1.0f);
    RC_CHECK_TRUE(rc_bitcast_u64(0xc000000000000000ull) == -2.0);

    // Round trips keep every bit, signed zeros included
    RC_CHECK(rc_bitcast_f32(rc_bitcast_u32(0x80000000u)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f64(rc_bitcast_u64(0x000fffffffffffffull)), ==, 0x000fffffffffffffull);
}


// ---- square root ----

RC_TEST(math, sqrt_f32)
{
    float infinity = rc_bitcast_u32(0x7f800000u);

    RC_CHECK(rc_sqrt_f32(0.0f), ==, 0.0f);
    RC_CHECK(rc_sqrt_f32(1.0f), ==, 1.0f);
    RC_CHECK(rc_sqrt_f32(4.0f), ==, 2.0f);
    RC_CHECK(rc_sqrt_f32(0.25f), ==, 0.5f);
    RC_CHECK(rc_sqrt_f32(infinity), ==, infinity);

    // Correctly rounded, so every target gives these bits
    RC_CHECK(rc_bitcast_f32(rc_sqrt_f32(2.0f)), ==, 0x3fb504f3u);

    // A denormal isn't flushed to zero: 2^-148 has the root 2^-74
    RC_CHECK(rc_sqrt_f32(rc_bitcast_u32(0x00000002u)), ==, rc_bitcast_u32(0x1a800000u));

    // The root of -0 is -0, and of anything else negative NaN
    RC_CHECK(rc_bitcast_f32(rc_sqrt_f32(-0.0f)), ==, 0x80000000u);
    RC_CHECK_FALSE(rc_is_finite_f32(rc_sqrt_f32(-1.0f)));
}


RC_TEST(math, sqrt_f64)
{
    double infinity = rc_bitcast_u64(0x7ff0000000000000ull);

    RC_CHECK(rc_sqrt_f64(0.0), ==, 0.0);
    RC_CHECK(rc_sqrt_f64(4.0), ==, 2.0);
    RC_CHECK(rc_sqrt_f64(0.25), ==, 0.5);
    RC_CHECK(rc_sqrt_f64(infinity), ==, infinity);
    RC_CHECK(rc_bitcast_f64(rc_sqrt_f64(2.0)), ==, 0x3ff6a09e667f3bcdull);
    RC_CHECK(rc_bitcast_f64(rc_sqrt_f64(-0.0)), ==, 0x8000000000000000ull);
    RC_CHECK_FALSE(rc_is_finite_f64(rc_sqrt_f64(-1.0)));
}


// ---- sign ----

RC_TEST(math, abs)
{
    float infinity_f32 = rc_bitcast_u32(0x7f800000u);
    double infinity_f64 = rc_bitcast_u64(0x7ff0000000000000ull);

    RC_CHECK(rc_abs_f32(-1.5f), ==, 1.5f);
    RC_CHECK(rc_abs_f32(1.5f), ==, 1.5f);
    RC_CHECK(rc_abs_f32(-infinity_f32), ==, infinity_f32);
    RC_CHECK(rc_bitcast_f32(rc_abs_f32(-0.0f)), ==, 0u);
    RC_CHECK(rc_abs_f64(-1.5), ==, 1.5);
    RC_CHECK(rc_abs_f64(1.5), ==, 1.5);
    RC_CHECK(rc_abs_f64(-infinity_f64), ==, infinity_f64);
    RC_CHECK(rc_bitcast_f64(rc_abs_f64(-0.0)), ==, 0ull);
}


RC_TEST(math, copysign)
{
    float infinity = rc_bitcast_u32(0x7f800000u);

    RC_CHECK(rc_copysign_f32(1.5f, -2.0f), ==, -1.5f);
    RC_CHECK(rc_copysign_f32(-1.5f, 2.0f), ==, 1.5f);
    RC_CHECK(rc_copysign_f32(-1.5f, -2.0f), ==, -1.5f);
    RC_CHECK(rc_copysign_f32(infinity, -1.0f), ==, -infinity);
    RC_CHECK(rc_copysign_f64(1.5, -2.0), ==, -1.5);
    RC_CHECK(rc_copysign_f64(-1.5, 2.0), ==, 1.5);

    // Zeros carry a sign too, on either side
    RC_CHECK(rc_copysign_f32(1.5f, -0.0f), ==, -1.5f);
    RC_CHECK(rc_copysign_f32(-1.5f, 0.0f), ==, 1.5f);
    RC_CHECK(rc_bitcast_f32(rc_copysign_f32(0.0f, -1.0f)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f64(rc_copysign_f64(0.0, -1.0)), ==, 0x8000000000000000ull);
    RC_CHECK(rc_copysign_f64(1.5, -0.0), ==, -1.5);
}


// ---- rounding ----

RC_TEST(math, floor)
{
    RC_CHECK(rc_floor_f32(1.5f), ==, 1.0f);
    RC_CHECK(rc_floor_f32(-1.5f), ==, -2.0f);
    RC_CHECK(rc_floor_f32(0.5f), ==, 0.0f);
    RC_CHECK(rc_floor_f32(-0.25f), ==, -1.0f);
    RC_CHECK(rc_floor_f32(2.0f), ==, 2.0f);
    RC_CHECK(rc_floor_f32(-2.0f), ==, -2.0f);
    RC_CHECK(rc_floor_f32(8388607.5f), ==, 8388607.0f);
    RC_CHECK(rc_bitcast_f32(rc_floor_f32(-0.0f)), ==, 0x80000000u);
    RC_CHECK(rc_floor_f64(1.5), ==, 1.0);
    RC_CHECK(rc_floor_f64(-1.5), ==, -2.0);
    RC_CHECK(rc_floor_f64(4503599627370495.5), ==, 4503599627370495.0);
    RC_CHECK(rc_bitcast_f64(rc_floor_f64(-0.0)), ==, 0x8000000000000000ull);
}


RC_TEST(math, ceil)
{
    RC_CHECK(rc_ceil_f32(1.5f), ==, 2.0f);
    RC_CHECK(rc_ceil_f32(-1.5f), ==, -1.0f);
    RC_CHECK(rc_ceil_f32(0.25f), ==, 1.0f);
    RC_CHECK(rc_ceil_f32(2.0f), ==, 2.0f);
    RC_CHECK(rc_ceil_f32(8388607.5f), ==, 8388608.0f);
    RC_CHECK(rc_ceil_f64(1.5), ==, 2.0);
    RC_CHECK(rc_ceil_f64(-1.5), ==, -1.0);
    RC_CHECK(rc_ceil_f64(4503599627370495.5), ==, 4503599627370496.0);

    // Rounding a small negative up gives -0, not +0
    RC_CHECK(rc_bitcast_f32(rc_ceil_f32(-0.25f)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f64(rc_ceil_f64(-0.25)), ==, 0x8000000000000000ull);
}


RC_TEST(math, trunc)
{
    RC_CHECK(rc_trunc_f32(1.7f), ==, 1.0f);
    RC_CHECK(rc_trunc_f32(-1.7f), ==, -1.0f);
    RC_CHECK(rc_trunc_f32(2.0f), ==, 2.0f);
    RC_CHECK(rc_trunc_f32(8388607.5f), ==, 8388607.0f);
    RC_CHECK(rc_trunc_f32(-8388607.5f), ==, -8388607.0f);
    RC_CHECK(rc_trunc_f64(1.7), ==, 1.0);
    RC_CHECK(rc_trunc_f64(-1.7), ==, -1.0);
    RC_CHECK(rc_bitcast_f32(rc_trunc_f32(-0.5f)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f64(rc_trunc_f64(-0.5)), ==, 0x8000000000000000ull);
}


RC_TEST(math, round_even)
{
    RC_CHECK(rc_round_even_f32(0.4f), ==, 0.0f);
    RC_CHECK(rc_round_even_f32(0.6f), ==, 1.0f);
    RC_CHECK(rc_round_even_f32(-0.6f), ==, -1.0f);
    RC_CHECK(rc_round_even_f32(0.49999997f), ==, 0.0f);
    RC_CHECK(rc_round_even_f32(8388609.0f), ==, 8388609.0f);

    // Halves go to the even neighbour, up or down
    RC_CHECK(rc_round_even_f32(0.5f), ==, 0.0f);
    RC_CHECK(rc_round_even_f32(1.5f), ==, 2.0f);
    RC_CHECK(rc_round_even_f32(2.5f), ==, 2.0f);
    RC_CHECK(rc_round_even_f32(3.5f), ==, 4.0f);
    RC_CHECK(rc_round_even_f32(-1.5f), ==, -2.0f);
    RC_CHECK(rc_round_even_f32(-2.5f), ==, -2.0f);
    RC_CHECK(rc_round_even_f32(8388607.5f), ==, 8388608.0f);
    RC_CHECK(rc_round_even_f64(0.5), ==, 0.0);
    RC_CHECK(rc_round_even_f64(1.5), ==, 2.0);
    RC_CHECK(rc_round_even_f64(-2.5), ==, -2.0);
    RC_CHECK(rc_round_even_f64(4503599627370495.5), ==, 4503599627370496.0);

    // Small negatives keep their sign
    RC_CHECK(rc_bitcast_f32(rc_round_even_f32(-0.5f)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f32(rc_round_even_f32(-0.4f)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f64(rc_round_even_f64(-0.5)), ==, 0x8000000000000000ull);
}


RC_TEST(math, rounding_infinity)
{
    float infinity_f32 = rc_bitcast_u32(0x7f800000u);
    double infinity_f64 = rc_bitcast_u64(0x7ff0000000000000ull);

    RC_CHECK(rc_floor_f32(infinity_f32), ==, infinity_f32);
    RC_CHECK(rc_ceil_f32(-infinity_f32), ==, -infinity_f32);
    RC_CHECK(rc_trunc_f32(infinity_f32), ==, infinity_f32);
    RC_CHECK(rc_round_even_f32(-infinity_f32), ==, -infinity_f32);
    RC_CHECK(rc_floor_f64(-infinity_f64), ==, -infinity_f64);
    RC_CHECK(rc_ceil_f64(infinity_f64), ==, infinity_f64);
    RC_CHECK(rc_trunc_f64(-infinity_f64), ==, -infinity_f64);
    RC_CHECK(rc_round_even_f64(infinity_f64), ==, infinity_f64);
}


// ---- classifying ----

RC_TEST(math, is_finite_f32)
{
    RC_CHECK_TRUE(rc_is_finite_f32(0.0f));
    RC_CHECK_TRUE(rc_is_finite_f32(-0.0f));
    RC_CHECK_TRUE(rc_is_finite_f32(1.0f));
    RC_CHECK_TRUE(rc_is_finite_f32(rc_bitcast_u32(0x7f7fffffu)));
    RC_CHECK_TRUE(rc_is_finite_f32(rc_bitcast_u32(0xff7fffffu)));
    RC_CHECK_TRUE(rc_is_finite_f32(rc_bitcast_u32(0x00000001u)));

    // Infinities, then quiet and signalling NaNs
    RC_CHECK_FALSE(rc_is_finite_f32(rc_bitcast_u32(0x7f800000u)));
    RC_CHECK_FALSE(rc_is_finite_f32(rc_bitcast_u32(0xff800000u)));
    RC_CHECK_FALSE(rc_is_finite_f32(rc_bitcast_u32(0x7fc00000u)));
    RC_CHECK_FALSE(rc_is_finite_f32(rc_bitcast_u32(0xffc00000u)));
    RC_CHECK_FALSE(rc_is_finite_f32(rc_bitcast_u32(0x7f800001u)));
}


RC_TEST(math, is_finite_f64)
{
    RC_CHECK_TRUE(rc_is_finite_f64(0.0));
    RC_CHECK_TRUE(rc_is_finite_f64(-0.0));
    RC_CHECK_TRUE(rc_is_finite_f64(1.0));
    RC_CHECK_TRUE(rc_is_finite_f64(rc_bitcast_u64(0x7fefffffffffffffull)));
    RC_CHECK_TRUE(rc_is_finite_f64(rc_bitcast_u64(0xffefffffffffffffull)));
    RC_CHECK_TRUE(rc_is_finite_f64(rc_bitcast_u64(0x0000000000000001ull)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_bitcast_u64(0x7ff0000000000000ull)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_bitcast_u64(0xfff0000000000000ull)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_bitcast_u64(0x7ff8000000000000ull)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_bitcast_u64(0x7ff0000000000001ull)));
}


// ---- picking by type ----

RC_TEST(math, picking_float_types)
{
    float infinity_f32 = rc_bitcast_u32(0x7f800000u);
    double infinity_f64 = rc_bitcast_u64(0x7ff0000000000000ull);

    // The root of 2 differs between the widths, so each must have come from the right one
    RC_CHECK(rc_sqrt(2.0f), ==, rc_sqrt_f32(2.0f));
    RC_CHECK(rc_sqrt(2.0), ==, rc_sqrt_f64(2.0));
    RC_CHECK(rc_sqrt(2.0), !=, (double)rc_sqrt_f32(2.0f));

    // 2^52 - 0.5 has no float, so only the f64 forms see its half
    RC_CHECK(rc_floor(4503599627370495.5), ==, 4503599627370495.0);
    RC_CHECK(rc_ceil(4503599627370495.5), ==, 4503599627370496.0);
    RC_CHECK(rc_trunc(4503599627370495.5), ==, 4503599627370495.0);
    RC_CHECK(rc_round_even(4503599627370495.5), ==, 4503599627370496.0);
    RC_CHECK(rc_floor(-1.5f), ==, -2.0f);
    RC_CHECK(rc_ceil(-1.5f), ==, -1.0f);
    RC_CHECK(rc_trunc(-1.5f), ==, -1.0f);
    RC_CHECK(rc_round_even(2.5f), ==, 2.0f);
    RC_CHECK(rc_abs(-1.5f), ==, 1.5f);
    RC_CHECK(rc_abs(-1.5), ==, 1.5);
    RC_CHECK(rc_copysign(1.5f, -1.0f), ==, -1.5f);
    RC_CHECK(rc_copysign(1.5, -1.0), ==, -1.5);
    RC_CHECK_FALSE(rc_is_finite(infinity_f32));
    RC_CHECK_FALSE(rc_is_finite(infinity_f64));

    // A double too big for a float is still finite as a double
    RC_CHECK_TRUE(rc_is_finite(1e300));

    // Each result has the width of its argument
    RC_CHECK_TRUE(IS_TYPE(rc_sqrt(2.0f), float));
    RC_CHECK_TRUE(IS_TYPE(rc_sqrt(2.0), double));
    RC_CHECK_TRUE(IS_TYPE(rc_abs(1.0f), float));
    RC_CHECK_TRUE(IS_TYPE(rc_abs(1.0), double));
    RC_CHECK_TRUE(IS_TYPE(rc_floor(1.0f), float));
    RC_CHECK_TRUE(IS_TYPE(rc_floor(1.0), double));
    RC_CHECK_TRUE(IS_TYPE(rc_ceil(1.0f), float));
    RC_CHECK_TRUE(IS_TYPE(rc_ceil(1.0), double));
    RC_CHECK_TRUE(IS_TYPE(rc_trunc(1.0f), float));
    RC_CHECK_TRUE(IS_TYPE(rc_trunc(1.0), double));
    RC_CHECK_TRUE(IS_TYPE(rc_round_even(1.0f), float));
    RC_CHECK_TRUE(IS_TYPE(rc_round_even(1.0), double));
    RC_CHECK_TRUE(IS_TYPE(rc_copysign(1.0f, 1.0f), float));
    RC_CHECK_TRUE(IS_TYPE(rc_copysign(1.0, 1.0), double));
}

RC_TEST(math, sub_overflows_u64)
{
    RC_CHECK_FALSE(rc_sub_overflows_u64(0, 0));
    RC_CHECK_FALSE(rc_sub_overflows_u64(UINT64_MAX, UINT64_MAX));
    RC_CHECK_FALSE(rc_sub_overflows_u64(UINT64_MAX, 1));
    RC_CHECK_TRUE(rc_sub_overflows_u64(0, 1));
    RC_CHECK_TRUE(rc_sub_overflows_u64(UINT64_MAX - 1, UINT64_MAX));
}


RC_TEST(math, popcount_u64)
{
    RC_CHECK(rc_popcount_u64(0), ==, 0u);
    RC_CHECK(rc_popcount_u64(UINT64_MAX), ==, 64u);
    RC_CHECK(rc_popcount_u64(0x5555555555555555ull), ==, 32u);
    RC_CHECK(rc_popcount_u64(0x8000000100000001ull), ==, 3u);
}


RC_TEST(math, picking_bit_types)
{
    uint64_t high_bit = 1ull << 40;

    RC_CHECK(rc_clz(1u), ==, 31u);
    RC_CHECK(rc_clz(high_bit), ==, 23u);
    RC_CHECK(rc_ctz(0x10u), ==, 4u);
    RC_CHECK(rc_ctz(high_bit), ==, 40u);
    RC_CHECK(rc_popcount(UINT32_MAX), ==, 32u);
    RC_CHECK(rc_popcount(UINT64_MAX), ==, 64u);
    RC_CHECK(rc_bitcast(1.0f), ==, 0x3f800000u);
    RC_CHECK(rc_bitcast(1.0), ==, 0x3ff0000000000000ull);
    RC_CHECK_TRUE(rc_bitcast(0x3f800000u) == 1.0f);
    RC_CHECK_TRUE(rc_bitcast((uint64_t)0x3ff0000000000000ull) == 1.0);

    // Each picks the width of its argument
    RC_CHECK_TRUE(IS_TYPE(rc_bitcast(1.0f), uint32_t));
    RC_CHECK_TRUE(IS_TYPE(rc_bitcast(1.0), uint64_t));
    RC_CHECK_TRUE(IS_TYPE(rc_bitcast(1u), float));
    RC_CHECK_TRUE(IS_TYPE(rc_bitcast(high_bit), double));
}


RC_TEST(math, picking_overflow_types)
{
    int64_t max_i64 = INT64_MAX;
    uint64_t max_u64 = UINT64_MAX;
    uint64_t zero_u64 = 0;

    RC_CHECK_TRUE(rc_add_overflows(max_i64, INT64_C(1)));
    RC_CHECK_FALSE(rc_add_overflows(max_i64, INT64_C(0)));
    RC_CHECK_TRUE(rc_add_overflows(max_u64, UINT64_C(1)));
    RC_CHECK_TRUE(rc_sub_overflows(-max_i64, INT64_C(2)));
    RC_CHECK_TRUE(rc_sub_overflows(zero_u64, UINT64_C(1)));
    RC_CHECK_TRUE(rc_mul_overflows(max_i64, INT64_C(2)));
    RC_CHECK_FALSE(rc_mul_overflows(max_u64, UINT64_C(1)));

    // All bits set is -1 to the signed forms, so adding 1 fits
    RC_CHECK_FALSE(rc_add_overflows((int64_t)-1, INT64_C(1)));
    RC_CHECK_TRUE(rc_add_overflows(max_u64, UINT64_C(1)));
}
