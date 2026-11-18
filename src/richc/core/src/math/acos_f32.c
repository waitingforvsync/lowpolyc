// math/acos_f32.c
//
// The arccosine of a float. Ported from musl 1.2.5 (MIT), src/math/acosf.c, which is FreeBSD's
// e_acosf.c. Its notice:
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

// pi/2 split in two, hi + lo, for the extra precision.
static const float rc_acos_pio2_hi_f32 = 1.5707962513e+00f; // 0x3fc90fda
static const float rc_acos_pio2_lo_f32 = 7.5497894159e-08f; // 0x33a22168

// The coefficients of R's numerator, and of its denominator, whose first is 1.
static const float rc_acos_p_f32[] = {1.6666586697e-01f, -4.2743422091e-02f, -8.6563630030e-03f};
static const float rc_acos_q_f32[] = {1.0f, -7.0662963390e-01f};


// The rational approximation R(z) of (asin(x) - x) / x^3, where z = x^2.
static float rc_acos_r_f32(float z)
{
    const float *p = rc_acos_p_f32;
    const float *q = rc_acos_q_f32;
    float numerator = z * (p[0] + z * (p[1] + z * p[2]));
    float denominator = q[0] + z * q[1];

    return numerator / denominator;
}


// acos(x) for -1 < x <= -0.5, as pi - 2 asin(sqrt((1 + x) / 2)).
static float rc_acos_near_minus_one_f32(float x)
{
    float z = (1.0f + x) * 0.5f;
    float s = rc_sqrt_f32(z);
    float w = rc_acos_r_f32(z) * s - rc_acos_pio2_lo_f32;

    return 2.0f * (rc_acos_pio2_hi_f32 - (s + w));
}


// acos(x) for 0.5 <= x < 1, as 2 asin(sqrt((1 - x) / 2)). The root s is split into a high part
// with its low 12 bits cleared and a correction c, so that their sum carries the extra precision.
static float rc_acos_near_one_f32(float x)
{
    float z = (1.0f - x) * 0.5f;
    float s = rc_sqrt_f32(z);
    float s_hi = rc_bitcast_u32(rc_bitcast_f32(s) & 0xfffff000u);
    float c = (z - s_hi * s_hi) / (s + s_hi);
    float w = rc_acos_r_f32(z) * s + c;

    return 2.0f * (s_hi + w);
}


float rc_acos_f32(float x)
{
    uint32_t magnitude = rc_bitcast_f32(x) & 0x7fffffffu;

    // acos(1) is 0 and acos(-1) is pi, and anything larger, or NaN, has none
    if (magnitude == 0x3f800000u) {
        return x < 0.0f ? 2.0f * rc_acos_pio2_hi_f32 : 0.0f;
    }

    if (magnitude > 0x3f800000u) {
        return 0.0f / (x - x);
    }

    // At or below 2^-26, acos(x) is pi/2 to float precision
    if (magnitude <= 0x32800000u) {
        return rc_acos_pio2_hi_f32;
    }

    // Below 0.5, acos(x) = pi/2 - (x + x^3 R(x^2))
    if (magnitude < 0x3f000000u) {
        return rc_acos_pio2_hi_f32 - (x - (rc_acos_pio2_lo_f32 - x * rc_acos_r_f32(x * x)));
    }

    if (x < 0.0f) {
        return rc_acos_near_minus_one_f32(x);
    }

    return rc_acos_near_one_f32(x);
}
