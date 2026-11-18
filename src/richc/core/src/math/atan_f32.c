// math/atan_f32.c
//
// The arctangent of a float. Ported from musl 1.2.5 (MIT), src/math/atanf.c, which is FreeBSD's
// s_atanf.c. Its notice:
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

// atan(0.5), atan(1), atan(1.5) and atan(infinity), each split in two, hi + lo.
static const float rc_atan_hi_f32[] = {
    4.6364760399e-01f, // 0x3eed6338
    7.8539812565e-01f, // 0x3f490fda
    9.8279368877e-01f, // 0x3f7b985e
    1.5707962513e+00f, // 0x3fc90fda
};
static const float rc_atan_lo_f32[] = {
    5.0121582440e-09f, // 0x31ac3769
    3.7748947079e-08f, // 0x33222168
    3.4473217170e-08f, // 0x33140fb4
    7.5497894159e-08f, // 0x33a22168
};

// The odd series for atan on [-7/16, 7/16]: atan(t) ~= t - t^3 (a0 + a1 t^2 + a2 t^4 + ...).
static const float rc_atan_series_f32[] = {
    3.3333328366e-01f,
    -1.9999158382e-01f,
    1.4253635705e-01f,
    -1.0648017377e-01f,
    6.1687607318e-02f,
};


// t - atan(t), from the series, split into its odd and even halves for speed.
static float rc_atan_remainder_f32(float t)
{
    const float *a = rc_atan_series_f32;
    float z = t * t;
    float w = z * z;
    float odd = z * (a[0] + w * (a[2] + w * a[4]));
    float even = w * (a[1] + w * a[3]);

    return t * (odd + even);
}


// atan(t) plus the tabled angle it was reduced by.
static float rc_atan_offset_f32(uint32_t angle, float t)
{
    return rc_atan_hi_f32[angle] - ((rc_atan_remainder_f32(t) - rc_atan_lo_f32[angle]) - t);
}


// atan(x) for 7/16 <= x < 2^26, from a tabled angle and the arctangent of what is left:
//   [7/16, 11/16)   atan(1/2) + atan((2x - 1) / (2 + x))
//   [11/16, 19/16)  atan(1) + atan((x - 1) / (x + 1))
//   [19/16, 39/16)  atan(3/2) + atan((x - 1.5) / (1 + 1.5x))
//   [39/16, 2^26)   atan(infinity) + atan(-1 / x)
static float rc_atan_reduced_f32(float x, uint32_t magnitude)
{
    if (magnitude < 0x3f300000u) {
        return rc_atan_offset_f32(0, (2.0f * x - 1.0f) / (2.0f + x));
    }

    if (magnitude < 0x3f980000u) {
        return rc_atan_offset_f32(1, (x - 1.0f) / (x + 1.0f));
    }

    if (magnitude < 0x401c0000u) {
        return rc_atan_offset_f32(2, (x - 1.5f) / (1.0f + 1.5f * x));
    }

    return rc_atan_offset_f32(3, -1.0f / x);
}


float rc_atan_f32(float x)
{
    uint32_t magnitude = rc_bitcast_f32(x) & 0x7fffffffu;

    // NaN stays NaN, and from 2^26 up atan(x) is +-pi/2 to float precision
    if (magnitude > 0x7f800000u) {
        return x;
    }

    if (magnitude >= 0x4c800000u) {
        return x < 0.0f ? -rc_atan_hi_f32[3] : rc_atan_hi_f32[3];
    }

    // Below 2^-12, atan(x) is x to float precision
    if (magnitude < 0x39800000u) {
        return x;
    }

    // Below 7/16, the series alone will do
    if (magnitude < 0x3ee00000u) {
        return x - rc_atan_remainder_f32(x);
    }

    if (x < 0.0f) {
        return -rc_atan_reduced_f32(-x, magnitude);
    }

    return rc_atan_reduced_f32(x, magnitude);
}
