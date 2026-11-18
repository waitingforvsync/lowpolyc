// math/trig_reduce_f32.c
//
// Reducing a float by the nearest multiple of pi/2, in double precision. Ported from musl 1.2.5
// (MIT), src/math/__rem_pio2f.c, which is FreeBSD's e_rem_pio2f.c. Its notice:
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

#include "trig.h"

// Adding and then subtracting this rounds a double below 2^51 to a whole number.
static const double to_int = 1.5 / DBL_EPSILON;

// pi/4 rounded to a float, which is a hair above it.
static const double pio4 = 0x1.921fb6p-1;

// 53 bits of 2/pi.
static const double inv_pio2 = 6.36619772367581382433e-01;

// The first 25 bits of pi/2, and pi/2 less them.
static const double pio2_1 = 1.57079631090164184570e+00;
static const double pio2_1t = 1.58932547735281966916e-08;

// x less multiple * pi/2. 25 + 53 bits of pi/2 are enough for a medium x.
static rc_trig_reduced_f32 rc_trig_reduce_by_f32(float x, double multiple)
{
    return (rc_trig_reduced_f32) {
        .quadrant = (int32_t)multiple,
        .angle = (double)x - multiple * pio2_1 - multiple * pio2_1t
    };
}


// |x| ~< 2^28 pi/2. The nearest multiple is only ever off by one in a directed rounding mode, but
// we check all the same.
static rc_trig_reduced_f32 rc_trig_reduce_medium_f32(float x)
{
    double multiple = (double)x * inv_pio2 + to_int - to_int;
    rc_trig_reduced_f32 reduced = rc_trig_reduce_by_f32(x, multiple);

    if (reduced.angle < -pio4) {
        return rc_trig_reduce_by_f32(x, multiple - 1.0);
    }

    if (reduced.angle > pio4) {
        return rc_trig_reduce_by_f32(x, multiple + 1.0);
    }

    return reduced;
}


// Larger x, given its bits, scaled into [2^23, 2^24) to be one whole 24-bit part.
static rc_trig_reduced_f32 rc_trig_reduce_large_f32(uint32_t bits)
{
    uint32_t abs_bits = bits & 0x7fffffffu;
    uint32_t exponent = (abs_bits >> 23) - (0x7fu + 23u);
    double part = (double)rc_bitcast_u32(abs_bits - (exponent << 23));
    rc_trig_reduced_f64 reduced = rc_trig_reduce_large(&part, 1, (int32_t)exponent, false);
    bool negative = (bits >> 31) != 0;

    return (rc_trig_reduced_f32) {
        .quadrant = negative ? -reduced.quadrant : reduced.quadrant,
        .angle = negative ? -reduced.hi : reduced.hi
    };
}


rc_trig_reduced_f32 rc_trig_reduce_f32(float x)
{
    uint32_t bits = rc_bitcast_f32(x);
    uint32_t abs_bits = bits & 0x7fffffffu;

    if (abs_bits < 0x4dc90fdbu) {
        return rc_trig_reduce_medium_f32(x);
    }

    // Infinity or NaN
    if (abs_bits >= 0x7f800000u) {
        return (rc_trig_reduced_f32) {.quadrant = 0, .angle = (double)(x - x)};
    }

    return rc_trig_reduce_large_f32(bits);
}
