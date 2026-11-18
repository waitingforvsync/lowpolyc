// math/trig_reduce_f64.c
//
// Reducing a double by the nearest multiple of pi/2, to a head and a tail. Ported from musl 1.2.5
// (MIT), src/math/__rem_pio2.c, which is FreeBSD's e_rem_pio2.c. Its notice:
//
// Optimized by Bruce D. Evans.
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunSoft, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.

#include "trig.h"

// A remainder in the middle of being worked out, as head - tail.
typedef struct rc_trig_remainder {
    double head;
    double tail;
} rc_trig_remainder;

// Adding and then subtracting this rounds a double below 2^51 to a whole number.
static const double to_int = 1.5 / DBL_EPSILON;

static const double pio4 = 0x1.921fb54442d18p-1;

// 53 bits of 2/pi.
static const double inv_pio2 = 6.36619772367581382433e-01;

// pi/2 in three pieces of 33 bits, pio2_1, pio2_2 and pio2_3, each followed by what is left of
// pi/2 after the pieces so far.
static const double pio2_1 = 1.57079632673412561417e+00;
static const double pio2_1t = 6.07710050650619224932e-11;
static const double pio2_2 = 6.07710050630396597660e-11;
static const double pio2_2t = 2.02226624879595063154e-21;
static const double pio2_3 = 2.02226624871116645580e-21;
static const double pio2_3t = 8.47842766036889956997e-32;

// x less multiple * pi/2 for a multiple up to 4 either way, where one round is good to 85 bits.
static rc_trig_reduced_f64 rc_trig_reduce_by_f64(double x, int32_t multiple)
{
    double scale = (double)multiple;
    double rough = x - scale * pio2_1;
    double hi = rough - scale * pio2_1t;

    return (rc_trig_reduced_f64) {
        .quadrant = multiple,
        .hi = hi,
        .lo = (rough - hi) - scale * pio2_1t
    };
}


// The first round of x less multiple * pi/2, good to 85 bits.
static rc_trig_remainder rc_trig_first_round(double x, double multiple)
{
    return (rc_trig_remainder) {.head = x - multiple * pio2_1, .tail = multiple * pio2_1t};
}


// Another round, taking the next piece of pi/2 into the head and the rest of pi/2 into the tail.
static rc_trig_remainder rc_trig_next_round(rc_trig_remainder previous, double multiple, double piece, double piece_tail)
{
    double taken = multiple * piece;
    double head = previous.head - taken;

    return (rc_trig_remainder) {.head = head, .tail = multiple * piece_tail - ((previous.head - head) - taken)};
}


// How many binary orders of magnitude the remainder is below x, whose biased exponent is given: the
// bits cancellation has cost us.
static int32_t rc_trig_bits_lost(rc_trig_remainder remainder, int32_t exponent)
{
    uint64_t bits = rc_bitcast_f64(remainder.head - remainder.tail);

    return exponent - (int32_t)((bits >> 52) & 0x7ffu);
}


// The multiple of pi/2 nearest x, as a double. It is only ever off by one in a directed rounding
// mode, but we check all the same.
static double rc_trig_nearest_multiple(double x)
{
    double multiple = x * inv_pio2 + to_int - to_int;
    rc_trig_remainder first = rc_trig_first_round(x, multiple);

    if (first.head - first.tail < -pio4) {
        return multiple - 1.0;
    }

    if (first.head - first.tail > pio4) {
        return multiple + 1.0;
    }

    return multiple;
}


// |x| ~< 2^20 pi/2, given the top word of |x|. Each round takes 33 more bits of pi/2, and we go on
// while cancellation has cost too many bits: two rounds are good to 118 bits, and three to 151,
// which covers every case.
static rc_trig_reduced_f64 rc_trig_reduce_medium_f64(double x, uint32_t top)
{
    int32_t exponent = (int32_t)(top >> 20);
    double multiple = rc_trig_nearest_multiple(x);
    rc_trig_remainder first = rc_trig_first_round(x, multiple);
    rc_trig_remainder second = rc_trig_bits_lost(first, exponent) > 16 ? rc_trig_next_round(first, multiple, pio2_2, pio2_2t) : first;
    rc_trig_remainder last = rc_trig_bits_lost(second, exponent) > 49 ? rc_trig_next_round(second, multiple, pio2_3, pio2_3t) : second;
    double hi = last.head - last.tail;

    return (rc_trig_reduced_f64) {
        .quadrant = (int32_t)multiple,
        .hi = hi,
        .lo = (last.head - hi) - last.tail
    };
}


// The multiple of pi/2 nearest x for |x| ~<= 9pi/4, given the top word of |x|, or 0 where x is so
// close to pi/2, pi, 3pi/2 or 2pi that one round would cancel too many bits.
static int32_t rc_trig_small_multiple(uint32_t top)
{
    // |x| ~= pi/2 or pi
    if (top <= 0x400f6a7au && (top & 0xfffffu) == 0x921fbu) {
        return 0;
    }

    // |x| ~<= 3pi/4
    if (top <= 0x4002d97cu) {
        return 1;
    }

    // |x| ~<= 5pi/4
    if (top <= 0x400f6a7au) {
        return 2;
    }

    // |x| ~<= 7pi/4, and ~= 3pi/2
    if (top <= 0x4015fdbcu) {
        return top == 0x4012d97cu ? 0 : 3;
    }

    // |x| ~= 2pi
    return top == 0x401921fbu ? 0 : 4;
}


// |x| ~<= 9pi/4, given the top word of |x|.
static rc_trig_reduced_f64 rc_trig_reduce_small_f64(double x, uint32_t top)
{
    int32_t multiple = rc_trig_small_multiple(top);
    bool negative = (rc_bitcast_f64(x) >> 63) != 0;

    if (multiple == 0) {
        return rc_trig_reduce_medium_f64(x, top);
    }

    return rc_trig_reduce_by_f64(x, negative ? -multiple : multiple);
}


// How many of the three parts to pass on: those up to the last nonzero one. The first never is.
static int32_t rc_trig_num_parts(const double *parts)
{
    if (parts[2] != 0.0) {
        return 3;
    }

    return parts[1] != 0.0 ? 2 : 1;
}


// Larger x, given the top word of |x|. |x| is scaled into [2^23, 2^24) and cut into three whole
// 24-bit parts.
static rc_trig_reduced_f64 rc_trig_reduce_large_f64(double x, uint32_t top)
{
    uint64_t bits = rc_bitcast_f64(x);
    double scaled = rc_bitcast_u64((bits & 0x000fffffffffffffull) | ((uint64_t)(0x3ff + 23) << 52));
    double part_0 = (double)(int32_t)scaled;
    double rest = (scaled - part_0) * 0x1p24;
    double part_1 = (double)(int32_t)rest;
    double parts[3] = {part_0, part_1, (rest - part_1) * 0x1p24};
    int32_t exponent = (int32_t)(top >> 20) - (0x3ff + 23);
    rc_trig_reduced_f64 reduced = rc_trig_reduce_large(parts, rc_trig_num_parts(parts), exponent, true);

    if ((bits >> 63) == 0) {
        return reduced;
    }

    return (rc_trig_reduced_f64) {
        .quadrant = -reduced.quadrant,
        .hi = -reduced.hi,
        .lo = -reduced.lo
    };
}


rc_trig_reduced_f64 rc_trig_reduce_f64(double x)
{
    uint32_t top = rc_trig_abs_high_word_f64(x);

    // |x| ~<= 9pi/4
    if (top <= 0x401c463bu) {
        return rc_trig_reduce_small_f64(x, top);
    }

    // |x| ~< 2^20 pi/2
    if (top < 0x413921fbu) {
        return rc_trig_reduce_medium_f64(x, top);
    }

    // Infinity or NaN
    if (top >= 0x7ff00000u) {
        return (rc_trig_reduced_f64) {.quadrant = 0, .hi = x - x, .lo = x - x};
    }

    return rc_trig_reduce_large_f64(x, top);
}
