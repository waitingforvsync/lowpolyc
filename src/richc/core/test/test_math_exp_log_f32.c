// test_math_exp_log_f32.c
//
// Tests for rc_exp_f32, rc_log_f32, rc_log2_f32 and rc_pow_f32, bit for bit. The expected bits are
// what musl gives, which every target must reproduce exactly.

#include "richc/math/math.h"

#include "richc/test.h"

// The bits of e^x, for checking exactly.
static uint32_t exp_bits(float x)
{
    return rc_bitcast_f32(rc_exp_f32(x));
}


// The bits of log(x), for checking exactly.
static uint32_t log_bits(float x)
{
    return rc_bitcast_f32(rc_log_f32(x));
}


// The bits of log2(x), for checking exactly.
static uint32_t log2_bits(float x)
{
    return rc_bitcast_f32(rc_log2_f32(x));
}


// The bits of x^y, for checking exactly.
static uint32_t pow_bits(float x, float y)
{
    return rc_bitcast_f32(rc_pow_f32(x, y));
}


// True if x is a NaN, of either sign and any payload.
static bool is_nan(float x)
{
    return !rc_is_finite_f32(x) && x != x;
}


// ---- exp ----

RC_TEST(math_exp_f32, exact)
{
    RC_CHECK(rc_exp_f32(0.0f), ==, 1.0f);
    RC_CHECK(rc_exp_f32(-0.0f), ==, 1.0f);
    RC_CHECK(rc_exp_f32(rc_bitcast_u32(0x7f800000u)), ==, rc_bitcast_u32(0x7f800000u));
    RC_CHECK(exp_bits(rc_bitcast_u32(0xff800000u)), ==, 0x00000000u);
    RC_CHECK_TRUE(is_nan(rc_exp_f32(rc_bitcast_u32(0x7fc00000u))));
    RC_CHECK_TRUE(is_nan(rc_exp_f32(rc_bitcast_u32(0xffc00000u))));
}


RC_TEST(math_exp_f32, ordinary)
{
    RC_CHECK(exp_bits(1.0f), ==, 0x402df854u);
    RC_CHECK(exp_bits(-1.0f), ==, 0x3ebc5ab2u);
    RC_CHECK(exp_bits(0.5f), ==, 0x3fd3094cu);
    RC_CHECK(exp_bits(rc_bitcast_u32(0x3dcccccdu)), ==, 0x3f8d763eu);
    RC_CHECK(exp_bits(rc_bitcast_u32(0x3f317218u)), ==, 0x40000000u);
    RC_CHECK(exp_bits(3.0f), ==, 0x41a0af2eu);
    RC_CHECK(exp_bits(-3.5f), ==, 0x3cf76081u);
    RC_CHECK(exp_bits(10.0f), ==, 0x46ac14eeu);
    RC_CHECK(exp_bits(-10.0f), ==, 0x383e6bceu);
    RC_CHECK(exp_bits(20.0f), ==, 0x4de75844u);
    RC_CHECK(exp_bits(-87.0f), ==, 0x00b33687u);

    // Tiny arguments round to 1, from either side
    RC_CHECK(exp_bits(0x1p-30f), ==, 0x3f800000u);
    RC_CHECK(exp_bits(-0x1p-30f), ==, 0x3f800000u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0x00000001u)), ==, 0x3f800000u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0x80800000u)), ==, 0x3f800000u);
}


RC_TEST(math_exp_f32, large)
{
    // From 88 up, the slow path checks for overflow before the main path takes it
    RC_CHECK(exp_bits(88.0f), ==, 0x7ef882b7u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0x42afffffu)), ==, 0x7ef8823bu);
    RC_CHECK(exp_bits(rc_bitcast_u32(0x42b17217u)), ==, 0x7f7fff84u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0x42b17218u)), ==, 0x7f800000u);
    RC_CHECK(exp_bits(100.0f), ==, 0x7f800000u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0x7f7fffffu)), ==, 0x7f800000u);

    // Below about -87.3 the result is subnormal, and below log(2^-150) it underflows to zero
    RC_CHECK(exp_bits(-88.0f), ==, 0x0041edc4u);
    RC_CHECK(exp_bits(-100.0f), ==, 0x0000001bu);
    RC_CHECK(exp_bits(rc_bitcast_u32(0xc2cff1b4u)), ==, 0x00000001u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0xc2cff1b5u)), ==, 0x00000000u);
    RC_CHECK(exp_bits(-104.0f), ==, 0x00000000u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0xff7fffffu)), ==, 0x00000000u);
}


// ---- log ----

RC_TEST(math_log_f32, exact)
{
    RC_CHECK(rc_log_f32(1.0f), ==, 0.0f);
    RC_CHECK(log_bits(1.0f), ==, 0x00000000u);
    RC_CHECK(log_bits(0.0f), ==, 0xff800000u);
    RC_CHECK(log_bits(-0.0f), ==, 0xff800000u);
    RC_CHECK(log_bits(rc_bitcast_u32(0x7f800000u)), ==, 0x7f800000u);

    // Below zero there's no real logarithm
    RC_CHECK_TRUE(is_nan(rc_log_f32(-1.0f)));
    RC_CHECK_TRUE(is_nan(rc_log_f32(rc_bitcast_u32(0x80000001u))));
    RC_CHECK_TRUE(is_nan(rc_log_f32(rc_bitcast_u32(0xff800000u))));
    RC_CHECK_TRUE(is_nan(rc_log_f32(rc_bitcast_u32(0x7fc00000u))));
    RC_CHECK_TRUE(is_nan(rc_log_f32(rc_bitcast_u32(0xffc00000u))));
}


RC_TEST(math_log_f32, ordinary)
{
    RC_CHECK(log_bits(2.0f), ==, 0x3f317218u);
    RC_CHECK(log_bits(0.5f), ==, 0xbf317218u);
    RC_CHECK(log_bits(10.0f), ==, 0x40135d8eu);
    RC_CHECK(log_bits(rc_bitcast_u32(0x402df854u)), ==, 0x3f7fffffu);
    RC_CHECK(log_bits(1.5f), ==, 0x3ecf991fu);
    RC_CHECK(log_bits(0.75f), ==, 0xbe934b11u);
    RC_CHECK(log_bits(100.0f), ==, 0x40935d8eu);
    RC_CHECK(log_bits(rc_bitcast_u32(0x2edbe6ffu)), ==, 0xc1b834f1u);
    RC_CHECK(log_bits(rc_bitcast_u32(0x501502f9u)), ==, 0x41b834f1u);

    // Either side of 1, and either side of 0x1.66p-1, where the table wraps around
    RC_CHECK(log_bits(rc_bitcast_u32(0x3f800001u)), ==, 0x33ffffffu);
    RC_CHECK(log_bits(rc_bitcast_u32(0x3f7fffffu)), ==, 0xb3800000u);
    RC_CHECK(log_bits(rc_bitcast_u32(0x3f330000u)), ==, 0xbeb73077u);
    RC_CHECK(log_bits(rc_bitcast_u32(0x3f32ffffu)), ==, 0xbeb7307au);
}


RC_TEST(math_log_f32, extremes)
{
    RC_CHECK(log_bits(rc_bitcast_u32(0x7f7fffffu)), ==, 0x42b17218u);
    RC_CHECK(log_bits(rc_bitcast_u32(0x00800000u)), ==, 0xc2aeac50u);

    // Subnormals are scaled up to normals first
    RC_CHECK(log_bits(rc_bitcast_u32(0x007fffffu)), ==, 0xc2aeac50u);
    RC_CHECK(log_bits(rc_bitcast_u32(0x00012345u)), ==, 0xc2b81e77u);
    RC_CHECK(log_bits(rc_bitcast_u32(0x00000001u)), ==, 0xc2ce8ed0u);
}


// ---- log2 ----

RC_TEST(math_log2_f32, exact)
{
    RC_CHECK(rc_log2_f32(1.0f), ==, 0.0f);
    RC_CHECK(log2_bits(1.0f), ==, 0x00000000u);
    RC_CHECK(log2_bits(0.0f), ==, 0xff800000u);
    RC_CHECK(log2_bits(-0.0f), ==, 0xff800000u);
    RC_CHECK(log2_bits(rc_bitcast_u32(0x7f800000u)), ==, 0x7f800000u);

    // Powers of 2 have whole logarithms, subnormal ones included
    RC_CHECK(rc_log2_f32(2.0f), ==, 1.0f);
    RC_CHECK(rc_log2_f32(8.0f), ==, 3.0f);
    RC_CHECK(rc_log2_f32(0.25f), ==, -2.0f);
    RC_CHECK(rc_log2_f32(1024.0f), ==, 10.0f);
    RC_CHECK(rc_log2_f32(0x1p127f), ==, 127.0f);
    RC_CHECK(rc_log2_f32(0x1p-126f), ==, -126.0f);
    RC_CHECK(rc_log2_f32(0x1p-149f), ==, -149.0f);

    // Below zero there's no real logarithm
    RC_CHECK_TRUE(is_nan(rc_log2_f32(-1.0f)));
    RC_CHECK_TRUE(is_nan(rc_log2_f32(rc_bitcast_u32(0x80000001u))));
    RC_CHECK_TRUE(is_nan(rc_log2_f32(rc_bitcast_u32(0xff800000u))));
    RC_CHECK_TRUE(is_nan(rc_log2_f32(rc_bitcast_u32(0x7fc00000u))));
}


RC_TEST(math_log2_f32, ordinary)
{
    RC_CHECK(log2_bits(3.0f), ==, 0x3fcae00du);
    RC_CHECK(log2_bits(10.0f), ==, 0x40549a78u);
    RC_CHECK(log2_bits(1.5f), ==, 0x3f15c01au);
    RC_CHECK(log2_bits(0.75f), ==, 0xbed47fccu);
    RC_CHECK(log2_bits(rc_bitcast_u32(0x402df854u)), ==, 0x3fb8aa3bu);
    RC_CHECK(log2_bits(rc_bitcast_u32(0x3dcccccdu)), ==, 0xc0549a78u);
    RC_CHECK(log2_bits(rc_bitcast_u32(0x2edbe6ffu)), ==, 0xc204e08bu);
    RC_CHECK(log2_bits(rc_bitcast_u32(0x501502f9u)), ==, 0x4204e08bu);

    // Either side of 1, and either side of 0x1.66p-1, where the table wraps around
    RC_CHECK(log2_bits(rc_bitcast_u32(0x3f800001u)), ==, 0x3438aa3au);
    RC_CHECK(log2_bits(rc_bitcast_u32(0x3f7fffffu)), ==, 0xb3b8aa3bu);
    RC_CHECK(log2_bits(rc_bitcast_u32(0x3f330000u)), ==, 0xbf0424a6u);
    RC_CHECK(log2_bits(rc_bitcast_u32(0x3f32ffffu)), ==, 0xbf0424a8u);

    // The extremes, subnormals included
    RC_CHECK(log2_bits(rc_bitcast_u32(0x7f7fffffu)), ==, 0x43000000u);
    RC_CHECK(log2_bits(rc_bitcast_u32(0x007fffffu)), ==, 0xc2fc0000u);
    RC_CHECK(log2_bits(rc_bitcast_u32(0x00012345u)), ==, 0xc304d054u);
}


// ---- pow ----

RC_TEST(math_pow_f32, zero_power)
{
    float infinity = rc_bitcast_u32(0x7f800000u);
    float nan = rc_bitcast_u32(0x7fc00000u);

    // x^0 is 1 for every x, even NaN
    RC_CHECK(rc_pow_f32(2.5f, 0.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(2.5f, -0.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(0.0f, 0.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(-0.0f, -0.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(infinity, 0.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(-infinity, -0.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(nan, 0.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(-7.0f, 0.0f), ==, 1.0f);
}


RC_TEST(math_pow_f32, one)
{
    float infinity = rc_bitcast_u32(0x7f800000u);
    float nan = rc_bitcast_u32(0x7fc00000u);

    // 1^y is 1 for every y, even NaN, and -1 to an infinite power is 1 too
    RC_CHECK(rc_pow_f32(1.0f, 2.5f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(1.0f, infinity), ==, 1.0f);
    RC_CHECK(rc_pow_f32(1.0f, -infinity), ==, 1.0f);
    RC_CHECK(rc_pow_f32(1.0f, nan), ==, 1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, infinity), ==, 1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, -infinity), ==, 1.0f);

    // x^1 is x
    RC_CHECK(rc_pow_f32(3.0f, 1.0f), ==, 3.0f);
    RC_CHECK(rc_pow_f32(-3.0f, 1.0f), ==, -3.0f);
    RC_CHECK(rc_pow_f32(0x1p-149f, 1.0f), ==, 0x1p-149f);
    RC_CHECK(rc_pow_f32(-0x1p-149f, 1.0f), ==, -0x1p-149f);
    RC_CHECK(pow_bits(-0.0f, 1.0f), ==, 0x80000000u);

    // -1 to an integer power is 1 or -1
    RC_CHECK(rc_pow_f32(-1.0f, 3.0f), ==, -1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, 4.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, 0x1p30f), ==, 1.0f);
}


RC_TEST(math_pow_f32, nan)
{
    float infinity = rc_bitcast_u32(0x7f800000u);
    float nan = rc_bitcast_u32(0x7fc00000u);

    RC_CHECK_TRUE(is_nan(rc_pow_f32(nan, 1.0f)));
    RC_CHECK_TRUE(is_nan(rc_pow_f32(nan, -2.0f)));
    RC_CHECK_TRUE(is_nan(rc_pow_f32(nan, infinity)));
    RC_CHECK_TRUE(is_nan(rc_pow_f32(2.0f, nan)));
    RC_CHECK_TRUE(is_nan(rc_pow_f32(0.0f, nan)));
    RC_CHECK_TRUE(is_nan(rc_pow_f32(-1.0f, nan)));
    RC_CHECK_TRUE(is_nan(rc_pow_f32(infinity, nan)));
}


RC_TEST(math_pow_f32, infinite_power)
{
    float infinity = rc_bitcast_u32(0x7f800000u);

    // Below 1 in size the power vanishes towards +infinity and grows towards -infinity, and above 1
    // the other way round
    RC_CHECK(pow_bits(0.5f, infinity), ==, 0x00000000u);
    RC_CHECK(pow_bits(-0.5f, infinity), ==, 0x00000000u);
    RC_CHECK(pow_bits(0.5f, -infinity), ==, 0x7f800000u);
    RC_CHECK(pow_bits(2.0f, infinity), ==, 0x7f800000u);
    RC_CHECK(pow_bits(-2.0f, infinity), ==, 0x7f800000u);
    RC_CHECK(pow_bits(2.0f, -infinity), ==, 0x00000000u);
    RC_CHECK(pow_bits(0.0f, infinity), ==, 0x00000000u);
    RC_CHECK(pow_bits(-0.0f, -infinity), ==, 0x7f800000u);
    RC_CHECK(pow_bits(infinity, infinity), ==, 0x7f800000u);
    RC_CHECK(pow_bits(-infinity, -infinity), ==, 0x00000000u);
}


RC_TEST(math_pow_f32, zero_and_infinite_base)
{
    float infinity = rc_bitcast_u32(0x7f800000u);

    // A zero base keeps its sign only for an odd integer power, and a negative power turns it into
    // an infinity
    RC_CHECK(pow_bits(0.0f, 3.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(-0.0f, 3.0f), ==, 0x80000000u);
    RC_CHECK(pow_bits(-0.0f, 2.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(-0.0f, 0.5f), ==, 0x00000000u);
    RC_CHECK(pow_bits(0.0f, -1.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(-0.0f, -1.0f), ==, 0xff800000u);
    RC_CHECK(pow_bits(-0.0f, -2.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(-0.0f, -0.5f), ==, 0x7f800000u);

    // An infinite base is the other way round
    RC_CHECK(pow_bits(infinity, 2.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(infinity, -2.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(-infinity, 3.0f), ==, 0xff800000u);
    RC_CHECK(pow_bits(-infinity, 2.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(-infinity, 0.5f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(-infinity, -3.0f), ==, 0x80000000u);
    RC_CHECK(pow_bits(-infinity, -2.0f), ==, 0x00000000u);
}


RC_TEST(math_pow_f32, negative_base)
{
    // Only an integer power of a negative number is real, and it's negative for an odd one
    RC_CHECK(rc_pow_f32(-2.0f, 3.0f), ==, -8.0f);
    RC_CHECK(rc_pow_f32(-2.0f, 2.0f), ==, 4.0f);
    RC_CHECK(rc_pow_f32(-2.0f, -3.0f), ==, -0.125f);
    RC_CHECK(pow_bits(-1.5f, 7.0f), ==, 0xc188b000u);
    RC_CHECK(pow_bits(-1.5f, -7.0f), ==, 0xbd6fbab4u);
    RC_CHECK_TRUE(is_nan(rc_pow_f32(-2.0f, 0.5f)));
    RC_CHECK_TRUE(is_nan(rc_pow_f32(-8.0f, 1.0f / 3.0f)));
    RC_CHECK_TRUE(is_nan(rc_pow_f32(-1.0f, rc_bitcast_u32(0x4affffffu))));

    // 2^23 + 1 is the largest odd float; from 2^24 up every float is even
    RC_CHECK(pow_bits(rc_bitcast_u32(0xbf800001u), rc_bitcast_u32(0x4b000001u)), ==, 0xc02df855u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0xbf7fffffu), rc_bitcast_u32(0x4b7fffffu)), ==, 0xbebc5ab2u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0xbf7fffffu), 0x1p24f), ==, 0x3ebc5ab1u);
    RC_CHECK(pow_bits(-2.0f, 0x1p24f), ==, 0x7f800000u);

    // Overflow and underflow keep the sign of an odd power
    RC_CHECK(pow_bits(-2.0f, 129.0f), ==, 0xff800000u);
    RC_CHECK(pow_bits(-2.0f, -151.0f), ==, 0x80000000u);
    RC_CHECK(pow_bits(-2.0f, -149.0f), ==, 0x80000001u);
}


RC_TEST(math_pow_f32, exact_powers)
{
    RC_CHECK(rc_pow_f32(2.0f, 10.0f), ==, 1024.0f);
    RC_CHECK(rc_pow_f32(2.0f, -10.0f), ==, 0x1p-10f);
    RC_CHECK(rc_pow_f32(3.0f, 2.0f), ==, 9.0f);
    RC_CHECK(rc_pow_f32(10.0f, 3.0f), ==, 1000.0f);
    RC_CHECK(rc_pow_f32(4.0f, 0.5f), ==, 2.0f);
    RC_CHECK(rc_pow_f32(0.25f, -0.5f), ==, 2.0f);
    RC_CHECK(rc_pow_f32(2.0f, 127.0f), ==, 0x1p127f);
}


RC_TEST(math_pow_f32, ordinary)
{
    RC_CHECK(pow_bits(2.0f, 0.5f), ==, 0x3fb504f3u);
    RC_CHECK(pow_bits(1.5f, 2.5f), ==, 0x40305cffu);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x3e99999au), rc_bitcast_u32(0x40f66666u)), ==, 0x38c573fau);
    RC_CHECK(pow_bits(10.0f, rc_bitcast_u32(0x3dcccccdu)), ==, 0x3fa12478u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x402df854u), rc_bitcast_u32(0x3fb504f3u)), ==, 0x40839fbfu);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x3f7ffffeu), 0x1p20f), ==, 0x3f61eb51u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x3f800001u), -0x1p22f), ==, 0x3f1b4598u);
}


RC_TEST(math_pow_f32, overflow_and_underflow)
{
    // 2^128 is just past the largest float, and 2^-150 just below half the smallest
    RC_CHECK(pow_bits(2.0f, 128.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(2.0f, -149.0f), ==, 0x00000001u);
    RC_CHECK(pow_bits(2.0f, -150.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(10.0f, 38.0f), ==, 0x7e967699u);
    RC_CHECK(pow_bits(10.0f, 39.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(10.0f, -45.0f), ==, 0x00000001u);
    RC_CHECK(pow_bits(10.0f, -46.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x7f7fffffu), 1.5f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x7f7fffffu), -1.5f), ==, 0x00000000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x501502f9u), 4.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(2.0f, rc_bitcast_u32(0x42fffffeu)), ==, 0x7f7fff4fu);
    RC_CHECK(pow_bits(2.0f, rc_bitcast_u32(0x42ffffffu)), ==, 0x7f7fffa7u);
}


RC_TEST(math_pow_f32, subnormal_base)
{
    // A subnormal base is scaled up to a normal one first
    RC_CHECK(pow_bits(0x1p-149f, 0.5f), ==, 0x1a3504f3u);
    RC_CHECK(pow_bits(0x1p-149f, -0.5f), ==, 0x64b504f3u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x007fffffu), 0.25f), ==, 0x2fb504f3u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x00012345u), -1.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x807fffffu), 3.0f), ==, 0x80000000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x807fffffu), -1.0f), ==, 0xfe800001u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x007fffffu), 2.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(0x1p-149f, -1.0f), ==, 0x7f800000u);
}
