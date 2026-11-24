// math/asin_f32.c
//
// rc_asin_f32: the arcsine of a float, worked in double.
//
// Up to 1/2 in size, asin(x) is x plus x^3 times a quintic in x^2, fitted to 2^-31.8. That form
// is odd, so asin(-x) is exactly -asin(x) there. Beyond, we work on a = |x| and give the result
// x's sign, with asin(a) = pi/2 - 2 asin(s) for s = sqrt((1 - a) / 2), which is at most 1/2
// again, and 1/2 - a/2 is exact. Rounding that double result to float adds little to the half ulp
// the rounding itself costs: 0.505 ulp at worst over every float. Beyond 1, s and so the result
// are NaN.
//
// The two ranges branch, on |x|'s bits, which settles sooner than comparing the float: working out
// both and choosing costs more than the odd misprediction. Beyond 1/2 the sign goes on as a product
// with 1 or -1, but beyond 1, where the result is NaN, by copysign, so asin stays odd bit for bit.

#include "richc/math/math.h"

#include "math/arc.h"

// asin(a) for a > 1/2: NaN beyond 1, and for NaN.
static double rc_asin_near_one(double a)
{
    double z = 0.5 - 0.5 * a;
    double s = rc_sqrt_f64(z);

    return RC_ARC_PI_2_HI - 2.0 * (s + rc_arc_asin_tail_f32(s, z));
}


float rc_asin_f32(float x)
{
    uint32_t bits = rc_bitcast_f32(x);
    uint32_t magnitude = bits & 0x7fffffffu;
    double d = (double)x;

    if (magnitude <= 0x3f000000u) {
        return (float)(d + rc_arc_asin_tail_f32(d, d * d));
    }

    // Beyond 1, and for NaN, the result is NaN, which a product would not give x's sign
    if (magnitude > 0x3f800000u) {
        return (float)rc_copysign_f64(rc_asin_near_one(rc_abs_f64(d)), d);
    }

    return (float)(rc_asin_near_one(rc_abs_f64(d)) * rc_arc_sign_f64(bits >> 31));
}
