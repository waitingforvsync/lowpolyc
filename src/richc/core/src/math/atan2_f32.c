// math/atan2_f32.c
//
// rc_atan2_f32: the angle of the point (x, y) from the +x axis, in floats, worked in double. We
// work on |y| and give the result y's sign, so atan2(-y, x) is exactly -atan2(y, x).
//
// The angle from the nearer axis is atan(r) for r, the smaller of |x| and |y| over the larger, in
// [0, 1]. Up to tan(pi/8), that is atan(u) for u = r, and beyond, pi/4 + atan(u) for
// u = (near - far) / (near + far). Either way |u| is at most tan(pi/8), but for a rounding, and u
// costs one rounding, as the sum and difference of two floats so close are exact in double.
// atan(u) = u (1 + u^2 Q(u^2)) for Q a quartic, fitted to 2^-30.5, as atan_f32 has it. The octant
// turns that into the result: 0 + a, pi/2 - a, pi - a or pi/2 + a, and the reduction adds pi/4
// for u beyond tan(pi/8). Rounding that double result to float adds little to the half ulp the
// rounding itself costs: about 0.51 ulp at worst. Below 2^-15 in size, atan(u) is u to a relative
// 2^-31.6, and we skip the polynomial; the test for that catches a NaN u too, from 0/0 and inf/inf,
// which take the special cases. The numerator caps far at 2^200 before the shift of 0 or 1
// multiplies it, so an infinite far doesn't make a NaN there. The octant's table holds y's sign as
// well, so no copysign is needed, and we read the signs off the doubles, not the floats.

#include "richc/math/math.h"

#include "math/arc.h"

// The angle the octant and the reduction add to, and the sign the octant gives atan(u).
typedef struct rc_atan2_octant {
    double offset;
    double sign;
} rc_atan2_octant;

// By y's sign bit, then x's, then whether |y| > |x|, then whether the reduction is past tan(pi/8).
// y's sign is in the offset and sign too.
static const rc_atan2_octant rc_atan2_octants[16] = {
    {.offset = 0.0, .sign = 1.0},
    {.offset = RC_ARC_PI_4, .sign = 1.0},
    {.offset = RC_ARC_PI_2_HI, .sign = -1.0},
    {.offset = RC_ARC_PI_4, .sign = -1.0},
    {.offset = RC_ARC_PI_HI, .sign = -1.0},
    {.offset = RC_ARC_PI_3_4, .sign = -1.0},
    {.offset = RC_ARC_PI_2_HI, .sign = 1.0},
    {.offset = RC_ARC_PI_3_4, .sign = 1.0},
    {.offset = -0.0, .sign = -1.0},
    {.offset = -RC_ARC_PI_4, .sign = -1.0},
    {.offset = -RC_ARC_PI_2_HI, .sign = 1.0},
    {.offset = -RC_ARC_PI_4, .sign = 1.0},
    {.offset = -RC_ARC_PI_HI, .sign = 1.0},
    {.offset = -RC_ARC_PI_3_4, .sign = 1.0},
    {.offset = -RC_ARC_PI_2_HI, .sign = -1.0},
    {.offset = -RC_ARC_PI_3_4, .sign = -1.0}
};


// atan2 where u is NaN, from y and x as doubles: a NaN, both zero, or both infinite.
static float rc_atan2_special(double y, double x)
{
    bool x_negative = (rc_bitcast_f64(x) >> 63) != 0;

    if (y != y || x != x) {
        return (float)(x + y);
    }

    if (y == 0.0) {
        return (float)rc_copysign_f64(x_negative ? RC_ARC_PI_HI : 0.0, y);
    }

    return (float)rc_copysign_f64(x_negative ? RC_ARC_PI_3_4 : RC_ARC_PI_4, y);
}


// The result from u, by the octant's index.
static double rc_atan2_in_octant(uint64_t index, double u)
{
    rc_atan2_octant octant = rc_atan2_octants[index];

    return (octant.offset + octant.sign * u) + octant.sign * (u * rc_arc_atan_ratio_f32(u));
}


// Whether |u| is at least 2^-15, which is false for NaN.
static bool rc_atan2_is_ordinary(double u)
{
    return rc_abs_f64(u) >= 0x1p-15;
}


// atan2 where |u| is below 2^-15, so that atan(u) is u to a relative 2^-31.6, or NaN.
static float rc_atan2_tiny(double y, double x, uint64_t index, double u)
{
    rc_atan2_octant octant = rc_atan2_octants[index];

    if (u != u) {
        return rc_atan2_special(y, x);
    }

    return (float)(octant.offset + octant.sign * u);
}


float rc_atan2_f32(float y, float x)
{
    double dy = (double)y;
    double dx = (double)x;
    double ay = rc_abs_f64(dy);
    double ax = rc_abs_f64(dx);
    bool steep = ay > ax;
    double near = steep ? ax : ay;
    double far = steep ? ay : ax;
    bool diagonal = near > far * RC_ARC_TAN_PI_8;
    double shift = diagonal ? 1.0 : 0.0;
    double u = (near - rc_min_f64(0x1p200, far) * shift) / (far + near * shift);
    uint64_t index = (rc_bitcast_f64(dy) >> 63) * 8 + (rc_bitcast_f64(dx) >> 63) * 4 + steep * 2 + diagonal;

    if (!rc_atan2_is_ordinary(u)) {
        return rc_atan2_tiny(dy, dx, index, u);
    }

    return (float)rc_atan2_in_octant(index, u);
}
