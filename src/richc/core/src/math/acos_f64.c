// math/acos_f64.c
//
// The arccosine of a double. Ported from musl 1.2.5 (MIT), src/math/acos.c, which is FreeBSD's
// e_acos.c. Its notice:
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunSoft, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.

#include "richc/math/math.h"

// pi/2 split in two, hi + lo, for the extra precision.
static const double rc_acos_pio2_hi_f64 = 1.57079632679489655800e+00; // 0x3ff921fb 54442d18
static const double rc_acos_pio2_lo_f64 = 6.12323399573676603587e-17; // 0x3c91a626 33145c07

// The coefficients of R's numerator, and of its denominator, whose first is 1.
static const double rc_acos_p_f64[] = {
    1.66666666666666657415e-01,  // 0x3fc55555 55555555
    -3.25565818622400915405e-01, // 0xbfd4d612 03eb6f7d
    2.01212532134862925881e-01,  // 0x3fc9c155 0e884455
    -4.00555345006794114027e-02, // 0xbfa48228 b5688f3b
    7.91534994289814532176e-04,  // 0x3f49efe0 7501b288
    3.47933107596021167570e-05,  // 0x3f023de1 0dfdf709
};
static const double rc_acos_q_f64[] = {
    1.0,
    -2.40339491173441421878e+00, // 0xc0033a27 1c8a2d4b
    2.02094576023350569471e+00,  // 0x40002ae5 9c598ac8
    -6.88283971605453293030e-01, // 0xbfe6066c 1b8d0159
    7.70381505559019352791e-02,  // 0x3fb3b8c5 b12e9282
};


// The rational approximation R(z) of (asin(x) - x) / x^3, where z = x^2, with a Remez error under
// 2^-58.75 for |x| <= 0.5.
static double rc_acos_r_f64(double z)
{
    const double *p = rc_acos_p_f64;
    const double *q = rc_acos_q_f64;
    double numerator = z * (p[0] + z * (p[1] + z * (p[2] + z * (p[3] + z * (p[4] + z * p[5])))));
    double denominator = q[0] + z * (q[1] + z * (q[2] + z * (q[3] + z * q[4])));

    return numerator / denominator;
}


// acos(x) for -1 < x <= -0.5, as pi - 2 asin(sqrt((1 + x) / 2)) = pi - 2 (s + s z R(z)), where
// z = (1 + x) / 2 and s = sqrt(z).
static double rc_acos_near_minus_one_f64(double x)
{
    double z = (1.0 + x) * 0.5;
    double s = rc_sqrt_f64(z);
    double w = rc_acos_r_f64(z) * s - rc_acos_pio2_lo_f64;

    return 2.0 * (rc_acos_pio2_hi_f64 - (s + w));
}


// acos(x) for 0.5 <= x < 1, as 2 asin(sqrt((1 - x) / 2)) = 2 (s + s z R(z)), where z = (1 - x) / 2
// and s = sqrt(z). The root is split into a high part with its low word cleared and a correction
// c, so that their sum carries the extra precision.
static double rc_acos_near_one_f64(double x)
{
    double z = (1.0 - x) * 0.5;
    double s = rc_sqrt_f64(z);
    double s_hi = rc_bitcast_u64(rc_bitcast_f64(s) & 0xffffffff00000000ull);
    double c = (z - s_hi * s_hi) / (s + s_hi);
    double w = rc_acos_r_f64(z) * s + c;

    return 2.0 * (s_hi + w);
}


double rc_acos_f64(double x)
{
    uint64_t magnitude = rc_bitcast_f64(x) & 0x7fffffffffffffffull;
    uint32_t top = (uint32_t)(magnitude >> 32);

    // acos(1) is 0 and acos(-1) is pi, and anything larger, or NaN, has none
    if (magnitude == 0x3ff0000000000000ull) {
        return x < 0.0 ? 2.0 * rc_acos_pio2_hi_f64 : 0.0;
    }

    if (top >= 0x3ff00000u) {
        return 0.0 / (x - x);
    }

    // At or below 2^-57, acos(x) is pi/2 to double precision
    if (top <= 0x3c600000u) {
        return rc_acos_pio2_hi_f64;
    }

    // Below 0.5, acos(x) = pi/2 - (x + x^3 R(x^2))
    if (top < 0x3fe00000u) {
        return rc_acos_pio2_hi_f64 - (x - (rc_acos_pio2_lo_f64 - x * rc_acos_r_f64(x * x)));
    }

    if (x < 0.0) {
        return rc_acos_near_minus_one_f64(x);
    }

    return rc_acos_near_one_f64(x);
}
