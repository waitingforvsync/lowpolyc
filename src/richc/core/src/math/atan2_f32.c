// math/atan2_f32.c
//
// The angle of a point, in floats. Ported from musl 1.2.5 (MIT), src/math/atan2f.c, which is
// FreeBSD's e_atan2f.c. Its notice:
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

// pi split in two, hi + lo, for the extra precision.
static const float rc_atan2_pi_f32 = 3.1415927410e+00f;     // 0x40490fdb
static const float rc_atan2_pi_lo_f32 = -8.7422776573e-08f; // 0xb3bbbd2e


// The angle, negated when y is negative.
static float rc_atan2_signed_f32(float angle, bool y_negative)
{
    return y_negative ? -angle : angle;
}


// The angle when x is infinite: +-pi/4 or +-3pi/4 if y is too, and otherwise +-0 or +-pi.
static float rc_atan2_infinite_x_f32(uint32_t y_magnitude, bool x_negative, bool y_negative)
{
    float pi = rc_atan2_pi_f32;

    if (y_magnitude == 0x7f800000u) {
        return rc_atan2_signed_f32(x_negative ? 3.0f * pi / 4.0f : pi / 4.0f, y_negative);
    }

    return rc_atan2_signed_f32(x_negative ? pi : 0.0f, y_negative);
}


// True if y is infinite or |y / x| > 2^26, so the angle is +-pi/2 to float precision.
static bool rc_atan2_is_steep_f32(uint32_t x_magnitude, uint32_t y_magnitude)
{
    return x_magnitude + (26u << 23) < y_magnitude || y_magnitude == 0x7f800000u;
}


// atan(|y / x|), taken as 0 when x is negative and |y / x| < 2^-26, where it vanishes beside pi.
static float rc_atan2_slope_f32(float y, float x, uint32_t x_magnitude, uint32_t y_magnitude)
{
    bool x_negative = x < 0.0f;
    bool tiny = y_magnitude + (26u << 23) < x_magnitude;

    if (x_negative && tiny) {
        return 0.0f;
    }

    return rc_atan_f32(rc_abs_f32(y / x));
}


// The angle from z = atan(|y / x|), moved into the quadrant of (x, y).
static float rc_atan2_from_slope_f32(float z, bool x_negative, bool y_negative)
{
    if (!x_negative) {
        return y_negative ? -z : z;
    }

    if (y_negative) {
        return (z - rc_atan2_pi_lo_f32) - rc_atan2_pi_f32;
    }

    return rc_atan2_pi_f32 - (z - rc_atan2_pi_lo_f32);
}


float rc_atan2_f32(float y, float x)
{
    uint32_t x_bits = rc_bitcast_f32(x);
    uint32_t y_bits = rc_bitcast_f32(y);
    uint32_t x_magnitude = x_bits & 0x7fffffffu;
    uint32_t y_magnitude = y_bits & 0x7fffffffu;
    bool x_negative = (x_bits >> 31) != 0;
    bool y_negative = (y_bits >> 31) != 0;

    // NaN in either gives NaN, and x = 1 is plain atan
    if (x != x || y != y) {
        return x + y;
    }

    if (x_bits == 0x3f800000u) {
        return rc_atan_f32(y);
    }

    // On the x axis, +-0 to the right and +-pi to the left
    if (y_magnitude == 0) {
        return x_negative ? rc_atan2_signed_f32(rc_atan2_pi_f32, y_negative) : y;
    }

    // On the y axis, +-pi/2
    if (x_magnitude == 0) {
        return rc_atan2_signed_f32(rc_atan2_pi_f32 / 2.0f, y_negative);
    }

    if (x_magnitude == 0x7f800000u) {
        return rc_atan2_infinite_x_f32(y_magnitude, x_negative, y_negative);
    }

    if (rc_atan2_is_steep_f32(x_magnitude, y_magnitude)) {
        return rc_atan2_signed_f32(rc_atan2_pi_f32 / 2.0f, y_negative);
    }

    return rc_atan2_from_slope_f32(rc_atan2_slope_f32(y, x, x_magnitude, y_magnitude), x_negative, y_negative);
}
