// math/cbrt_f64.c
//
// The cube root of a double. Ported from musl 1.2.5 (MIT), src/math/cbrt.c, which is FreeBSD's
// s_cbrt.c. Its notice:
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunPro, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.
//
// Optimized by Bruce D. Evans.

#include "richc/math/math.h"

// The biases that turn a third of the high word into a rough cube root, for normal and subnormal
// numbers: (1023 - 1023/3 - 0.03306235651) * 2^20, and that less 54/3 for the 2^54 scaling.
static const uint32_t rc_cbrt_bias_f64 = 715094163;
static const uint32_t rc_cbrt_subnormal_bias_f64 = 696219795;

// P(r) ~= 1 / cbrt(r) for |r - 1| < 1/10, with |1/cbrt(r) - P(r)| < 2^-23.5.
static const double rc_cbrt_p_f64[] = {
    1.87595182427177009643,   // 0x3ffe03e6 0f61e692
    -1.88497979543377169875,  // 0xbffe28e0 92f02420
    1.621429720105354466140,  // 0x3ff9f160 4a49d6c2
    -0.758397934778766047437, // 0xbfe844cb bee751d9
    0.145996192886612446982,  // 0x3fc2b000 d4e4edd7
};


// A cube root of the nonzero finite x good to 5 bits. For x = 2^e (1 + m), cbrt(x) is about
// 2^(e/3) (1 + (e%3 + m) / 3), within 1 in 16, and a bias of -0.03306235651 on the last term
// halves that. Dividing the high word by 3, exponent bias and all, gives almost exactly this. A
// subnormal is scaled up by 2^54 first, and its bias divides the scale back out.
static double rc_cbrt_estimate_f64(double x, uint32_t top)
{
    uint64_t sign = rc_bitcast_f64(x) & 0x8000000000000000ull;

    if (top < 0x00100000u) {
        uint32_t scaled_top = (uint32_t)(rc_bitcast_f64(x * 0x1p54) >> 32) & 0x7fffffffu;

        return rc_bitcast_u64(sign | ((uint64_t)(scaled_top / 3 + rc_cbrt_subnormal_bias_f64) << 32));
    }

    return rc_bitcast_u64(sign | ((uint64_t)(top / 3 + rc_cbrt_bias_f64) << 32));
}


// t to 23 bits, as t P(t^3 / x), since t is within about 1/32 of cbrt(x) and so t^3 / x is close
// enough to 1.
static double rc_cbrt_refine_f64(double t, double x)
{
    const double *p = rc_cbrt_p_f64;
    double r = (t * t) * (t / x);

    return t * ((p[0] + r * (p[1] + r * p[2])) + ((r * r) * r) * (p[3] + r * p[4]));
}


// t rounded away from zero to 23 bits: a little sloppily, but always larger in magnitude than
// cbrt(x) and by no more than about 2 23-bit ulps, which the Newton step needs.
static double rc_cbrt_round_f64(double t)
{
    return rc_bitcast_u64((rc_bitcast_f64(t) + 0x80000000u) & 0xffffffffc0000000ull);
}


// One Newton step to 53 bits, with an error under 0.667 ulps. t * t and t + t are exact, and so is
// r - t, with |r| < |t|.
static double rc_cbrt_newton_f64(double t, double x)
{
    double r = x / (t * t);
    double correction = (r - t) / ((t + t) + r);

    return t + t * correction;
}


// The cube root of the nonzero finite x.
static double rc_cbrt_finite_f64(double x, uint32_t top)
{
    double estimate = rc_cbrt_estimate_f64(x, top);
    double t23 = rc_cbrt_round_f64(rc_cbrt_refine_f64(estimate, x));

    return rc_cbrt_newton_f64(t23, x);
}


double rc_cbrt_f64(double x)
{
    uint64_t magnitude = rc_bitcast_f64(x) & 0x7fffffffffffffffull;
    uint32_t top = (uint32_t)(magnitude >> 32);

    // Infinity, NaN and both zeros are their own cube roots
    if (top >= 0x7ff00000u) {
        return x + x;
    }

    if (magnitude == 0) {
        return x;
    }

    return rc_cbrt_finite_f64(x, top);
}
