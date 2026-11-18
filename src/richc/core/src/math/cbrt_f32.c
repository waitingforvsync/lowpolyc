// math/cbrt_f32.c
//
// The cube root of a float. Ported from musl 1.2.5 (MIT), src/math/cbrtf.c, which is FreeBSD's
// s_cbrtf.c. Its notice:
//
// Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
// Debugged and optimized by Bruce D. Evans.
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunPro, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.

#include "richc/math/math.h"

// The biases that turn a third of the bits into a rough cube root, for normal and for subnormal
// numbers: (127 - 127.0/3 - 0.03306235651) * 2^23, and the same less 24/3 for the 2^24 scaling.
static const uint32_t rc_cbrt_bias_f32 = 709958130;
static const uint32_t rc_cbrt_subnormal_bias_f32 = 642849266;


// A cube root of the nonzero finite x good to 5 bits, from a third of its bits. A subnormal is
// scaled up by 2^24 first, and its bias divides the scale back out.
static float rc_cbrt_estimate_f32(float x, uint32_t magnitude)
{
    uint32_t sign = rc_bitcast_f32(x) & 0x80000000u;

    if (magnitude < 0x00800000u) {
        uint32_t scaled = rc_bitcast_f32(x * 0x1p24f) & 0x7fffffffu;

        return rc_bitcast_u32(sign | (scaled / 3 + rc_cbrt_subnormal_bias_f32));
    }

    return rc_bitcast_u32(sign | (magnitude / 3 + rc_cbrt_bias_f32));
}


// One Newton step for t^3 = x, solving t^2 - x/t = 0, arranged so nothing overflows or
// underflows in double.
static double rc_cbrt_newton_f32(double t, double x)
{
    double r = t * t * t;

    return t * (x + x + r) / (x + r + r);
}


// The cube root of the nonzero finite x. Two Newton steps take the estimate to 16 bits and then 47,
// and rounding that to 24 is perfect.
static float rc_cbrt_finite_f32(float x, uint32_t magnitude)
{
    double estimate = (double)rc_cbrt_estimate_f32(x, magnitude);
    double t16 = rc_cbrt_newton_f32(estimate, (double)x);
    double t47 = rc_cbrt_newton_f32(t16, (double)x);

    return (float)t47;
}


float rc_cbrt_f32(float x)
{
    uint32_t magnitude = rc_bitcast_f32(x) & 0x7fffffffu;

    // Infinity, NaN and both zeros are their own cube roots
    if (magnitude >= 0x7f800000u) {
        return x + x;
    }

    if (magnitude == 0) {
        return x;
    }

    return rc_cbrt_finite_f32(x, magnitude);
}
