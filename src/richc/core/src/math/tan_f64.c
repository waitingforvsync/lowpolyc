// math/tan_f64.c
//
// The tangent of a double. Ported from musl 1.2.5 (MIT), src/math/tan.c, which is FreeBSD's
// s_tan.c. Its notice:
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunSoft, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.

#include "trig.h"

// tan(x) from x reduced by a multiple of pi/2, where an odd multiple turns tan into -1 / tan.
static double rc_tan_reduced_f64(rc_trig_reduced_f64 reduced)
{
    return rc_tan_kernel_f64(reduced.hi, reduced.lo, ((uint32_t)reduced.quadrant & 1u) != 0);
}


double rc_tan_f64(double x)
{
    uint32_t top = rc_trig_abs_high_word_f64(x);

    // |x| < 2^-27, where tan(x) ~= x
    if (top < 0x3e400000u) {
        return x;
    }

    // |x| ~<= pi/4
    if (top <= 0x3fe921fbu) {
        return rc_tan_kernel_f64(x, 0.0, false);
    }

    // Infinity or NaN
    if (top >= 0x7ff00000u) {
        return x - x;
    }

    return rc_tan_reduced_f64(rc_trig_reduce_f64(x));
}
