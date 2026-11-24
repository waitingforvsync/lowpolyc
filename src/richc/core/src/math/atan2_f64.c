// math/atan2_f64.c
//
// rc_atan2_f64: the angle of the point (x, y) from the +x axis, in doubles. We work on |y| and give
// the result y's sign, so atan2(-y, x) is exactly -atan2(y, x).
//
// The smaller of |x| and |y| over the larger, near / far, is a ratio in [0, 1] whose arctangent a
// is the angle from the nearer axis, and the octant makes that a, pi/2 - a, pi - a or pi/2 + a.
// From 1/16 up, the rounded ratio picks a cell of the atan table, a quarter of a binade, with
// middle c, and a = atan(c) + atan(u) for u = (near - c far) / (far + c near). That comes from near
// and far themselves, so the ratio's rounding never enters, and splitting far makes c far exact,
// so the numerator, which cancels, is exact but for one rounding. Below 1/16, u is the ratio, and
// we add back what its rounding lost, but below 2^-30 the ratio alone will do. atan(u) is u plus
// u^3 times a quartic in u^2, fitted to 2^-62.5. Every sum is exact but the last: 0.66 ulp at worst
// in testing.
//
// The ranges branch: working out each and choosing costs more than the odd misprediction. The test
// for the table's range catches a NaN ratio too, from 0/0 and inf/inf, which take the special
// cases. The octant's table holds y's sign as well, so no copysign is needed.

#include "richc/math/math.h"

#include "math/arc.h"

// The angle the octant adds to, and the sign it gives the angle from the nearer axis.
typedef struct rc_atan2_octant {
    rc_arc_sum offset;
    double sign;
} rc_atan2_octant;

// The two sides of the angle: the smaller of |x| and |y| and the larger.
typedef struct rc_atan2_sides {
    double near;
    double far;
} rc_atan2_sides;

// The angle from the nearer axis as base + u + correction: an arctangent from the table, or 0, to
// twice double precision, then u, then a correction much smaller than u.
typedef struct rc_atan2_parts {
    rc_arc_sum base;
    double u;
    double correction;
} rc_atan2_parts;

// By y's sign bit, then x's, then whether |y| > |x|: y's sign is in the offset and sign too.
static const rc_atan2_octant rc_atan2_octants[8] = {
    {.offset = {.hi = 0.0, .lo = 0.0}, .sign = 1.0},
    {.offset = {.hi = RC_ARC_PI_2_HI, .lo = RC_ARC_PI_2_LO}, .sign = -1.0},
    {.offset = {.hi = RC_ARC_PI_HI, .lo = RC_ARC_PI_LO}, .sign = -1.0},
    {.offset = {.hi = RC_ARC_PI_2_HI, .lo = RC_ARC_PI_2_LO}, .sign = 1.0},
    {.offset = {.hi = -0.0, .lo = -0.0}, .sign = -1.0},
    {.offset = {.hi = -RC_ARC_PI_2_HI, .lo = -RC_ARC_PI_2_LO}, .sign = 1.0},
    {.offset = {.hi = -RC_ARC_PI_HI, .lo = -RC_ARC_PI_LO}, .sign = 1.0},
    {.offset = {.hi = -RC_ARC_PI_2_HI, .lo = -RC_ARC_PI_2_LO}, .sign = -1.0}
};


// atan2 where |y| / |x| is NaN: a NaN, both zero, or both infinite.
static double rc_atan2_special(double y, double x)
{
    bool x_negative = (rc_bitcast_f64(x) >> 63) != 0;

    if (y != y || x != x) {
        return x + y;
    }

    if (y == 0.0) {
        return rc_copysign_f64(x_negative ? RC_ARC_PI_HI : 0.0, y);
    }

    return rc_copysign_f64(x_negative ? RC_ARC_PI_3_4 : RC_ARC_PI_4, y);
}


// near and far, both scaled by 2^-512 if far is beyond 2^1000 and by 2^512 if below 2^-900.
static rc_atan2_sides rc_atan2_rescaled(rc_atan2_sides sides)
{
    double scale = sides.far > 1.0 ? 0x1p-512 : 0x1p512;

    return (rc_atan2_sides) {.near = sides.near * scale, .far = sides.far * scale};
}


// Whether far is from 2^-900 to 2^1000, where no product of near and far overflows or underflows.
// We test its bits, in one go.
static bool rc_atan2_is_moderate(double far)
{
    return rc_bitcast_f64(far) - 0x07b0000000000000ull <= 0x7e70000000000000ull - 0x07b0000000000000ull;
}


// near and far, scaled if need be so that no product of theirs overflows or underflows.
static rc_atan2_sides rc_atan2_scaled(rc_atan2_sides sides)
{
    if (!rc_atan2_is_moderate(sides.far)) {
        return rc_atan2_rescaled(sides);
    }

    return sides;
}


// The arctangent of near / far from 1/16 up, through the cell of the table that the rounded ratio
// picks, with middle c: atan(c) + atan(u) for u = (near - c far) / (far + c near). c has 4
// significant bits, so with the low 4 cut off far, c far_hi is exact, and so is near - c far_hi,
// the two being so close, which leaves u a few roundings in its last place.
static rc_atan2_parts rc_atan2_by_table(rc_atan2_sides sides, double ratio)
{
    uint64_t cell = rc_bitcast_f64(ratio) >> RC_ARC_ATAN_TABLE_SHIFT;
    const rc_arc_sum *entry = &rc_arc_atan_table[cell - RC_ARC_ATAN_TABLE_FIRST];
    double c = rc_arc_atan_cell_middle(cell);
    double far_hi = rc_bitcast_u64(rc_bitcast_f64(sides.far) & ~0xfull);
    double numerator = (sides.near - c * far_hi) - c * (sides.far - far_hi);
    double u = numerator / (sides.far + c * sides.near);

    return (rc_atan2_parts) {
        .base = {.hi = entry->hi, .lo = entry->lo},
        .u = u,
        .correction = rc_arc_atan_tail_f64(u)
    };
}


// The arctangent of near / far from 2^-30 to 1/16: the ratio, what its rounding lost,
// (near - ratio far) / far, and atan(u) - u.
static rc_atan2_parts rc_atan2_small(rc_atan2_sides sides, double ratio)
{
    double residual = rc_arc_residual_f64(sides.near, ratio, sides.far);

    return (rc_atan2_parts) {.u = ratio, .correction = residual / sides.far + rc_arc_atan_tail_f64(ratio)};
}


// The arctangent of near / far below 1/16. Below 2^-30, the rounded ratio alone will do: atan's
// cube is too small to move the rounding, and near could underflow and spoil the ratio's error.
static rc_atan2_parts rc_atan2_below(rc_atan2_sides sides, double ratio)
{
    if (ratio < 0x1p-30) {
        return (rc_atan2_parts) {.u = ratio};
    }

    return rc_atan2_small(rc_atan2_scaled(sides), ratio);
}


// The result from the angle from the nearer axis, by the octant: the signs of y and x, and steep if
// |y| > |x|. The octant's offset and the angle's base come together first, both exactly, which
// can be under way while u is still being divided out.
static double rc_atan2_in_octant(double y, double x, bool steep, rc_atan2_parts angle)
{
    rc_atan2_octant octant = rc_atan2_octants[(rc_bitcast_f64(y) >> 63) * 4 + (rc_bitcast_f64(x) >> 63) * 2 + steep];
    rc_arc_sum base = rc_arc_fast_two_sum(octant.offset.hi, octant.sign * angle.base.hi);
    rc_arc_sum sum = rc_arc_fast_two_sum(base.hi, octant.sign * angle.u);
    double lo = (octant.offset.lo + base.lo) + octant.sign * (angle.base.lo + angle.correction);

    return sum.hi + (sum.lo + lo);
}


// Whether the ratio is at least 1/16, where the table takes it, which is false for NaN.
static bool rc_atan2_is_tabled(double ratio)
{
    return ratio >= 0x1p-4;
}


// atan2 where the ratio is below 1/16, or NaN: a NaN, both zero, or both infinite.
static double rc_atan2_untabled(double y, double x, bool steep, rc_atan2_sides sides, double ratio)
{
    if (ratio != ratio) {
        return rc_atan2_special(y, x);
    }

    return rc_atan2_in_octant(y, x, steep, rc_atan2_below(sides, ratio));
}


double rc_atan2_f64(double y, double x)
{
    double ay = rc_abs_f64(y);
    double ax = rc_abs_f64(x);
    bool steep = ay > ax;
    rc_atan2_sides sides = {.near = steep ? ax : ay, .far = steep ? ay : ax};
    double ratio = sides.near / sides.far;

    if (!rc_atan2_is_tabled(ratio)) {
        return rc_atan2_untabled(y, x, steep, sides, ratio);
    }

    return rc_atan2_in_octant(y, x, steep, rc_atan2_by_table(rc_atan2_scaled(sides), ratio));
}
