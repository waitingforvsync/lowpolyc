// math/asin_f32.c
//
// The arcsine of a float. Ported from musl 1.2.5 (MIT), src/math/asinf.c, which is FreeBSD's
// e_asinf.c. Its notice:
//
// Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunPro, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.

#include "richc/math/math.h"

// pi/2, as a double: the range near 1 finishes in double precision.
static const double rc_asin_pio2_f32 = 1.570796326794896558e+00;

// The coefficients of R's numerator, and of its denominator, whose first is 1.
static const float rc_asin_p_f32[] = {1.6666586697e-01f, -4.2743422091e-02f, -8.6563630030e-03f};
static const float rc_asin_q_f32[] = {1.0f, -7.0662963390e-01f};


// The rational approximation R(z) of (asin(x) - x) / x^3, where z = x^2.
static float rc_asin_r_f32(float z)
{
    const float *p = rc_asin_p_f32;
    const float *q = rc_asin_q_f32;
    float numerator = z * (p[0] + z * (p[1] + z * p[2]));
    float denominator = q[0] + z * q[1];

    return numerator / denominator;
}


// asin(x) for 0.5 <= |x| < 1, as pi/2 - 2 asin(sqrt((1 - |x|) / 2)), with the root in double.
static float rc_asin_near_one_f32(float x)
{
    float z = (1.0f - rc_abs_f32(x)) * 0.5f;
    double s = rc_sqrt_f64((double)z);
    float result = (float)(rc_asin_pio2_f32 - 2.0 * (s + s * (double)rc_asin_r_f32(z)));

    return x < 0.0f ? -result : result;
}


float rc_asin_f32(float x)
{
    uint32_t magnitude = rc_bitcast_f32(x) & 0x7fffffffu;

    // asin(+-1) is +-pi/2, and anything larger, or NaN, has none
    if (magnitude == 0x3f800000u) {
        return (float)((double)x * rc_asin_pio2_f32);
    }

    if (magnitude > 0x3f800000u) {
        return 0.0f / (x - x);
    }

    // Below 2^-12, asin(x) is x to float precision
    if (magnitude < 0x39800000u) {
        return x;
    }

    // Below 0.5, asin(x) = x + x^3 R(x^2)
    if (magnitude < 0x3f000000u) {
        return x + x * rc_asin_r_f32(x * x);
    }

    return rc_asin_near_one_f32(x);
}
