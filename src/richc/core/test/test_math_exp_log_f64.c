// test_math_exp_log_f64.c
//
// Tests for rc_exp_f64, rc_log_f64, rc_log2_f64 and rc_pow_f64: a few values anyone can check, then
// the exact bits for the special values, each branch of the code and some ordinary arguments. The
// bits are musl's, which the ports match on every input tried, so a target that gives others has
// gone astray.

#include "richc/math/math.h"

#include "richc/test.h"

// The bits of exp(x).
static uint64_t exp_bits(double x)
{
    return rc_bitcast_f64(rc_exp_f64(x));
}


// The bits of log(x).
static uint64_t log_bits(double x)
{
    return rc_bitcast_f64(rc_log_f64(x));
}


// The bits of log2(x).
static uint64_t log2_bits(double x)
{
    return rc_bitcast_f64(rc_log2_f64(x));
}


// The bits of x^y.
static uint64_t pow_bits(double x, double y)
{
    return rc_bitcast_f64(rc_pow_f64(x, y));
}


// True if x is a NaN, which is neither finite nor equal to itself.
static bool is_nan(double x)
{
    return !rc_is_finite_f64(x) && x != x;
}


// ---- exp ----

RC_TEST(math_exp_f64, readable)
{
    double infinity = rc_bitcast_u64(0x7ff0000000000000ull);

    RC_CHECK(rc_exp_f64(0.0), ==, 1.0);
    RC_CHECK(rc_exp_f64(-0.0), ==, 1.0);
    RC_CHECK(rc_exp_f64(infinity), ==, infinity);
    RC_CHECK(rc_exp_f64(-infinity), ==, 0.0);
    RC_CHECK(rc_exp_f64(0x1.62e42fefa39efp-1), ==, 2.0);
    RC_CHECK(rc_exp_f64(1000.0), ==, infinity);
    RC_CHECK(rc_exp_f64(-1000.0), ==, 0.0);
}


RC_TEST(math_exp_f64, special)
{
    double infinity = rc_bitcast_u64(0x7ff0000000000000ull);
    double nan = rc_bitcast_u64(0x7ff8000000000000ull);

    RC_CHECK(exp_bits(0.0), ==, 0x3ff0000000000000ull);
    RC_CHECK(exp_bits(-0.0), ==, 0x3ff0000000000000ull);
    RC_CHECK(exp_bits(infinity), ==, 0x7ff0000000000000ull);
    RC_CHECK(exp_bits(-infinity), ==, 0x0000000000000000ull);
    RC_CHECK_TRUE(is_nan(rc_exp_f64(nan)));

    // Below 2^-54, 1 + x
    RC_CHECK(exp_bits(0x1p-1074), ==, 0x3ff0000000000000ull);
    RC_CHECK(exp_bits(-0x0.fffffffffffffp-1022), ==, 0x3ff0000000000000ull);
    RC_CHECK(exp_bits(0x1.fffffffffffffp-55), ==, 0x3ff0000000000000ull);
    RC_CHECK(exp_bits(-0x1.fffffffffffffp-55), ==, 0x3ff0000000000000ull);

    // From 1024 up, overflow or underflow
    RC_CHECK(exp_bits(1024.0), ==, 0x7ff0000000000000ull);
    RC_CHECK(exp_bits(0x1.fffffffffffffp+1023), ==, 0x7ff0000000000000ull);
    RC_CHECK(exp_bits(-1024.0), ==, 0x0000000000000000ull);
    RC_CHECK(exp_bits(-0x1.fffffffffffffp+1023), ==, 0x0000000000000000ull);
}


RC_TEST(math_exp_f64, golden)
{
    // From 2^-54, the reduction
    RC_CHECK(exp_bits(0x1p-54), ==, 0x3ff0000000000000ull);
    RC_CHECK(exp_bits(-0x1p-54), ==, 0x3ff0000000000000ull);
    RC_CHECK(exp_bits(0x1.b7cdfd9d7bdbbp-34), ==, 0x3ff000000006df38ull);
    RC_CHECK(exp_bits(0.5), ==, 0x3ffa61298e1e069cull);
    RC_CHECK(exp_bits(1.0), ==, 0x4005bf0a8b145769ull);
    RC_CHECK(exp_bits(-1.0), ==, 0x3fd78b56362cef38ull);
    RC_CHECK(exp_bits(0x1.62e42fefa39efp-1), ==, 0x4000000000000000ull);
    RC_CHECK(exp_bits(10.0), ==, 0x40d5829dcf950560ull);
    RC_CHECK(exp_bits(-10.0), ==, 0x3f07cd79b5647c9bull);
    RC_CHECK(exp_bits(100.0), ==, 0x48f3494a9b171bf5ull);
    RC_CHECK(exp_bits(-100.0), ==, 0x36ea8c1f14e2af5dull);

    // Below 512 the result is scaled directly, and from 512 with care
    RC_CHECK(exp_bits(0x1.fffffffffffffp+8), ==, 0x6e19476504ba839aull);
    RC_CHECK(exp_bits(512.0), ==, 0x6e19476504ba852eull);
    RC_CHECK(exp_bits(-0x1.fffffffffffffp+8), ==, 0x11c44109edb20a75ull);
    RC_CHECK(exp_bits(-512.0), ==, 0x11c44109edb20931ull);
    RC_CHECK(exp_bits(700.0), ==, 0x7f0d945df4f8ec8eull);

    // The largest finite result, and the first overflow
    RC_CHECK(exp_bits(0x1.62e42fefa39efp+9), ==, 0x7fefffffffffff2aull);
    RC_CHECK(exp_bits(0x1.62e42fefa39fp+9), ==, 0x7ff0000000000000ull);
    RC_CHECK(exp_bits(1000.0), ==, 0x7ff0000000000000ull);

    // Near the smallest normal, then the subnormals, and zero below the smallest of them
    RC_CHECK(exp_bits(-0x1.6232bdd7abcd2p+9), ==, 0x001000000000007cull);
    RC_CHECK(exp_bits(-720.0), ==, 0x0000000993b4dc95ull);
    RC_CHECK(exp_bits(-740.0), ==, 0x0000000000000055ull);
    RC_CHECK(exp_bits(-0x1.74385446d71c3p+9), ==, 0x0000000000000001ull);
    RC_CHECK(exp_bits(-0x1.74910d52d3051p+9), ==, 0x0000000000000001ull);
    RC_CHECK(exp_bits(-746.0), ==, 0x0000000000000000ull);
    RC_CHECK(exp_bits(-1000.0), ==, 0x0000000000000000ull);
}


// ---- log ----

RC_TEST(math_log_f64, readable)
{
    double infinity = rc_bitcast_u64(0x7ff0000000000000ull);

    RC_CHECK(rc_log_f64(1.0), ==, 0.0);
    RC_CHECK(rc_log_f64(0.0), ==, -infinity);
    RC_CHECK(rc_log_f64(infinity), ==, infinity);
    RC_CHECK(rc_log_f64(0x1.5bf0a8b145769p+1), ==, 1.0);
    RC_CHECK_TRUE(is_nan(rc_log_f64(-1.0)));
}


RC_TEST(math_log_f64, special)
{
    double infinity = rc_bitcast_u64(0x7ff0000000000000ull);
    double nan = rc_bitcast_u64(0x7ff8000000000000ull);

    RC_CHECK(log_bits(0.0), ==, 0xfff0000000000000ull);
    RC_CHECK(log_bits(-0.0), ==, 0xfff0000000000000ull);
    RC_CHECK(log_bits(infinity), ==, 0x7ff0000000000000ull);
    RC_CHECK_TRUE(is_nan(rc_log_f64(-infinity)));
    RC_CHECK_TRUE(is_nan(rc_log_f64(nan)));
    RC_CHECK_TRUE(is_nan(rc_log_f64(-1.0)));
    RC_CHECK_TRUE(is_nan(rc_log_f64(-0x1p-1074)));
}


RC_TEST(math_log_f64, golden)
{
    // Subnormals are normalised first
    RC_CHECK(log_bits(0x1p-1074), ==, 0xc0874385446d71c3ull);
    RC_CHECK(log_bits(0x0.fffffffffffffp-1022), ==, 0xc086232bdd7abcd2ull);

    // The smallest and largest normals, then ordinary values
    RC_CHECK(log_bits(0x1p-1022), ==, 0xc086232bdd7abcd2ull);
    RC_CHECK(log_bits(0x1.fffffffffffffp+1023), ==, 0x40862e42fefa39efull);
    RC_CHECK(log_bits(2.0), ==, 0x3fe62e42fefa39efull);
    RC_CHECK(log_bits(0.5), ==, 0xbfe62e42fefa39efull);
    RC_CHECK(log_bits(3.0), ==, 0x3ff193ea7aad030bull);
    RC_CHECK(log_bits(10.0), ==, 0x40026bb1bbb55516ull);
    RC_CHECK(log_bits(0x1.999999999999ap-4), ==, 0xc0026bb1bbb55515ull);
    RC_CHECK(log_bits(1e10), ==, 0x4037069e2aa2aa5bull);
    RC_CHECK(log_bits(0x1.7e43c8800759cp+996), ==, 0x4085963447f87fb5ull);
    RC_CHECK(log_bits(0x1.56e1fc2f8f359p-997), ==, 0xc085963447f87fb5ull);
    RC_CHECK(log_bits(0x1.5bf0a8b145769p+1), ==, 0x3ff0000000000000ull);
    RC_CHECK(log_bits(0x1.6p-1), ==, 0xbfd7fafa3bd8151cull);

    // [1 - 2^-4, 1 + 0x1.09p-4) has its own polynomial: both its ends, and the values beyond them
    RC_CHECK(log_bits(0x1.0000000000001p+0), ==, 0x3cafffffffffffffull);
    RC_CHECK(log_bits(0x1.fffffffffffffp-1), ==, 0xbca0000000000000ull);
    RC_CHECK(log_bits(1.0009765625), ==, 0x3f4ffc00aa8ab110ull);
    RC_CHECK(log_bits(0.9990234375), ==, 0xbf50020055655889ull);
    RC_CHECK(log_bits(0x1.ep-1), ==, 0xbfb08598b59e3a07ull);
    RC_CHECK(log_bits(0x1.dffffffffffffp-1), ==, 0xbfb08598b59e3a0full);
    RC_CHECK(log_bits(0x1.108ffffffffffp+0), ==, 0x3fb00c7c14184eb6ull);
    RC_CHECK(log_bits(0x1.109p+0), ==, 0x3fb00c7c14184ec5ull);
}


// ---- log2 ----

RC_TEST(math_log2_f64, readable)
{
    double infinity = rc_bitcast_u64(0x7ff0000000000000ull);

    RC_CHECK(rc_log2_f64(1.0), ==, 0.0);
    RC_CHECK(rc_log2_f64(2.0), ==, 1.0);
    RC_CHECK(rc_log2_f64(1024.0), ==, 10.0);
    RC_CHECK(rc_log2_f64(0.5), ==, -1.0);
    RC_CHECK(rc_log2_f64(0x1p-1074), ==, -1074.0);
    RC_CHECK(rc_log2_f64(0.0), ==, -infinity);
    RC_CHECK_TRUE(is_nan(rc_log2_f64(-1.0)));
}


RC_TEST(math_log2_f64, special)
{
    double infinity = rc_bitcast_u64(0x7ff0000000000000ull);
    double nan = rc_bitcast_u64(0x7ff8000000000000ull);

    RC_CHECK(log2_bits(0.0), ==, 0xfff0000000000000ull);
    RC_CHECK(log2_bits(-0.0), ==, 0xfff0000000000000ull);
    RC_CHECK(log2_bits(infinity), ==, 0x7ff0000000000000ull);
    RC_CHECK_TRUE(is_nan(rc_log2_f64(-infinity)));
    RC_CHECK_TRUE(is_nan(rc_log2_f64(nan)));
    RC_CHECK_TRUE(is_nan(rc_log2_f64(-1.0)));
    RC_CHECK_TRUE(is_nan(rc_log2_f64(-0x1.fffffffffffffp+1023)));
}


RC_TEST(math_log2_f64, golden)
{
    // Subnormals are normalised first
    RC_CHECK(log2_bits(0x1p-1074), ==, 0xc090c80000000000ull);
    RC_CHECK(log2_bits(0x0.fffffffffffffp-1022), ==, 0xc08ff00000000000ull);

    // The smallest and largest normals, exact powers of two, then ordinary values
    RC_CHECK(log2_bits(0x1p-1022), ==, 0xc08ff00000000000ull);
    RC_CHECK(log2_bits(0x1.fffffffffffffp+1023), ==, 0x4090000000000000ull);
    RC_CHECK(log2_bits(2.0), ==, 0x3ff0000000000000ull);
    RC_CHECK(log2_bits(1024.0), ==, 0x4024000000000000ull);
    RC_CHECK(log2_bits(0.5), ==, 0xbff0000000000000ull);
    RC_CHECK(log2_bits(0x1p1023), ==, 0x408ff80000000000ull);
    RC_CHECK(log2_bits(3.0), ==, 0x3ff95c01a39fbd68ull);
    RC_CHECK(log2_bits(10.0), ==, 0x400a934f0979a371ull);
    RC_CHECK(log2_bits(0x1.999999999999ap-4), ==, 0xc00a934f0979a371ull);
    RC_CHECK(log2_bits(0x1.7e43c8800759cp+996), ==, 0x408f24a09f1a8b89ull);
    RC_CHECK(log2_bits(0x1.6p-1), ==, 0xbfe14c560fe68af9ull);

    // [1 - 0x1.5b51p-5, 1 + 0x1.6ab2p-5) has its own polynomial: both its ends, and the values
    // beyond them
    RC_CHECK(log2_bits(0x1.0000000000001p+0), ==, 0x3cb71547652b82fdull);
    RC_CHECK(log2_bits(0x1.fffffffffffffp-1), ==, 0xbca71547652b82feull);
    RC_CHECK(log2_bits(1.0009765625), ==, 0x3f5712653743f454ull);
    RC_CHECK(log2_bits(0.9990234375), ==, 0xbf57182a894b69c6ull);
    RC_CHECK(log2_bits(0x1.ea4afp-1), ==, 0xbfb00007a842d2bdull);
    RC_CHECK(log2_bits(0x1.ea4aeffffffffp-1), ==, 0xbfb00007a842d2c9ull);
    RC_CHECK(log2_bits(0x1.0b558ffffffffp+0), ==, 0x3fb0000cb1ea2bccull);
    RC_CHECK(log2_bits(0x1.0b559p+0), ==, 0x3fb0000cb1ea2be2ull);
}


// ---- pow ----

RC_TEST(math_pow_f64, readable)
{
    double nan = rc_bitcast_u64(0x7ff8000000000000ull);

    RC_CHECK(rc_pow_f64(7.0, 0.0), ==, 1.0);
    RC_CHECK(rc_pow_f64(nan, 0.0), ==, 1.0);
    RC_CHECK(rc_pow_f64(1.0, nan), ==, 1.0);
    RC_CHECK(rc_pow_f64(2.0, 10.0), ==, 1024.0);
    RC_CHECK(rc_pow_f64(10.0, 2.0), ==, 100.0);
    RC_CHECK(rc_pow_f64(3.0, 3.0), ==, 27.0);
    RC_CHECK(rc_pow_f64(4.0, 0.5), ==, 2.0);
    RC_CHECK(rc_pow_f64(2.0, -1.0), ==, 0.5);
    RC_CHECK(rc_pow_f64(-2.0, 3.0), ==, -8.0);
    RC_CHECK(rc_pow_f64(-2.0, 2.0), ==, 4.0);
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-2.0, 0.5)));
}


RC_TEST(math_pow_f64, zero_one_infinity_nan)
{
    double infinity = rc_bitcast_u64(0x7ff0000000000000ull);
    double nan = rc_bitcast_u64(0x7ff8000000000000ull);

    // y = 0, or x = 1, is 1, even for NaN
    RC_CHECK(pow_bits(5.0, 0.0), ==, 0x3ff0000000000000ull);
    RC_CHECK(pow_bits(nan, 0.0), ==, 0x3ff0000000000000ull);
    RC_CHECK(pow_bits(infinity, -0.0), ==, 0x3ff0000000000000ull);
    RC_CHECK(pow_bits(0.0, 0.0), ==, 0x3ff0000000000000ull);
    RC_CHECK(pow_bits(1.0, nan), ==, 0x3ff0000000000000ull);
    RC_CHECK(pow_bits(1.0, infinity), ==, 0x3ff0000000000000ull);
    RC_CHECK(pow_bits(1.0, -0x1.d99999999999ap+1), ==, 0x3ff0000000000000ull);

    // A NaN otherwise spreads
    RC_CHECK_TRUE(is_nan(rc_pow_f64(nan, 2.0)));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(2.0, nan)));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-1.0, nan)));

    // y infinite
    RC_CHECK(pow_bits(-1.0, infinity), ==, 0x3ff0000000000000ull);
    RC_CHECK(pow_bits(-1.0, -infinity), ==, 0x3ff0000000000000ull);
    RC_CHECK(pow_bits(0.5, infinity), ==, 0x0000000000000000ull);
    RC_CHECK(pow_bits(2.0, infinity), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(0.5, -infinity), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(2.0, -infinity), ==, 0x0000000000000000ull);
    RC_CHECK(pow_bits(-0.5, infinity), ==, 0x0000000000000000ull);
    RC_CHECK(pow_bits(-2.0, infinity), ==, 0x7ff0000000000000ull);

    // x zero: the sign stays for an odd y
    RC_CHECK(pow_bits(0.0, 3.0), ==, 0x0000000000000000ull);
    RC_CHECK(pow_bits(-0.0, 3.0), ==, 0x8000000000000000ull);
    RC_CHECK(pow_bits(-0.0, 2.0), ==, 0x0000000000000000ull);
    RC_CHECK(pow_bits(-0.0, 0.5), ==, 0x0000000000000000ull);
    RC_CHECK(pow_bits(0.0, -1.0), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(-0.0, -1.0), ==, 0xfff0000000000000ull);
    RC_CHECK(pow_bits(-0.0, -2.0), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(0.0, -infinity), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(0.0, infinity), ==, 0x0000000000000000ull);

    // x infinite
    RC_CHECK(pow_bits(infinity, 2.0), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(infinity, -2.0), ==, 0x0000000000000000ull);
    RC_CHECK(pow_bits(-infinity, 3.0), ==, 0xfff0000000000000ull);
    RC_CHECK(pow_bits(-infinity, -3.0), ==, 0x8000000000000000ull);
    RC_CHECK(pow_bits(-infinity, 2.0), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(-infinity, -2.0), ==, 0x0000000000000000ull);
    RC_CHECK(pow_bits(-infinity, 0.5), ==, 0x7ff0000000000000ull);
}


RC_TEST(math_pow_f64, negative_x)
{
    // A whole y: odd keeps the sign and even drops it, however large y is
    RC_CHECK(pow_bits(-2.0, 3.0), ==, 0xc020000000000000ull);
    RC_CHECK(pow_bits(-2.0, 2.0), ==, 0x4010000000000000ull);
    RC_CHECK(pow_bits(-2.0, -3.0), ==, 0xbfc0000000000000ull);
    RC_CHECK(pow_bits(-3.0, 0x1p53), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(-1.5, 0x1.0000000000001p+52), ==, 0xfff0000000000000ull);

    // Not a whole y
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-2.0, 0.5)));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-8.0, 0x1.5555555555555p-2)));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-0x1.fffffffffffffp+1023, 0.5)));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-2.0, 0x1p-70)));
}


RC_TEST(math_pow_f64, overflow_underflow)
{
    RC_CHECK(pow_bits(2.0, 1023.0), ==, 0x7fe0000000000000ull);
    RC_CHECK(pow_bits(2.0, 1024.0), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(-2.0, 1025.0), ==, 0xfff0000000000000ull);
    RC_CHECK(pow_bits(10.0, 400.0), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(10.0, -400.0), ==, 0x0000000000000000ull);

    // Results in the subnormals, rounded once
    RC_CHECK(pow_bits(2.0, -1022.0), ==, 0x0010000000000000ull);
    RC_CHECK(pow_bits(2.0, -1060.5), ==, 0x0000000000002d41ull);
    RC_CHECK(pow_bits(2.0, -1074.0), ==, 0x0000000000000001ull);
    RC_CHECK(pow_bits(2.0, -1075.0), ==, 0x0000000000000000ull);
    RC_CHECK(pow_bits(-2.0, -1073.0), ==, 0x8000000000000002ull);
    RC_CHECK(pow_bits(-2.0, -1075.0), ==, 0x8000000000000000ull);
    RC_CHECK(pow_bits(0.5, 1074.0), ==, 0x0000000000000001ull);

    // |y| from 2^63 overflows or underflows, and below 2^-65 gives 1 +- y
    RC_CHECK(pow_bits(0x1.0000000000001p+0, 0x1p63), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(0x1.fffffffffffffp-1, 0x1p63), ==, 0x0000000000000000ull);
    RC_CHECK(pow_bits(0x1.fffffffffffffp-1, -0x1p64), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(2.0, 0x1p63), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(-2.0, 0x1p63), ==, 0x7ff0000000000000ull);
    RC_CHECK(pow_bits(2.0, 0x1p-66), ==, 0x3ff0000000000000ull);
    RC_CHECK(pow_bits(0.5, 0x1p-66), ==, 0x3ff0000000000000ull);
    RC_CHECK(pow_bits(2.0, -0x1p-66), ==, 0x3ff0000000000000ull);

    // Subnormal x is normalised first
    RC_CHECK(pow_bits(0x1p-1074, 0.5), ==, 0x1e60000000000000ull);
    RC_CHECK(pow_bits(0x1p-1074, -0.5), ==, 0x6180000000000000ull);
    RC_CHECK(pow_bits(0x1p-1074, 1.0), ==, 0x0000000000000001ull);
    RC_CHECK(pow_bits(0x0.fffffffffffffp-1022, 2.0), ==, 0x0000000000000000ull);
    RC_CHECK(pow_bits(-0x1p-1074, 3.0), ==, 0x8000000000000000ull);
    RC_CHECK(pow_bits(0x0.8p-1022, -0.25), ==, 0x4feae89f995ad3adull);
}


RC_TEST(math_pow_f64, golden)
{
    RC_CHECK(pow_bits(2.0, 10.0), ==, 0x4090000000000000ull);
    RC_CHECK(pow_bits(2.0, -1.0), ==, 0x3fe0000000000000ull);
    RC_CHECK(pow_bits(4.0, 0.5), ==, 0x4000000000000000ull);
    RC_CHECK(pow_bits(10.0, 2.0), ==, 0x4059000000000000ull);
    RC_CHECK(pow_bits(3.0, 3.0), ==, 0x403b000000000000ull);
    RC_CHECK(pow_bits(2.0, 0.5), ==, 0x3ff6a09e667f3bcdull);
    RC_CHECK(pow_bits(1.5, 2.5), ==, 0x40060b9fd68a4554ull);
    RC_CHECK(pow_bits(0x1.d99999999999ap+1, -0x1.4cccccccccccdp+0), ==, 0x3fc75d30f9b44e56ull);
    RC_CHECK(pow_bits(0x1.4f8b588e368f1p-17, 0x1.3333333333333p-2), ==, 0x3fa030dc4ea03a73ull);
    RC_CHECK(pow_bits(0x1.edd2f1a9fbe77p+6, 0x1.f8f5c28f5c28fp+2), ==, 0x435c37e53d5e93d2ull);
    RC_CHECK(pow_bits(0x1.999999999999ap-4, 0x1.999999999999ap-4), ==, 0x3fe96b230bcdc434ull);
    RC_CHECK(pow_bits(0x1.5bf0a8b145769p+1, 3.0), ==, 0x403415e5bf6fb105ull);
    RC_CHECK(pow_bits(0x1.ff7ced916872bp-1, 1e5), ==, 0x36e940747bb85395ull);
    RC_CHECK(pow_bits(0x1.7e43c8800759cp+996, 0x1.051eb851eb852p+0), ==, 0x7f76c8e5ca239078ull);
}
