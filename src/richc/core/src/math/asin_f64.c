// math/asin_f64.c
//
// The arcsine of a double. Ported from musl 1.2.5 (MIT), src/math/asin.c, which is FreeBSD's
// e_asin.c. Its notice:
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunSoft, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.

#include "richc/math/math.h"

// pi/2 split in two, hi + lo, for the extra precision.
static const double rc_asin_pio2_hi_f64 = 1.57079632679489655800e+00; // 0x3ff921fb 54442d18
static const double rc_asin_pio2_lo_f64 = 6.12323399573676603587e-17; // 0x3c91a626 33145c07

// The coefficients of R's numerator, and of its denominator, whose first is 1.
static const double rc_asin_p_f64[] = {
    1.66666666666666657415e-01,  // 0x3fc55555 55555555
    -3.25565818622400915405e-01, // 0xbfd4d612 03eb6f7d
    2.01212532134862925881e-01,  // 0x3fc9c155 0e884455
    -4.00555345006794114027e-02, // 0xbfa48228 b5688f3b
    7.91534994289814532176e-04,  // 0x3f49efe0 7501b288
    3.47933107596021167570e-05,  // 0x3f023de1 0dfdf709
};
static const double rc_asin_q_f64[] = {
    1.0,
    -2.40339491173441421878e+00, // 0xc0033a27 1c8a2d4b
    2.02094576023350569471e+00,  // 0x40002ae5 9c598ac8
    -6.88283971605453293030e-01, // 0xbfe6066c 1b8d0159
    7.70381505559019352791e-02,  // 0x3fb3b8c5 b12e9282
};


// The rational approximation R(z) of (asin(x) - x) / x^3, where z = x^2, with a Remez error under
// 2^-58.75 for |x| <= 0.5.
static double rc_asin_r_f64(double z)
{
    const double *p = rc_asin_p_f64;
    const double *q = rc_asin_q_f64;
    double numerator = z * (p[0] + z * (p[1] + z * (p[2] + z * (p[3] + z * (p[4] + z * p[5])))));
    double denominator = q[0] + z * (q[1] + z * (q[2] + z * (q[3] + z * q[4])));

    return numerator / denominator;
}


// pi/2 - 2 (s + s z R(z)) for 0.975 < |x| < 1, where z = (1 - |x|) / 2 and s = sqrt(z).
static double rc_asin_from_root_f64(double s, double r)
{
    return rc_asin_pio2_hi_f64 - (2.0 * (s + s * r) - rc_asin_pio2_lo_f64);
}


// The same for 0.5 <= |x| <= 0.975, where it needs more care. The root is split into a high part f
// with its low word cleared and a correction c = sqrt(z) - f, and pi/2 into two quarters, to keep
// the precision through the subtraction.
static double rc_asin_from_split_root_f64(double z, double s, double r)
{
    double pio4_hi = 0.5 * rc_asin_pio2_hi_f64;
    double f = rc_bitcast_u64(rc_bitcast_f64(s) & 0xffffffff00000000ull);
    double c = (z - f * f) / (s + f);

    return pio4_hi - (2.0 * s * r - (rc_asin_pio2_lo_f64 - 2.0 * c) - (pio4_hi - 2.0 * f));
}


// asin(x) for 0.5 <= |x| < 1, as pi/2 - 2 asin(sqrt((1 - |x|) / 2)).
static double rc_asin_near_one_f64(double x, uint32_t top)
{
    double z = (1.0 - rc_abs_f64(x)) * 0.5;
    double s = rc_sqrt_f64(z);
    double r = rc_asin_r_f64(z);
    double result = top >= 0x3fef3333u ? rc_asin_from_root_f64(s, r) : rc_asin_from_split_root_f64(z, s, r);

    return x < 0.0 ? -result : result;
}


double rc_asin_f64(double x)
{
    uint64_t magnitude = rc_bitcast_f64(x) & 0x7fffffffffffffffull;
    uint32_t top = (uint32_t)(magnitude >> 32);

    // asin(+-1) is +-pi/2, and anything larger, or NaN, has none
    if (magnitude == 0x3ff0000000000000ull) {
        return x * rc_asin_pio2_hi_f64;
    }

    if (top >= 0x3ff00000u) {
        return 0.0 / (x - x);
    }

    // Below 2^-26, asin(x) is x to double precision
    if (top < 0x3e500000u) {
        return x;
    }

    // Below 0.5, asin(x) = x + x^3 R(x^2)
    if (top < 0x3fe00000u) {
        return x + x * rc_asin_r_f64(x * x);
    }

    return rc_asin_near_one_f64(x, top);
}
