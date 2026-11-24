// math/atan_f32.c
//
// rc_atan_f32: the arctangent of a float, worked in double.
//
// Up to tan(pi/8) in size, atan(x) = x (1 + x^2 Q(x^2)) for Q a quartic, fitted to 2^-30.5. That
// form is odd, -0 included, so atan(-x) is exactly -atan(x) there. Beyond, we work on t = |x| and
// give the result x's sign: up to tan(3pi/8), atan(t) = pi/4 + atan(u) with
// u = (t - 1) / (t + 1), and beyond that, pi/2 + atan(u) with u = -1/t. Either way |u| is at most
// tan(pi/8), but for a rounding, and u costs one rounding, as t - 1 and t + 1 are exact. From 2^26
// up, the result is the float nearest pi/2. Rounding the double result to float adds little to the
// half ulp the rounding itself costs: 0.51 ulp at worst over every float.
//
// The ranges branch, on |x|'s bits, which settles sooner than comparing the float: from 2^26 up
// first, where we make the result from the bits alone, then the widest first. Working out all three
// reductions and choosing costs more than the odd misprediction. Beyond tan(pi/8) the sign goes on
// as a product with 1 or -1.

#include "richc/math/math.h"

#include "math/arc.h"

// base + atan(u), for |u| <= tan(pi/8), but for a rounding.
static double rc_atan_from(double base, double u)
{
    return (base + u) + u * rc_arc_atan_ratio_f32(u);
}


// The bits of the float nearest pi/2.
#define RC_ATAN_PI_2_F32_BITS 0x3fc90fdbu


// Whether |x|, as its bits, is from 2^26 up to infinity, where pi/2 - 1/|x| rounds to the float
// nearest pi/2, as does pi/2's double: not NaN.
static bool rc_atan_is_huge(uint32_t magnitude)
{
    return magnitude - 0x4c800000u <= 0x7f800000u - 0x4c800000u;
}


// atan(x) for tan(3pi/8) < |x| < 2^26 and NaN, given d = x as a double and x's bits:
// pi/2 + atan(-1/|x|) with x's sign, and NaN for NaN.
static float rc_atan_beyond(double d, uint32_t bits)
{
    return (float)(rc_atan_from(RC_ARC_PI_2_HI, -1.0 / rc_abs_f64(d)) * rc_arc_sign_f64(bits >> 31));
}


// atan(x) for tan(pi/8) < |x| <= tan(3pi/8), given d = x as a double and x's bits: pi/4 + atan(u)
// for u = (t - 1) / (t + 1), t = |x|, with x's sign.
static float rc_atan_middle(double d, uint32_t bits)
{
    double t = rc_abs_f64(d);

    return (float)(rc_atan_from(RC_ARC_PI_4, (t - 1.0) / (t + 1.0)) * rc_arc_sign_f64(bits >> 31));
}


float rc_atan_f32(float x)
{
    uint32_t bits = rc_bitcast_f32(x);
    uint32_t magnitude = bits & 0x7fffffffu;
    double d = (double)x;

    if (rc_atan_is_huge(magnitude)) {
        return rc_bitcast_u32(RC_ATAN_PI_2_F32_BITS | (bits & 0x80000000u));
    }

    if (magnitude > RC_ARC_TAN_3PI_8_F32_BITS) {
        return rc_atan_beyond(d, bits);
    }

    if (magnitude > RC_ARC_TAN_PI_8_F32_BITS) {
        return rc_atan_middle(d, bits);
    }

    return (float)(d * (1.0 + rc_arc_atan_ratio_f32(d)));
}
