// math/sin_f32.c
//
// The sine of a float. Ported from musl 1.2.5 (MIT), src/math/sinf.c, which is FreeBSD's
// s_sinf.c. Its notice:
//
// Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
// Optimized by Bruce D. Evans.
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunPro, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.

#include "trig.h"

// Small multiples of pi/2, rounded to double precision.
static const double pio2 = 0x1.921fb54442d18p+0;
static const double pi = 0x1.921fb54442d18p+1;
static const double three_pio2 = 0x1.2d97c7f3321d2p+2;
static const double two_pi = 0x1.921fb54442d18p+2;

// sin(x) from x reduced by a multiple of pi/2.
static float rc_sin_reduced_f32(rc_trig_reduced_f32 reduced)
{
    switch ((uint32_t)reduced.quadrant & 3u) {
    case 0:
        return rc_sin_kernel_f32(reduced.angle);
    case 1:
        return rc_cos_kernel_f32(reduced.angle);
    case 2:
        return rc_sin_kernel_f32(-reduced.angle);
    default:
        return -rc_cos_kernel_f32(reduced.angle);
    }
}


// Up to 9pi/4 either way, we subtract the nearest multiple of pi/2 in double precision right here.
float rc_sin_f32(float x)
{
    uint32_t bits = rc_bitcast_f32(x);
    uint32_t abs_bits = bits & 0x7fffffffu;
    bool negative = (bits >> 31) != 0;
    double wide = (double)x;

    // |x| < 2^-12, where sin(x) ~= x
    if (abs_bits < 0x39800000u) {
        return x;
    }

    // |x| ~<= pi/4
    if (abs_bits <= 0x3f490fdau) {
        return rc_sin_kernel_f32(wide);
    }

    // |x| ~<= 3pi/4
    if (abs_bits <= 0x4016cbe3u) {
        return negative ? -rc_cos_kernel_f32(wide + pio2) : rc_cos_kernel_f32(wide - pio2);
    }

    // |x| ~<= 5pi/4
    if (abs_bits <= 0x407b53d1u) {
        return rc_sin_kernel_f32(negative ? -(wide + pi) : -(wide - pi));
    }

    // |x| ~<= 7pi/4
    if (abs_bits <= 0x40afeddfu) {
        return negative ? rc_cos_kernel_f32(wide + three_pio2) : -rc_cos_kernel_f32(wide - three_pio2);
    }

    // |x| ~<= 9pi/4
    if (abs_bits <= 0x40e231d5u) {
        return rc_sin_kernel_f32(negative ? wide + two_pi : wide - two_pi);
    }

    // Infinity or NaN
    if (abs_bits >= 0x7f800000u) {
        return x - x;
    }

    return rc_sin_reduced_f32(rc_trig_reduce_f32(x));
}
