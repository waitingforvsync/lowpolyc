// math/acos_f64.c
//
// rc_acos_f64: the arccosine of a double.
//
// Up to 1/2 in size, acos(x) = pi/2 - asin(x), with asin(x) = x + x R(x^2), where R(z) =
// asin(a) / a - 1 for z = a^2 is z times a ratio of a quintic and a quartic, fitted to 2^-59.6.
// We take pi/2 - x exactly, as a sum of two doubles, and pi/2's second double with it. Beyond,
// with s = sqrt(z) for z = (1 - |x|) / 2, which is at most 1/2 again, acos(x) = 2 asin(s) for
// x > 0 and pi - 2 asin(s) for x < 0, and 1 - |x| is exact. For x > 0 the rounding of s would cost
// half an ulp, so we split s into its top 26 bits f and c = (z - f^2) / (s + f), which together are
// good to twice double precision, as f^2 and z - f^2 are exact. For x < 0, where the result is at
// least 2pi/3, it is enough to take pi - 2s exactly, as a sum of two doubles. 0.71 ulp at worst in
// testing. Beyond 1, s and so the result are NaN.
//
// The three ranges branch, on x's bits, which settles sooner than comparing the double: working
// out all three and choosing costs more than the odd misprediction.

#include "richc/math/math.h"

#include "math/arc.h"

// acos(x) for x > 1/2: 2 (f + c) (1 + R(z)). NaN beyond 1. At 1, s + f is 0, so we divide by a
// tiny number instead, which leaves every other quotient alone.
static double rc_acos_near_one(double x)
{
    double z = (1.0 - x) * 0.5;
    double s = rc_sqrt_f64(z);
    double f = rc_arc_top_bits(s);
    double c = (z - f * f) / rc_max_f64(0x1p-600, s + f);

    return 2.0 * (f + (c + s * rc_arc_asin_ratio_f64(z)));
}


// acos(x) for x < -1/2: pi - 2 s (1 + R(z)), with pi - 2s worked exactly as a sum of two. NaN
// beyond -1, and for NaN.
static double rc_acos_near_minus_one(double x)
{
    double z = (1.0 + x) * 0.5;
    double s = rc_sqrt_f64(z);
    rc_arc_sum base = rc_arc_fast_two_sum(RC_ARC_PI_HI, -2.0 * s);

    return base.hi + ((base.lo + RC_ARC_PI_LO) - 2.0 * (s * rc_arc_asin_ratio_f64(z)));
}


// acos(x) for |x| <= 1/2: pi/2 - x (1 + R(x^2)), with pi/2 - x worked exactly as a sum of two.
static double rc_acos_middle(double x)
{
    rc_arc_sum base = rc_arc_fast_two_sum(RC_ARC_PI_2_HI, -x);

    return base.hi + ((base.lo + RC_ARC_PI_2_LO) - x * rc_arc_asin_ratio_f64(x * x));
}


double rc_acos_f64(double x)
{
    uint64_t bits = rc_bitcast_f64(x);

    if ((bits & 0x7fffffffffffffffull) <= 0x3fe0000000000000ull) {
        return rc_acos_middle(x);
    }

    if (bits >> 63 == 0) {
        return rc_acos_near_one(x);
    }

    return rc_acos_near_minus_one(x);
}
