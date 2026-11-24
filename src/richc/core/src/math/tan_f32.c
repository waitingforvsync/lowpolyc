// math/tan_f32.c
//
// The tangent of a float, worked in double, as a rational function: tan r = r N(z) / D(z) with
// z = r^2 and N and D quadratics (see trig.h). Below 2^-12 it is x, zero and subnormals included.
// Below 2^20 we reduce x to r = x - k pi/2 within pi/4 with a two-piece Cody-Waite (which leaves x
// below pi/4 as it is), and the kernel gives N / D for an even k and -D / N for an odd one, without
// a branch. Larger arguments take Payne-Hanek, inline, on |x|, with the sign put back; infinity
// and NaN give a NaN. Every step is odd in x, so tangent is exactly odd.
//
// Error: r is good to a relative 2^-39 and the rational to 2^-35.2, so the quotient is within
// 2^-34.9 of the truth, and rounding it to a float lands within 0.501 ulp. We measured 0.500.

#include "richc/math/math.h"

#include "math/trig.h"

// tan x for a finite |x| of 2^20 or more, worked on |x| with the sign put back.
static float rc_tan_huge_f32(float x)
{
    uint64_t sign = (uint64_t)(rc_bitcast_f32(x) & 0x80000000u) << 32;
    rc_trig_reduced_f32 reduced = rc_trig_reduce_huge_f32(rc_abs_f32(x));
    double value = rc_trig_tan_quadrant_f32(reduced.r, reduced.quadrant);

    return (float)rc_bitcast_u64(rc_bitcast_f64(value) ^ sign);
}


// tan x for |x| from 2^-12 to below 2^20.
static float rc_tan_short_f32(float x)
{
    rc_trig_reduced_f32 reduced = rc_trig_reduce_short_f32((double)x);

    return (float)rc_trig_tan_quadrant_f32(reduced.r, reduced.quadrant);
}


float rc_tan_f32(float x)
{
    if (rc_trig_is_below_f32(x, RC_TRIG_TINY_BITS_F32)) {
        return x;
    }

    if (rc_trig_is_below_f32(x, RC_TRIG_SHORT_LIMIT_BITS_F32)) {
        return rc_tan_short_f32(x);
    }

    return rc_is_finite_f32(x) ? rc_tan_huge_f32(x) : x - x;
}
