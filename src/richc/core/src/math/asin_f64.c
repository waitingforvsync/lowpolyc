// math/asin_f64.c
//
// rc_asin_f64: the arcsine of a double.
//
// Up to 1/2 in size, asin(x) = x + x R(x^2), where R(z) = asin(a) / a - 1 for z = a^2 is z times a
// ratio of a quintic and a quartic, fitted to 2^-59.6. That form is odd, so asin(-x) is exactly
// -asin(x) there. Beyond, we work on a = |x| and give the result x's sign, with
// asin(a) = pi/2 - 2 asin(s) for s = sqrt(z), z = (1 - a) / 2, which is at most 1/2 again, and
// 1 - a is exact. Up to 0.975, the rounding of s alone could cost an ulp, so we split s into its
// top 26 bits f and c = (z - f^2) / (s + f), which together are good to twice double precision:
// f^2 and z - f^2 are exact, and so is pi/2 - 2f. Beyond 0.975, 2s is small beside pi/2 and the
// rounded root will do. 0.77 ulp at worst in testing. Beyond 1, s and so the result are NaN.
//
// The two ranges branch, on |x|'s bits, which settles sooner than comparing the double: working
// out both and choosing costs more than the odd misprediction. Beyond 1/2 the sign goes on as a
// product with 1 or -1, but beyond 1, where the result is NaN, by copysign, so asin stays odd bit
// for bit.

#include "richc/math/math.h"

#include "math/arc.h"

// asin(a) for 1/2 < a <= 0.975, from z, s = sqrt(z) and r = R(z), splitting s.
static double rc_asin_split(double z, double s, double r)
{
    double f = rc_arc_top_bits(s);
    double c = (z - f * f) / (s + f);

    return (RC_ARC_PI_2_HI - 2.0 * f) + (RC_ARC_PI_2_LO - 2.0 * (c + s * r));
}


// asin(a) for a > 1/2: NaN beyond 1, and for NaN.
static double rc_asin_near_one(double a)
{
    double z = (1.0 - a) * 0.5;
    double s = rc_sqrt_f64(z);
    double r = rc_arc_asin_ratio_f64(z);

    if (a > 0.975) {
        return RC_ARC_PI_2_HI - (2.0 * (s + s * r) - RC_ARC_PI_2_LO);
    }

    return rc_asin_split(z, s, r);
}


double rc_asin_f64(double x)
{
    uint64_t bits = rc_bitcast_f64(x);
    uint64_t magnitude = bits & 0x7fffffffffffffffull;

    if (magnitude <= 0x3fe0000000000000ull) {
        return x + x * rc_arc_asin_ratio_f64(x * x);
    }

    // Beyond 1, and for NaN, the result is NaN, which a product would not give x's sign
    if (magnitude > 0x3ff0000000000000ull) {
        return rc_copysign_f64(rc_asin_near_one(rc_abs_f64(x)), x);
    }

    return rc_asin_near_one(rc_abs_f64(x)) * rc_arc_sign_f64(bits >> 63);
}
