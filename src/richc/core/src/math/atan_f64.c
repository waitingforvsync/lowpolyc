// math/atan_f64.c
//
// rc_atan_f64: the arctangent of a double. We work on |x| and give the result x's sign, so
// atan(-x) is exactly -atan(x).
//
// From 1/16 to 16, x sits in a cell of the atan table, a quarter of a binade, whose middle c gives
// atan(x) = atan(c) + atan(u), u = (x - c) / (1 + x c), with |u| < 1/16. x - c is exact, so u is
// out by only a few roundings in its last place, and u is small beside atan(c), which the table
// holds to twice double precision. Below 1/16 we take c = 0, so u is x itself; beyond 16, the
// angle is pi/2 + atan(-1/x). atan(u) is u plus u^3 times a quartic in u^2, fitted to 2^-62.5, so
// most of the error is the final rounding: 0.73 ulp at worst in testing.
//
// The ranges branch on the top 14 bits of |x|, the table's index. Below 2^-27 and from 2^54 up,
// tested only off the table, x takes a shortcut, which keeps subnormals out of the polynomial:
// there atan(x) rounds to x, and to pi/2's first double with x's sign. The sign goes on as a
// product with 1 or -1.

#include "richc/math/math.h"

#include "math/arc.h"

// base + atan(u), for |u| < 1/16, with x's sign.
static double rc_atan_finish(double x, rc_arc_sum base, double u)
{
    return (base.hi + (u + (base.lo + rc_arc_atan_tail_f64(u)))) * rc_arc_sign_f64(rc_bitcast_f64(x) >> 63);
}


// atan(x) for t = |x| beyond 16, infinity and NaN included: pi/2 + atan(-1/t). From 2^54 up, that
// is pi/2 - 1/t, which rounds to pi/2's first double: pi/2's second double less 1/t is between 0.02
// and 0.28 of its ulp.
static double rc_atan_beyond(double x, double t)
{
    if (t >= 0x1p54) {
        return RC_ARC_PI_2_HI * rc_arc_sign_f64(rc_bitcast_f64(x) >> 63);
    }

    return rc_atan_finish(x, (rc_arc_sum) {.hi = RC_ARC_PI_2_HI, .lo = RC_ARC_PI_2_LO}, -1.0 / t);
}


// atan(x) for t = |x| below 1/16: atan(t) itself, and below 2^-27, x.
static double rc_atan_below(double x, double t)
{
    if (t < 0x1p-27) {
        return x;
    }

    return rc_atan_finish(x, (rc_arc_sum) {0}, t);
}


// atan(x) for t = |x| from 1/16 to 16, through the cell of the table that the top 14 bits of t
// pick, whose middle c has those bits and then a 1.
static double rc_atan_by_table(double x, double t, uint64_t cell)
{
    rc_arc_sum base = rc_arc_atan_table[cell - RC_ARC_ATAN_TABLE_FIRST];
    double c = rc_arc_atan_cell_middle(cell);

    return rc_atan_finish(x, base, (t - c) / (1.0 + t * c));
}


double rc_atan_f64(double x)
{
    double t = rc_abs_f64(x);
    uint64_t cell = rc_bitcast_f64(t) >> RC_ARC_ATAN_TABLE_SHIFT;

    if (cell > RC_ARC_ATAN_TABLE_LAST) {
        return rc_atan_beyond(x, t);
    }

    if (cell < RC_ARC_ATAN_TABLE_FIRST) {
        return rc_atan_below(x, t);
    }

    return rc_atan_by_table(x, t, cell);
}
