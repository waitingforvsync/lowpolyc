// math/sin_f64.c
//
// The sine of a double. Below 2^-26 it is x, zero and subnormals included; below pi/4 the kernel
// takes x as it is; below 2^20 we reduce x to r = x - k pi/2, a double-double within pi/4, by
// Cody-Waite, and the kernel takes r and the quadrant k, whose parity picks sine's polynomial or
// cosine's and whose second bit the sign, all without a branch (see trig.h). Larger arguments,
// infinity and NaN go to the slow path, which works on |x| and for a negative x adds 2 to the
// quadrant. Every step is odd in x, so sine is exactly odd.
//
// Error: the reduction is good to a relative 2^-64 and the polynomials to 2^-63. Sine's r^3/6
// rounds twice before joining r exactly, which keeps it within 0.67 ulp, and cosine's tail, past
// an exact 1 - r^2/2, is at most 0.016 of it, which keeps that within 0.61 (see trig.h): so sin x
// is within 0.67 ulp. We measured 0.64.

#include "richc/math/math.h"

#include "math/trig.h"

// sin x for |x| of 2^20 or more, infinity or NaN: sin |x| for a positive x, and for a negative
// one sin(|x| + pi), which the kernel gives as exactly -sin |x|.
static double rc_sin_slow(double x)
{
    return rc_trig_sin_slow(rc_abs_f64(x), (uint32_t)(rc_bitcast_f64(x) >> 62) & 2u);
}


// sin x for |x| from pi/4 to below 2^20.
static double rc_sin_short(double x)
{
    rc_trig_reduced reduced = rc_trig_reduce_short(x);

    return rc_trig_sin_quadrant(reduced.r, reduced.quadrant);
}


double rc_sin_f64(double x)
{
    if (rc_trig_is_below(x, RC_TRIG_SIN_TINY_BITS_F64)) {
        return x;
    }

    if (rc_trig_is_below(x, RC_TRIG_QUARTER_PI_BITS_F64)) {
        return rc_trig_sin_quadrant((rc_trig_dd) {.hi = x, .lo = 0.0}, 0);
    }

    return rc_trig_is_below(x, RC_TRIG_SHORT_LIMIT_BITS_F64) ? rc_sin_short(x) : rc_sin_slow(x);
}
