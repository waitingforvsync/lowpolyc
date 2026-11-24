// math/pow_f32.c
//
// x^y for floats, as 2^(n / 32) with n = 32 y log2(x) in double, with the special cases of C17's
// Annex F. log2(x) comes from the log2 kernel in exp_log_f32.h, relative error 2^-36.3, and much
// less for x near 1, where y can be largest. Wherever the result is neither +inf nor +0
// (|n| < 32 x 160), that leaves n off by under 2^-24.0, and the result by 2^-29.5 relative. The
// 2^(n / 32) kernel adds 2^-33, so the error stays within 0.52 ulp, and every exactly
// representable result comes out exact. A finite y that isn't 0 and a positive normal x take the
// main path after two unsigned comparisons on the bits. A negative x needs a whole y, and gives
// |x|^y with the sign bit set when y is odd.

#include "richc/math/math.h"

#include "math/exp_log_f32.h"

// 2^(n / 32) for an n beyond the kernel's range, with sign as its sign bit: +-inf above it and +-0
// below.
static float rc_pow_f32_out_of_range(double n, uint64_t sign)
{
    uint32_t magnitude = n > 0.0 ? 0x7f800000u : 0;

    return rc_bitcast_u32(magnitude | (uint32_t)(sign >> 32));
}


// True when n is beyond the kernel's range, |n| < 32 x 1000, tested on the bits of n plus the
// shifter, which the kernel works out too. Below 2^51 in size those bits are the shifter's plus the
// nearest whole number to n, and beyond it they are far from them. On the integer side, the test
// leaves the floating point units to the main work.
static bool rc_pow_f32_is_out_of_range(double n)
{
    uint64_t shifted_bits = rc_bitcast_f64(n + RC_EXP2_F32_SHIFTER);

    return shifted_bits - (0x4338000000000000u - 32000u) >= 64000u;
}


// x^y for the bits of a positive normal x or a scaled subnormal, and a finite y, with sign as the
// sign bit of the result. Beyond |n| = 32 x 1000 the result is far beyond the floats. Inline, so
// that the main path and the negative x one each have their own copy and neither pays for a call.
static inline float rc_pow_f32_from_bits(uint32_t x_bits, float y, uint64_t sign)
{
    double n = (double)y * 32.0 * rc_log2_f32_kernel(x_bits);

    if (rc_pow_f32_is_out_of_range(n)) {
        return rc_pow_f32_out_of_range(n, sign);
    }

    return (float)rc_exp2_f32_kernel(n, sign);
}


// True for the bits of a finite y that isn't 0, the main path's y.
static bool rc_pow_f32_is_ordinary_y(uint32_t bits)
{
    return 2u * bits - 1u < 2u * 0x7f800000u - 1u;
}


// True when a whole y is odd. Every float from 2^24 up is even, so 2^24 stands in for them all.
static bool rc_pow_f32_is_odd_whole(float y)
{
    float below_even = rc_min_f32(rc_abs_f32(y), 0x1p24f);

    return ((uint32_t)below_even & 1u) != 0;
}


// True when y is an odd whole number.
static bool rc_pow_f32_is_odd(float y)
{
    return rc_trunc_f32(y) == y && rc_pow_f32_is_odd_whole(y);
}


// The sign bit a negative x gives x^y for a whole y: set for an odd y and clear for an even one.
static uint64_t rc_pow_f32_negative_sign(float y)
{
    return (uint64_t)rc_pow_f32_is_odd_whole(y) << 63;
}


// x^y for a y of +-inf: 1 for x = -1, +inf when |x|^y grows without limit, and +0 when it shrinks.
static float rc_pow_f32_infinite_y(float x, float y)
{
    float magnitude = rc_abs_f32(x);

    if (magnitude == 1.0f) {
        return 1.0f;
    }

    return (magnitude > 1.0f) == (y > 0.0f) ? rc_bitcast_u32(0x7f800000u) : 0.0f;
}


// x^y for x of +-0 or +-inf and a finite y that isn't 0: +0 or +inf, taking x's sign for odd y.
static float rc_pow_f32_zero_or_infinite_x(float x, float y)
{
    bool infinite = (x == 0.0f) == (y < 0.0f);
    float magnitude = infinite ? rc_bitcast_u32(0x7f800000u) : 0.0f;

    return rc_pow_f32_is_odd(y) ? rc_copysign_f32(magnitude, x) : magnitude;
}


// x^y for a subnormal x and a finite y that isn't 0, from |x|^y: a negative x needs a whole y,
// and gives a negative result for an odd one.
static float rc_pow_f32_subnormal_x(float x, float y)
{
    uint32_t magnitude_bits = rc_log_f32_subnormal_bits(rc_bitcast_f32(x) & 0x7fffffffu);

    if (x > 0.0f) {
        return rc_pow_f32_from_bits(magnitude_bits, y, 0);
    }

    if (rc_trunc_f32(y) != y) {
        return rc_bitcast_u32(0x7fc00000u);
    }

    return rc_pow_f32_from_bits(magnitude_bits, y, rc_pow_f32_negative_sign(y));
}


// x^y for an x of +-0, +-inf, NaN or a subnormal, and a finite y that isn't 0.
static float rc_pow_f32_rare_x(float x, float y)
{
    if (x != x) {
        return x + y;
    }

    if (x == 0.0f || !rc_is_finite_f32(x)) {
        return rc_pow_f32_zero_or_infinite_x(x, y);
    }

    return rc_pow_f32_subnormal_x(x, y);
}


// x^y for an x that isn't positive and normal, and a finite y that isn't 0. A negative normal x
// is the commonest, and goes straight to the work for |x| with the sign of the result.
static float rc_pow_f32_special_x(float x, float y)
{
    uint32_t magnitude_bits = rc_bitcast_f32(x) & 0x7fffffffu;

    if (rc_log_f32_is_special(magnitude_bits)) {
        return rc_pow_f32_rare_x(x, y);
    }

    if (rc_trunc_f32(y) != y) {
        return rc_bitcast_u32(0x7fc00000u);
    }

    return rc_pow_f32_from_bits(magnitude_bits, y, rc_pow_f32_negative_sign(y));
}


// x^y for a y of +-0, +-inf or NaN, in the order of Annex F's precedence.
static float rc_pow_f32_special_y(float x, float y)
{
    if (y == 0.0f) {
        return 1.0f;
    }

    if (x == 1.0f) {
        return 1.0f;
    }

    if (x != x || y != y) {
        return x + y;
    }

    return rc_pow_f32_infinite_y(x, y);
}


float rc_pow_f32(float x, float y)
{
    uint32_t x_bits = rc_bitcast_f32(x);

    if (!rc_pow_f32_is_ordinary_y(rc_bitcast_f32(y))) {
        return rc_pow_f32_special_y(x, y);
    }

    if (rc_log_f32_is_special(x_bits)) {
        return rc_pow_f32_special_x(x, y);
    }

    return rc_pow_f32_from_bits(x_bits, y, 0);
}
