// math/sin_f32.c
//
// The sine of a float, worked in double. Below 2^-12 it is x, zero and subnormals included. Below
// 2^20 we reduce x to r = x - k pi/2 within pi/4 with a two-piece Cody-Waite (which leaves x below
// pi/4 as it is), and the kernel takes r and the quadrant k, whose parity picks sine's polynomial
// or cosine's and whose second bit the sign, all without a branch (see trig.h). Larger arguments
// take Payne-Hanek, inline, on |x|, and for a negative x add 2 to the quadrant; infinity and NaN
// give a NaN. Every step is odd in x, so sine is exactly odd.
//
// Error: the reduction keeps r to a relative 2^-39 and the polynomials are within 2^-37.5 (sine)
// and 2^-33.9 (cosine), so the double is within 2^-33.8 of the truth, and rounding it to a float
// lands within 0.502 ulp. We measured 0.501.

#include "richc/math/math.h"

#include "math/trig.h"

// sin x for a finite |x| of 2^20 or more: sin |x| for a positive x, and for a negative one
// sin(|x| + pi), which the kernel gives as exactly -sin |x|.
static float rc_sin_huge_f32(float x)
{
    rc_trig_reduced_f32 reduced = rc_trig_reduce_huge_f32(rc_abs_f32(x));

    return (float)rc_trig_sin_quadrant_f32(reduced.r, reduced.quadrant + ((rc_bitcast_f32(x) >> 30) & 2u));
}


// sin x for |x| from 2^-12 to below 2^20.
static float rc_sin_short_f32(float x)
{
    rc_trig_reduced_f32 reduced = rc_trig_reduce_short_f32((double)x);

    return (float)rc_trig_sin_quadrant_f32(reduced.r, reduced.quadrant);
}


float rc_sin_f32(float x)
{
    if (rc_trig_is_below_f32(x, RC_TRIG_TINY_BITS_F32)) {
        return x;
    }

    if (rc_trig_is_below_f32(x, RC_TRIG_SHORT_LIMIT_BITS_F32)) {
        return rc_sin_short_f32(x);
    }

    return rc_is_finite_f32(x) ? rc_sin_huge_f32(x) : x - x;
}
