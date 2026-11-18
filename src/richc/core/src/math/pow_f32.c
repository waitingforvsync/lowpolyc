// math/pow_f32.c
//
// A float raised to a float power. Ported from musl 1.2.5 (MIT), src/math/powf.c, which is Arm's
// optimized-routines. Its notice:
//
// Copyright (c) 2017-2018, Arm Limited.
// SPDX-License-Identifier: MIT
//
// pow(x, y) is 2^(y log2(x)). log2(x) has a relative error of 1.83 x 2^-33 and the power of 2 one
// of 1.69 x 2^-34, which come to 1.27 x 2^-26 before rounding and at most 0.82 ulp after.

#include "exp_log_f32.h"

// Set in the shifted k of exp, it reaches the sign bit and makes the result negative.
#define RC_POW_SIGN_BIAS_F32 (1u << (RC_EXP_TABLE_BITS_F32 + 11))

// Whether y is an integer (NONE if not), and if so whether it is odd, for the sign of a negative x
// to the y.
typedef enum rc_pow_parity {
    RC_POW_PARITY_NONE,
    RC_POW_PARITY_ODD,
    RC_POW_PARITY_EVEN
} rc_pow_parity;


// log2(x) to more than float precision, for the bits of a normal x or a normalised subnormal one.
// With x = 2^k z and c the centre nearest z, log2(x) = log1p(z/c - 1) / ln2 + log2(c) + k, and
// the polynomial gives the first term.
static double rc_pow_log2_f32(uint32_t x_bits)
{
    rc_log_parts_f32 parts = rc_log_parts_of_f32(x_bits);
    rc_log_centre_f32 centre = rc_pow_data_f32.centres[parts.index];
    const double *poly = rc_pow_data_f32.poly;
    double r = parts.z * centre.inv_c - 1.0;
    double y0 = centre.log_c + (double)parts.exponent;
    double r2 = r * r;
    double high = poly[0] * r + poly[1];
    double middle = poly[2] * r + poly[3];
    double r4 = r2 * r2;
    double linear = poly[4] * r + y0;
    double low = middle * r2 + linear;

    return high * r4 + low;
}


// 2^x for x in [-150, 0x1.fffffffd1d571p+6], negative if sign_bias is set. x = k/32 + r, with k
// whole and r in [-1/64, 1/64]; adding the shift rounds x to k/32 and leaves k in the low bits.
static float rc_pow_exp2_f32(double x, uint32_t sign_bias)
{
    double shifted = x + rc_exp_data_f32.shift_scaled;
    double k_over_32 = shifted - rc_exp_data_f32.shift_scaled;

    return rc_exp_from_reduced_f32(rc_bitcast_f64(shifted), sign_bias, x - k_over_32, rc_exp_data_f32.poly);
}


// pow(x, y) for the bits of a positive normal x, or a normalised subnormal one, and a finite
// non-zero y. y log2(x) can't overflow a double, y being a float.
static float rc_pow_of_normal_f32(uint32_t x_bits, float y, uint32_t sign_bias)
{
    double y_log2_x = (double)y * rc_pow_log2_f32(x_bits);

    if (y_log2_x > 0x1.fffffffd1d571p+6) {
        return rc_exp_overflow_f32(sign_bias);
    }

    if (y_log2_x <= -150.0) {
        return rc_exp_underflow_f32(sign_bias);
    }

    return rc_pow_exp2_f32(y_log2_x, sign_bias);
}


// pow(|x|, y) for finite non-zero x and y, negative if sign_bias is set.
static float rc_pow_of_finite_f32(float x, float y, uint32_t sign_bias)
{
    uint32_t magnitude = rc_bitcast_f32(x) & 0x7fffffffu;

    if (magnitude < 0x00800000u) {
        return rc_pow_of_normal_f32(rc_log_normalise_subnormal_f32(x), y, sign_bias);
    }

    return rc_pow_of_normal_f32(magnitude, y, sign_bias);
}


// The parity of a y with a biased exponent in [0x7f, 0x7f + 23], given the bit of its mantissa
// worth 1.
static rc_pow_parity rc_pow_parity_by_units_f32(uint32_t y_bits, uint32_t units)
{
    if ((y_bits & (units - 1u)) != 0) {
        return RC_POW_PARITY_NONE;
    }

    if ((y_bits & units) != 0) {
        return RC_POW_PARITY_ODD;
    }

    return RC_POW_PARITY_EVEN;
}


// The parity of a finite non-zero y. Below 1 it can't be an integer, and from 2^24 up it must be
// an even one.
static rc_pow_parity rc_pow_parity_of_f32(uint32_t y_bits)
{
    uint32_t exponent = (y_bits >> 23) & 0xffu;

    if (exponent < 0x7fu) {
        return RC_POW_PARITY_NONE;
    }

    if (exponent > 0x7fu + 23u) {
        return RC_POW_PARITY_EVEN;
    }

    return rc_pow_parity_by_units_f32(y_bits, 1u << (0x7fu + 23u - exponent));
}


// True if the float with these bits is zero, infinite or NaN.
static bool rc_pow_is_zero_inf_nan_f32(uint32_t bits)
{
    return 2u * bits - 1u >= 2u * 0x7f800000u - 1u;
}


// pow(x, y) for y zero, infinite or NaN.
static float rc_pow_of_special_y_f32(float x, float y)
{
    uint32_t x_bits = rc_bitcast_f32(x);
    uint32_t y_bits = rc_bitcast_f32(y);
    uint32_t x_magnitude = x_bits & 0x7fffffffu;
    uint32_t y_magnitude = y_bits & 0x7fffffffu;
    bool x_below_one = x_magnitude < 0x3f800000u;
    bool y_positive = (y_bits & 0x80000000u) == 0;

    // x^0 is 1 even for a NaN x, and so is 1^y for a NaN y
    if (y_magnitude == 0) {
        return 1.0f;
    }

    if (x_bits == 0x3f800000u) {
        return 1.0f;
    }

    if (x_magnitude > 0x7f800000u) {
        return x + y;
    }

    if (y_magnitude > 0x7f800000u) {
        return x + y;
    }

    // y is infinite from here
    if (x_magnitude == 0x3f800000u) {
        return 1.0f;
    }

    if (x_below_one == y_positive) {
        return 0.0f;
    }

    return y * y;
}


// pow(x, y) for x zero, infinite or NaN, and y finite and non-zero.
static float rc_pow_of_special_x_f32(float x, uint32_t y_bits)
{
    float square = x * x;
    bool negate = (rc_bitcast_f32(x) & 0x80000000u) != 0 && rc_pow_parity_of_f32(y_bits) == RC_POW_PARITY_ODD;
    float power = negate ? -square : square;

    if ((y_bits & 0x80000000u) != 0) {
        return 1.0f / power;
    }

    return power;
}


// pow(x, y) for finite x below zero and finite non-zero y. Only an integer y has a real answer,
// negative if y is odd.
static float rc_pow_of_negative_x_f32(float x, float y)
{
    rc_pow_parity parity = rc_pow_parity_of_f32(rc_bitcast_f32(y));

    if (parity == RC_POW_PARITY_NONE) {
        return (x - x) / (x - x);
    }

    if (parity == RC_POW_PARITY_ODD) {
        return rc_pow_of_finite_f32(x, y, RC_POW_SIGN_BIAS_F32);
    }

    return rc_pow_of_finite_f32(x, y, 0);
}


float rc_pow_f32(float x, float y)
{
    uint32_t x_bits = rc_bitcast_f32(x);
    uint32_t y_bits = rc_bitcast_f32(y);

    if (rc_pow_is_zero_inf_nan_f32(y_bits)) {
        return rc_pow_of_special_y_f32(x, y);
    }

    if (rc_pow_is_zero_inf_nan_f32(x_bits)) {
        return rc_pow_of_special_x_f32(x, y_bits);
    }

    if ((x_bits & 0x80000000u) != 0) {
        return rc_pow_of_negative_x_f32(x, y);
    }

    return rc_pow_of_finite_f32(x, y, 0);
}
