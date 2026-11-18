// math/atan2_f64.c
//
// The angle of a point, in doubles. Ported from musl 1.2.5 (MIT), src/math/atan2.c, which is
// FreeBSD's e_atan2.c. Its notice:
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunSoft, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.

#include "richc/math/math.h"

// pi split in two, hi + lo, for the extra precision.
static const double rc_atan2_pi_f64 = 3.1415926535897931160e+00;    // 0x400921fb 54442d18
static const double rc_atan2_pi_lo_f64 = 1.2246467991473531772e-16; // 0x3ca1a626 33145c07


// The angle, negated when y is negative.
static double rc_atan2_signed_f64(double angle, bool y_negative)
{
    return y_negative ? -angle : angle;
}


// The angle when x is infinite: +-pi/4 or +-3pi/4 if y is too, and otherwise +-0 or +-pi.
static double rc_atan2_infinite_x_f64(uint32_t y_top, bool x_negative, bool y_negative)
{
    double pi = rc_atan2_pi_f64;

    if (y_top == 0x7ff00000u) {
        return rc_atan2_signed_f64(x_negative ? 3.0 * pi / 4.0 : pi / 4.0, y_negative);
    }

    return rc_atan2_signed_f64(x_negative ? pi : 0.0, y_negative);
}


// True if y is infinite or |y / x| > 2^64, so the angle is +-pi/2 to double precision. Each top is
// the high word of the magnitude.
static bool rc_atan2_is_steep_f64(uint32_t x_top, uint32_t y_top)
{
    return x_top + (64u << 20) < y_top || y_top == 0x7ff00000u;
}


// atan(|y / x|), taken as 0 when x is negative and |y / x| < 2^-64, where it vanishes beside pi.
static double rc_atan2_slope_f64(double y, double x, uint32_t x_top, uint32_t y_top)
{
    bool x_negative = x < 0.0;
    bool tiny = y_top + (64u << 20) < x_top;

    if (x_negative && tiny) {
        return 0.0;
    }

    return rc_atan_f64(rc_abs_f64(y / x));
}


// The angle from z = atan(|y / x|), moved into the quadrant of (x, y).
static double rc_atan2_from_slope_f64(double z, bool x_negative, bool y_negative)
{
    if (!x_negative) {
        return y_negative ? -z : z;
    }

    if (y_negative) {
        return (z - rc_atan2_pi_lo_f64) - rc_atan2_pi_f64;
    }

    return rc_atan2_pi_f64 - (z - rc_atan2_pi_lo_f64);
}


double rc_atan2_f64(double y, double x)
{
    uint64_t x_bits = rc_bitcast_f64(x);
    uint64_t y_bits = rc_bitcast_f64(y);
    uint64_t x_magnitude = x_bits & 0x7fffffffffffffffull;
    uint64_t y_magnitude = y_bits & 0x7fffffffffffffffull;
    uint32_t x_top = (uint32_t)(x_magnitude >> 32);
    uint32_t y_top = (uint32_t)(y_magnitude >> 32);
    bool x_negative = (x_bits >> 63) != 0;
    bool y_negative = (y_bits >> 63) != 0;

    // NaN in either gives NaN, and x = 1 is plain atan
    if (x != x || y != y) {
        return x + y;
    }

    if (x_bits == 0x3ff0000000000000ull) {
        return rc_atan_f64(y);
    }

    // On the x axis, +-0 to the right and +-pi to the left
    if (y_magnitude == 0) {
        return x_negative ? rc_atan2_signed_f64(rc_atan2_pi_f64, y_negative) : y;
    }

    // On the y axis, +-pi/2
    if (x_magnitude == 0) {
        return rc_atan2_signed_f64(rc_atan2_pi_f64 / 2.0, y_negative);
    }

    if (x_top == 0x7ff00000u) {
        return rc_atan2_infinite_x_f64(y_top, x_negative, y_negative);
    }

    if (rc_atan2_is_steep_f64(x_top, y_top)) {
        return rc_atan2_signed_f64(rc_atan2_pi_f64 / 2.0, y_negative);
    }

    return rc_atan2_from_slope_f64(rc_atan2_slope_f64(y, x, x_top, y_top), x_negative, y_negative);
}
