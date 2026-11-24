// test_math_trig.c
//
// Tests for rc_sin, rc_cos and rc_tan: the special values, the symmetries, values anyone can check,
// and golden bit patterns. The golden inputs take every path: tiny numbers whose results need no
// work, small doubles that skip the reduction, ones near multiples of pi/2, the short reductions to
// their limit and the huge ones past them, with the hardest arguments to reduce of each width
// (tools/math/trig/hardest.py) and the largest finite numbers. Every golden result was checked
// against mpmath (tools/math/trig/golden.py): all but one are correctly rounded, so these bits are
// right as well as fixed. The odd one out, the tangent of the double below pi/4, is 0.55 ulp out.

#include "richc/math/math.h"

#include "richc/test.h"

// An input and its sine, cosine and tangent, as bits.
typedef struct trig_golden_f64 {
    uint64_t x;
    uint64_t sine;
    uint64_t cosine;
    uint64_t tangent;
} trig_golden_f64;

// The same for floats.
typedef struct trig_golden_f32 {
    uint32_t x;
    uint32_t sine;
    uint32_t cosine;
    uint32_t tangent;
} trig_golden_f32;

static const trig_golden_f64 trig_goldens_f64[] = {
    // Tiny, given at once: subnormal and tiny; then no reduction, up to just below pi/4
    {0x0000000000000001ull, 0x0000000000000001ull, 0x3ff0000000000000ull, 0x0000000000000001ull},
    {0x01a56e1fc2f8f359ull, 0x01a56e1fc2f8f359ull, 0x3ff0000000000000ull, 0x01a56e1fc2f8f359ull},
    {0x3e10000000000000ull, 0x3e10000000000000ull, 0x3ff0000000000000ull, 0x3e10000000000000ull},
    {0x3e45798ee2308c3aull, 0x3e45798ee2308c3aull, 0x3ff0000000000000ull, 0x3e45798ee2308c3aull},
    {0x3fb999999999999aull, 0x3fb98eaecb8bcb2cull, 0x3fefd712f9a817c1ull, 0x3fb9af8877430b80ull},
    {0x3fe0000000000000ull, 0x3fdeaee8744b05f0ull, 0x3fec1528065b7d50ull, 0x3fe17b4f5bf3474aull},
    {0x3fe921fb54442d17ull, 0x3fe6a09e667f3bccull, 0x3fe6a09e667f3bcdull, 0x3feffffffffffffeull},

    // The short reduction: pi/4 rounded down, ordinary angles and multiples of pi/2
    {0x3fe921fb54442d18ull, 0x3fe6a09e667f3bccull, 0x3fe6a09e667f3bcdull, 0x3fefffffffffffffull},
    {0x3ff0000000000000ull, 0x3feaed548f090ceeull, 0x3fe14a280fb5068cull, 0x3ff8eb245cbee3a6ull},
    {0x3ff921fb54442d18ull, 0x3ff0000000000000ull, 0x3c91a62633145c07ull, 0x434d02967c31cdb5ull},
    {0x4000000000000000ull, 0x3fed18f6ead1b446ull, 0xbfdaa22657537205ull, 0xc0017af62e0950f8ull},
    {0x400921fb54442d18ull, 0x3ca1a62633145c07ull, 0xbff0000000000000ull, 0xbca1a62633145c07ull},
    {0x4012d97c7f3321d2ull, 0xbff0000000000000ull, 0xbcaa79394c9e8a0aull, 0x4333570efd768923ull},
    {0x401921fb54442d18ull, 0xbcb1a62633145c07ull, 0x3ff0000000000000ull, 0xbcb1a62633145c07ull},
    {0x4024000000000000ull, 0xbfe1689ef5f34f52ull, 0xbfead9ac890c6b1full, 0x3fe4bf5f34be3782ull},
    {0x4059000000000000ull, 0xbfe03425b78c4db8ull, 0x3feb981dbf665fdfull, 0xbfe2ca74d62b5d38ull},

    // Its hardest argument, 45.55, 2^-60.5 from 29 pi/2, and that doubled; 1e5; and its last
    {0x4046c6cbc45dc8deull, 0x3ff0000000000000ull, 0xbc26d61b58c99c43ull, 0xc3b66b9ebc4850c6ull},
    {0x4056c6cbc45dc8deull, 0xbc36d61b58c99c43ull, 0xbff0000000000000ull, 0x3c36d61b58c99c43ull},
    {0x40f86a0000000000ull, 0x3fa24daa9c527e96ull, 0xbfeffac3841b3da7ull, 0xbfa250a9d503313dull},
    {0x412fffff80000000ull, 0x3fb633156c9d2a04ull, 0x3fefe12400c920a8ull, 0x3fb64892a87f593dull},

    // The huge reduction: its first; 1.4e7, 2^-59.0 from a multiple of pi/2; 1e22; two 2^-57.4
    // and 2^-58.9 away; 1e300; the hardest double (2^-60.9 away); one 2^-58.8 away; and DBL_MAX
    {0x4130000000000000ull, 0x3fd526ccb2fc8656ull, 0x3fee33ada92fe2aeull, 0x3fd6692e5779206full},
    {0x416b951f1572eba5ull, 0xbff0000000000000ull, 0xbc3f54f5227a4e84ull, 0x43a057584c429b3aull},
    {0x4480f0cf064dd592ull, 0xbfeb453ab76bf397ull, 0x3fe0be2cef01c8f4ull, 0xbffa0f79c1b6b257ull},
    {0x442782b7a20df6d4ull, 0xbff0000000000000ull, 0xbc57a6500ca6d83bull, 0x4385a63c82b6dae2ull},
    {0x482504cac51f1eafull, 0xbff0000000000000ull, 0xbc40cb604d34f341ull, 0x439e7c7d0f43f81eull},
    {0x7e37e43c8800759cull, 0xbfea2c16b010e385ull, 0xbfe2699022adc4c1ull, 0x3ff6be411f37ac77ull},
    {0x7506ac5b262ca1ffull, 0x3ff0000000000000ull, 0xbc214ae72e6ba22full, 0xc3bd9ba9a7975636ull},
    {0x7dee009c53148be1ull, 0x3ff0000000000000ull, 0x3c4295a3b0a64b1dull, 0x439b8cbf7ecfaffaull},
    {0x7fefffffffffffffull, 0x3f7452fc98b34e97ull, 0xbfefffe62ecfab75ull, 0xbf74530cfe729484ull}
};

static const trig_golden_f32 trig_goldens_f32[] = {
    // Tiny, given at once: subnormal and tiny; then small enough that the reduction leaves them as
    // they are, up to just below pi/4
    {0x00000001u, 0x00000001u, 0x3f800000u, 0x00000001u},
    {0x0da24260u, 0x0da24260u, 0x3f800000u, 0x0da24260u},
    {0x35800000u, 0x35800000u, 0x3f800000u, 0x35800000u},
    {0x3dcccccdu, 0x3dcc7577u, 0x3f7eb898u, 0x3dcd7c44u},
    {0x3f000000u, 0x3ef57744u, 0x3f60a940u, 0x3f0bda7bu},
    {0x3f490fdau, 0x3f3504f3u, 0x3f3504f4u, 0x3f7fffffu},

    // The short reduction: pi/4 rounded up, ordinary angles and multiples of pi/2
    {0x3f490fdbu, 0x3f3504f3u, 0x3f3504f3u, 0x3f800000u},
    {0x3f800000u, 0x3f576aa4u, 0x3f0a5140u, 0x3fc75923u},
    {0x3fc90fdbu, 0x3f800000u, 0xb33bbd2eu, 0xcbae8a4au},
    {0x40000000u, 0x3f68c7b7u, 0xbed51133u, 0xc00bd7b1u},
    {0x40490fdbu, 0xb3bbbd2eu, 0xbf800000u, 0x33bbbd2eu},
    {0x4096cbe4u, 0xbf800000u, 0x324cde2eu, 0xcc9ff26du},
    {0x41200000u, 0xbf0b44f8u, 0xbf56cd64u, 0x3f25fafau},
    {0x42c80000u, 0xbf01a12eu, 0x3f5cc0eeu, 0xbf1653a7u},

    // Its hardest argument, 252.9, 2^-27.8 from 161 pi/2; 1e5; and its last
    {0x437ce5f1u, 0x3f800000u, 0xb18fd1deu, 0xcd63d737u},
    {0x47c35000u, 0x3d126d55u, 0xbf7fd61cu, 0xbd12854fu},
    {0x497fffffu, 0x3e8ab32cu, 0x3f766d7au, 0x3e90166eu},

    // The huge reduction: its first, one 2^-26.3 from a multiple of pi/2, and the float below 2^27
    {0x49800000u, 0x3ea93666u, 0x3f719d6du, 0x3eb34973u},
    {0x4c2332e9u, 0xbf800000u, 0x32493de6u, 0xcca2d439u},
    {0x4cffffffu, 0xbf072942u, 0xbf59692bu, 0x3f1f26c9u},

    // Further on: 2^27, 1e9, arguments 2^-28.9, 2^-27.0 and 2^-27.2 from a multiple of pi/2, the
    // hardest float (2^-29.2 away), one 2^-26.5 away, and FLT_MAX
    {0x4d000000u, 0xbf436e65u, 0x3f255b29u, 0xbf9747e1u},
    {0x4e6e6b28u, 0x3f0bbc65u, 0x3f567fc6u, 0x3f26c592u},
    {0x50a3e87fu, 0x3f800000u, 0xb10a4ed8u, 0xcdeceba8u},
    {0x65898498u, 0x3f800000u, 0xb1fbf026u, 0xcd02104fu},
    {0x6a1976f1u, 0x3f800000u, 0xb1de3bdfu, 0xcd1372c2u},
    {0x6f79be45u, 0x3f800000u, 0xb0ddeea9u, 0xce13a60eu},
    {0x77584625u, 0xbf800000u, 0xb22fa975u, 0x4cba8a45u},
    {0x7f7fffffu, 0xbf0599b3u, 0x3f5a5f96u, 0xbf1c9ecau}
};

typedef double (*trig_fn_f64)(double);
typedef float (*trig_fn_f32)(float);


// The i-th of a spread of bit patterns: splitmix64's mix of i, so every exponent turns up.
static uint64_t trig_spread_bits(uint64_t i)
{
    uint64_t z = (i + 1) * 0x9e3779b97f4a7c15ull;

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}


// The i-th double of the spread, finite and positive, from every binade.
static double trig_spread_f64(uint32_t i)
{
    uint64_t bits = trig_spread_bits(i) & 0x7fffffffffffffffull;

    return rc_is_finite_f64(rc_bitcast_u64(bits)) ? rc_bitcast_u64(bits) : (double)i;
}


// The i-th float of the spread, finite and positive, from every binade.
static float trig_spread_f32(uint32_t i)
{
    uint32_t bits = (uint32_t)(trig_spread_bits(i) >> 33);

    return rc_is_finite_f32(rc_bitcast_u32(bits)) ? rc_bitcast_u32(bits) : (float)i;
}


// The i-th of a spread of ordinary angles, up to about 1160.
static double trig_angle_f64(uint32_t i)
{
    return (double)i * 0.29;
}


// True if f(-x) is exactly -f(x), sign bit and all, over the spread and the ordinary angles.
static bool trig_is_odd_f64(trig_fn_f64 f)
{
    for (uint32_t i = 0; i < 4000; i++) {
        double x = (i & 1) != 0 ? trig_spread_f64(i) : trig_angle_f64(i);

        if (rc_bitcast_f64(f(-x)) != (rc_bitcast_f64(f(x)) ^ 0x8000000000000000ull)) {
            return false;
        }
    }

    return true;
}


// True if f(-x) is exactly -f(x) for floats.
static bool trig_is_odd_f32(trig_fn_f32 f)
{
    for (uint32_t i = 0; i < 4000; i++) {
        float x = (i & 1) != 0 ? trig_spread_f32(i) : (float)trig_angle_f64(i);

        if (rc_bitcast_f32(f(-x)) != (rc_bitcast_f32(f(x)) ^ 0x80000000u)) {
            return false;
        }
    }

    return true;
}


// True if x is NaN, by the two tests that don't lean on its payload.
static bool trig_is_nan_f64(double x)
{
    return !rc_is_finite_f64(x) && x != x;
}


// True if x is NaN.
static bool trig_is_nan_f32(float x)
{
    return !rc_is_finite_f32(x) && x != x;
}


// The double k pi/2 comes to, which for k up to 201 is within 2^-44 of it.
static double trig_half_pi_times(uint32_t k)
{
    return (double)k * 1.5707963267948966;
}

// ---- sine ----

RC_TEST(math_sin, zeros_keep_their_sign)
{
    RC_CHECK(rc_bitcast_f64(rc_sin_f64(0.0)), ==, 0x0000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_sin_f64(-0.0)), ==, 0x8000000000000000ull);
    RC_CHECK(rc_bitcast_f32(rc_sin_f32(0.0f)), ==, 0x00000000u);
    RC_CHECK(rc_bitcast_f32(rc_sin_f32(-0.0f)), ==, 0x80000000u);
}


RC_TEST(math_sin, infinity_and_nan)
{
    RC_CHECK_TRUE(trig_is_nan_f64(rc_sin_f64(rc_bitcast_u64(0x7ff0000000000000ull))));
    RC_CHECK_TRUE(trig_is_nan_f64(rc_sin_f64(rc_bitcast_u64(0xfff0000000000000ull))));
    RC_CHECK_TRUE(trig_is_nan_f64(rc_sin_f64(rc_bitcast_u64(0x7ff8000000000000ull))));
    RC_CHECK_TRUE(trig_is_nan_f32(rc_sin_f32(rc_bitcast_u32(0x7f800000u))));
    RC_CHECK_TRUE(trig_is_nan_f32(rc_sin_f32(rc_bitcast_u32(0xff800000u))));
    RC_CHECK_TRUE(trig_is_nan_f32(rc_sin_f32(rc_bitcast_u32(0x7fc00000u))));
}


RC_TEST(math_sin, readable_values)
{
    RC_CHECK(rc_abs_f64(rc_sin_f64(0.5235987755982988) - 0.5), <, 1e-15);
    RC_CHECK(rc_abs_f64(rc_sin_f64(1.0) - 0.8414709848078965), <, 1e-15);
    RC_CHECK(rc_abs_f64(rc_sin_f64(-2.0) + 0.9092974268256817), <, 1e-15);
    RC_CHECK(rc_abs_f32(rc_sin_f32(0.52359878f) - 0.5f), <, 1e-7f);
    RC_CHECK(rc_abs_f32(rc_sin_f32(1.0f) - 0.84147098f), <, 1e-7f);

    // pi/2 rounds to a double 6e-17 short, whose sine is 1 to far more places than a double has
    RC_CHECK(rc_sin_f64(1.5707963267948966), ==, 1.0);
    RC_CHECK(rc_sin_f32(1.5707964f), ==, 1.0f);

    // A double's pi falls short of the real one by 1.2246e-16, which is therefore its sine
    RC_CHECK(rc_sin_f64(3.141592653589793), ==, 1.2246467991473532e-16);
}


RC_TEST(math_sin, small_arguments_are_themselves)
{
    // Below 2^-26 the cube is lost past the last bit, subnormals included
    RC_CHECK(rc_sin_f64(1e-10), ==, 1e-10);
    RC_CHECK(rc_sin_f64(-1e-300), ==, -1e-300);
    RC_CHECK(rc_sin_f64(rc_bitcast_u64(0x0000000000000001ull)), ==, rc_bitcast_u64(0x0000000000000001ull));
    RC_CHECK(rc_sin_f32(1e-5f), ==, 1e-5f);
    RC_CHECK(rc_sin_f32(rc_bitcast_u32(0x80000001u)), ==, rc_bitcast_u32(0x80000001u));
}


RC_TEST(math_sin, near_multiples_of_half_pi)
{
    // Near an odd multiple the sine rounds to +-1, and near an even one it is tiny
    for (uint32_t k = 1; k <= 200; k += 2) {
        double odd = trig_half_pi_times(k);
        double even = trig_half_pi_times(k + 1);

        RC_CHECK(rc_abs_f64(rc_sin_f64(odd)), ==, 1.0);
        RC_CHECK(rc_abs_f64(rc_sin_f64(even)), <, 0x1p-40);
        RC_CHECK(rc_abs_f32(rc_sin_f32((float)odd)), ==, 1.0f);
        RC_CHECK(rc_abs_f32(rc_sin_f32((float)even)), <, 0x1p-15f);
    }
}


RC_TEST(math_sin, odd)
{
    RC_CHECK_TRUE(trig_is_odd_f64(rc_sin_f64));
    RC_CHECK_TRUE(trig_is_odd_f32(rc_sin_f32));
}


RC_TEST(math_sin, golden_f64)
{
    for (uint32_t i = 0; i < RC_COUNT(trig_goldens_f64); i++) {
        trig_golden_f64 golden = trig_goldens_f64[i];

        RC_CHECK(rc_bitcast_f64(rc_sin_f64(rc_bitcast_u64(golden.x))), ==, golden.sine);
    }
}


RC_TEST(math_sin, golden_f32)
{
    for (uint32_t i = 0; i < RC_COUNT(trig_goldens_f32); i++) {
        trig_golden_f32 golden = trig_goldens_f32[i];

        RC_CHECK(rc_bitcast_f32(rc_sin_f32(rc_bitcast_u32(golden.x))), ==, golden.sine);
    }
}


RC_TEST(math_sin, picks_the_type)
{
    RC_CHECK(rc_sin(0.5f), ==, rc_sin_f32(0.5f));
    RC_CHECK(rc_sin(0.5), ==, rc_sin_f64(0.5));
}

// ---- cosine ----

RC_TEST(math_cos, zeros_give_one)
{
    RC_CHECK(rc_bitcast_f64(rc_cos_f64(0.0)), ==, 0x3ff0000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_cos_f64(-0.0)), ==, 0x3ff0000000000000ull);
    RC_CHECK(rc_bitcast_f32(rc_cos_f32(0.0f)), ==, 0x3f800000u);
    RC_CHECK(rc_bitcast_f32(rc_cos_f32(-0.0f)), ==, 0x3f800000u);
}


RC_TEST(math_cos, infinity_and_nan)
{
    RC_CHECK_TRUE(trig_is_nan_f64(rc_cos_f64(rc_bitcast_u64(0x7ff0000000000000ull))));
    RC_CHECK_TRUE(trig_is_nan_f64(rc_cos_f64(rc_bitcast_u64(0xfff0000000000000ull))));
    RC_CHECK_TRUE(trig_is_nan_f64(rc_cos_f64(rc_bitcast_u64(0x7ff8000000000000ull))));
    RC_CHECK_TRUE(trig_is_nan_f32(rc_cos_f32(rc_bitcast_u32(0x7f800000u))));
    RC_CHECK_TRUE(trig_is_nan_f32(rc_cos_f32(rc_bitcast_u32(0xff800000u))));
    RC_CHECK_TRUE(trig_is_nan_f32(rc_cos_f32(rc_bitcast_u32(0x7fc00000u))));
}


RC_TEST(math_cos, readable_values)
{
    RC_CHECK(rc_abs_f64(rc_cos_f64(1.0471975511965979) - 0.5), <, 1e-15);
    RC_CHECK(rc_abs_f64(rc_cos_f64(1.0) - 0.5403023058681398), <, 1e-15);
    RC_CHECK(rc_abs_f64(rc_cos_f64(4.0) + 0.6536436208636119), <, 1e-15);
    RC_CHECK(rc_abs_f32(rc_cos_f32(1.0471976f) - 0.5f), <, 1e-7f);
    RC_CHECK(rc_abs_f32(rc_cos_f32(1.0f) - 0.54030231f), <, 1e-7f);

    // pi and 2 pi are close enough that the cosine rounds to -1 and 1
    RC_CHECK(rc_cos_f64(3.141592653589793), ==, -1.0);
    RC_CHECK(rc_cos_f64(6.283185307179586), ==, 1.0);
    RC_CHECK(rc_cos_f32(3.14159265f), ==, -1.0f);

    // The cosine of pi/2's double is the 6.1232e-17 it falls short by
    RC_CHECK(rc_cos_f64(1.5707963267948966), ==, 6.123233995736766e-17);
}


RC_TEST(math_cos, small_arguments_give_one)
{
    RC_CHECK(rc_cos_f64(1e-9), ==, 1.0);
    RC_CHECK(rc_cos_f64(rc_bitcast_u64(0x0000000000000001ull)), ==, 1.0);
    RC_CHECK(rc_cos_f32(1e-4f), ==, 1.0f);
    RC_CHECK(rc_cos_f32(rc_bitcast_u32(0x00000001u)), ==, 1.0f);
}


RC_TEST(math_cos, near_multiples_of_half_pi)
{
    // Near an odd multiple the cosine is tiny, and near an even one it rounds to +-1
    for (uint32_t k = 1; k <= 200; k += 2) {
        double odd = trig_half_pi_times(k);
        double even = trig_half_pi_times(k + 1);

        RC_CHECK(rc_abs_f64(rc_cos_f64(odd)), <, 0x1p-40);
        RC_CHECK(rc_abs_f64(rc_cos_f64(even)), ==, 1.0);
        RC_CHECK(rc_abs_f32(rc_cos_f32((float)odd)), <, 0x1p-15f);
        RC_CHECK(rc_abs_f32(rc_cos_f32((float)even)), ==, 1.0f);
    }
}


RC_TEST(math_cos, even)
{
    for (uint32_t i = 0; i < 4000; i++) {
        double x = (i & 1) != 0 ? trig_spread_f64(i) : trig_angle_f64(i);
        float x_f32 = (i & 1) != 0 ? trig_spread_f32(i) : (float)trig_angle_f64(i);

        RC_CHECK(rc_bitcast_f64(rc_cos_f64(-x)), ==, rc_bitcast_f64(rc_cos_f64(x)));
        RC_CHECK(rc_bitcast_f32(rc_cos_f32(-x_f32)), ==, rc_bitcast_f32(rc_cos_f32(x_f32)));
    }
}


RC_TEST(math_cos, golden_f64)
{
    for (uint32_t i = 0; i < RC_COUNT(trig_goldens_f64); i++) {
        trig_golden_f64 golden = trig_goldens_f64[i];

        RC_CHECK(rc_bitcast_f64(rc_cos_f64(rc_bitcast_u64(golden.x))), ==, golden.cosine);
    }
}


RC_TEST(math_cos, golden_f32)
{
    for (uint32_t i = 0; i < RC_COUNT(trig_goldens_f32); i++) {
        trig_golden_f32 golden = trig_goldens_f32[i];

        RC_CHECK(rc_bitcast_f32(rc_cos_f32(rc_bitcast_u32(golden.x))), ==, golden.cosine);
    }
}


RC_TEST(math_cos, picks_the_type)
{
    RC_CHECK(rc_cos(0.5f), ==, rc_cos_f32(0.5f));
    RC_CHECK(rc_cos(0.5), ==, rc_cos_f64(0.5));
}

// ---- tangent ----

RC_TEST(math_tan, zeros_keep_their_sign)
{
    RC_CHECK(rc_bitcast_f64(rc_tan_f64(0.0)), ==, 0x0000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_tan_f64(-0.0)), ==, 0x8000000000000000ull);
    RC_CHECK(rc_bitcast_f32(rc_tan_f32(0.0f)), ==, 0x00000000u);
    RC_CHECK(rc_bitcast_f32(rc_tan_f32(-0.0f)), ==, 0x80000000u);
}


RC_TEST(math_tan, infinity_and_nan)
{
    RC_CHECK_TRUE(trig_is_nan_f64(rc_tan_f64(rc_bitcast_u64(0x7ff0000000000000ull))));
    RC_CHECK_TRUE(trig_is_nan_f64(rc_tan_f64(rc_bitcast_u64(0xfff0000000000000ull))));
    RC_CHECK_TRUE(trig_is_nan_f64(rc_tan_f64(rc_bitcast_u64(0x7ff8000000000000ull))));
    RC_CHECK_TRUE(trig_is_nan_f32(rc_tan_f32(rc_bitcast_u32(0x7f800000u))));
    RC_CHECK_TRUE(trig_is_nan_f32(rc_tan_f32(rc_bitcast_u32(0xff800000u))));
    RC_CHECK_TRUE(trig_is_nan_f32(rc_tan_f32(rc_bitcast_u32(0x7fc00000u))));
}


RC_TEST(math_tan, readable_values)
{
    RC_CHECK(rc_abs_f64(rc_tan_f64(0.4636476090008061) - 0.5), <, 1e-15);
    RC_CHECK(rc_abs_f64(rc_tan_f64(1.0) - 1.5574077246549023), <, 1e-15);
    RC_CHECK(rc_abs_f64(rc_tan_f64(2.0) + 2.185039863261519), <, 1e-15);
    RC_CHECK(rc_abs_f32(rc_tan_f32(1.0f) - 1.5574077f), <, 2e-7f);

    // pi/4's double is a hair short, so its tangent is the double just below 1
    RC_CHECK(rc_tan_f64(0.7853981633974483), ==, 1.0 - 0x1p-53);

    // Just short of pi/2 the tangent is huge, and just past it hugely negative
    RC_CHECK(rc_tan_f64(1.5707963267948966), ==, 1.633123935319537e16);
    RC_CHECK(rc_tan_f32(1.5707962f), ==, 13245402.0f);
    RC_CHECK(rc_tan_f32(1.5707964f), ==, -22877332.0f);
}


RC_TEST(math_tan, small_arguments_are_themselves)
{
    RC_CHECK(rc_tan_f64(1e-10), ==, 1e-10);
    RC_CHECK(rc_tan_f64(-1e-300), ==, -1e-300);
    RC_CHECK(rc_tan_f64(rc_bitcast_u64(0x0000000000000001ull)), ==, rc_bitcast_u64(0x0000000000000001ull));
    RC_CHECK(rc_tan_f32(1e-5f), ==, 1e-5f);
    RC_CHECK(rc_tan_f32(rc_bitcast_u32(0x80000001u)), ==, rc_bitcast_u32(0x80000001u));
}


RC_TEST(math_tan, near_multiples_of_half_pi)
{
    // Near an odd multiple the tangent is huge, and near an even one tiny
    for (uint32_t k = 1; k <= 200; k += 2) {
        double odd = trig_half_pi_times(k);
        double even = trig_half_pi_times(k + 1);

        RC_CHECK(rc_abs_f64(rc_tan_f64(odd)), >, 0x1p40);
        RC_CHECK(rc_abs_f64(rc_tan_f64(even)), <, 0x1p-40);
        RC_CHECK(rc_abs_f32(rc_tan_f32((float)odd)), >, 0x1p15f);
        RC_CHECK(rc_abs_f32(rc_tan_f32((float)even)), <, 0x1p-15f);
    }
}


RC_TEST(math_tan, odd)
{
    RC_CHECK_TRUE(trig_is_odd_f64(rc_tan_f64));
    RC_CHECK_TRUE(trig_is_odd_f32(rc_tan_f32));
}


RC_TEST(math_tan, golden_f64)
{
    for (uint32_t i = 0; i < RC_COUNT(trig_goldens_f64); i++) {
        trig_golden_f64 golden = trig_goldens_f64[i];

        RC_CHECK(rc_bitcast_f64(rc_tan_f64(rc_bitcast_u64(golden.x))), ==, golden.tangent);
    }
}


RC_TEST(math_tan, golden_f32)
{
    for (uint32_t i = 0; i < RC_COUNT(trig_goldens_f32); i++) {
        trig_golden_f32 golden = trig_goldens_f32[i];

        RC_CHECK(rc_bitcast_f32(rc_tan_f32(rc_bitcast_u32(golden.x))), ==, golden.tangent);
    }
}


RC_TEST(math_tan, picks_the_type)
{
    RC_CHECK(rc_tan(0.5f), ==, rc_tan_f32(0.5f));
    RC_CHECK(rc_tan(0.5), ==, rc_tan_f64(0.5));
}
