// math/sin_f64.c
//
// The sine of a double. Ported from musl 1.2.5 (MIT), src/math/sin.c, which is FreeBSD's s_sin.c.
// Its notice:
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunSoft, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.

#include "trig.h"

// sin(x) from x reduced by a multiple of pi/2.
static double rc_sin_reduced_f64(rc_trig_reduced_f64 reduced)
{
    switch ((uint32_t)reduced.quadrant & 3u) {
    case 0:
        return rc_sin_kernel_f64(reduced.hi, reduced.lo, true);
    case 1:
        return rc_cos_kernel_f64(reduced.hi, reduced.lo);
    case 2:
        return -rc_sin_kernel_f64(reduced.hi, reduced.lo, true);
    default:
        return -rc_cos_kernel_f64(reduced.hi, reduced.lo);
    }
}


double rc_sin_f64(double x)
{
    uint32_t top = rc_trig_abs_high_word_f64(x);

    // |x| < 2^-26, where sin(x) ~= x
    if (top < 0x3e500000u) {
        return x;
    }

    // |x| ~<= pi/4
    if (top <= 0x3fe921fbu) {
        return rc_sin_kernel_f64(x, 0.0, false);
    }

    // Infinity or NaN
    if (top >= 0x7ff00000u) {
        return x - x;
    }

    return rc_sin_reduced_f64(rc_trig_reduce_f64(x));
}
