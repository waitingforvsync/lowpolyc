// math/cos_f64.c
//
// The cosine of a double: the sine kernel a quadrant on. Below 2^-27 it is 1; below pi/4 the
// kernel takes x as it is; below 2^20 we reduce x to r = x - k pi/2, a double-double within pi/4,
// by Cody-Waite, and the kernel takes r and k + 1, whose parity picks cosine's polynomial or sine's
// and whose second bit the sign, all without a branch (see trig.h). Larger arguments, infinity and
// NaN go to the slow path. Every step is odd or even in x as it should be, so cosine is exactly
// even.
//
// Error: the reduction is good to a relative 2^-64 and the polynomials to 2^-63. Cosine's tail,
// past an exact 1 - r^2/2, is at most 0.016 of it, which keeps it within 0.61 ulp, and sine's
// r^3/6 rounds twice before joining r exactly, which keeps that within 0.67 (see trig.h): so cos x
// is within 0.67 ulp. We measured 0.64.

#include "richc/math/math.h"

#include "math/trig.h"

// cos x for |x| from pi/4 to below 2^20.
static double rc_cos_short(double x)
{
    rc_trig_reduced reduced = rc_trig_reduce_short(x);

    return rc_trig_sin_quadrant(reduced.r, reduced.quadrant + 1u);
}


double rc_cos_f64(double x)
{
    if (rc_trig_is_below(x, RC_TRIG_TINY_BITS_F64)) {
        return 1.0;
    }

    if (rc_trig_is_below(x, RC_TRIG_QUARTER_PI_BITS_F64)) {
        return rc_trig_sin_quadrant((rc_trig_dd) {.hi = x, .lo = 0.0}, 1);
    }

    return rc_trig_is_below(x, RC_TRIG_SHORT_LIMIT_BITS_F64) ? rc_cos_short(x) : rc_trig_sin_slow(rc_abs_f64(x), 1);
}
