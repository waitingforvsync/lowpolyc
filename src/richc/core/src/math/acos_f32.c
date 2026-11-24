// math/acos_f32.c
//
// rc_acos_f32: the arccosine of a float, worked in double.
//
// Up to 1/2 in size, acos(x) = pi/2 - asin(x), with asin(x) x plus x^3 times a quintic in x^2,
// fitted to 2^-31.8. Beyond, acos(x) = 2 asin(s) for x > 0 and pi - 2 asin(s) for x < 0, with
// s = sqrt((1 - |x|) / 2), which is at most 1/2 again, and 1/2 - |x|/2 is exact. Rounding that
// double result to float adds little to the half ulp the rounding itself costs: 0.505 ulp at worst
// over every float. Beyond 1, s and so the result are NaN.
//
// The two ranges branch, on |x|'s bits, which settles sooner than comparing the float: working out
// both and choosing costs more than the odd misprediction. Beyond 1/2, x's sign picks the offset
// and scale from a table, with no branch.

#include "richc/math/math.h"

#include "math/arc.h"

// What turns asin(s) into acos(x) for |x| > 1/2, by x's sign bit: 2 asin(s) for x > 0, and
// pi - 2 asin(s) for x < 0.
typedef struct rc_acos_side {
    double offset;
    double scale;
} rc_acos_side;

static const rc_acos_side rc_acos_sides[2] = {
    {.offset = 0.0, .scale = 2.0},
    {.offset = RC_ARC_PI_HI, .scale = -2.0}
};


// acos(x) for |x| > 1/2, given a = |x| and x's sign bit: NaN beyond 1, and for NaN.
static double rc_acos_near_one(double a, uint32_t sign)
{
    double z = 0.5 - 0.5 * a;
    double s = rc_sqrt_f64(z);
    rc_acos_side side = rc_acos_sides[sign];

    return side.offset + side.scale * (s + rc_arc_asin_tail_f32(s, z));
}


float rc_acos_f32(float x)
{
    uint32_t bits = rc_bitcast_f32(x);
    double d = (double)x;

    if ((bits & 0x7fffffffu) <= 0x3f000000u) {
        return (float)(RC_ARC_PI_2_HI - (d + rc_arc_asin_tail_f32(d, d * d)));
    }

    return (float)rc_acos_near_one(rc_abs_f64(d), bits >> 31);
}
