// math/tan_f64.c
//
// The tangent of a double, as a rational function: tan r = N(r) / D(r), each built as a leading
// double and a small tail (see trig.h). Below 2^-27 it is x, zero and subnormals included; below
// pi/4 the kernel takes x as it is; below 2^20 we reduce x to r = x - k pi/2, a double-double
// within pi/4, by Cody-Waite, and the kernel gives N / D for an even k and -D / N for an odd one,
// without a branch. Larger arguments, infinity and NaN go to the slow path, which works on |x| and
// puts the sign back. Every step is odd in x, so tangent is exactly odd.
//
// Error: the reduction is good to a relative 2^-64 and the rational to 2^-62. N's r^3/6 rounds
// twice before joining r exactly, a relative 0.23 2^-53, D is within 0.03 2^-53, and correcting
// the quotient adds 0.06 2^-53 before the last rounding: within 0.82 ulp. We measured 0.77.

#include "richc/math/math.h"

#include "math/trig.h"

// tan x for |x| of 2^20 or more, infinity or NaN.
static double rc_tan_slow(double x)
{
    uint64_t sign = rc_bitcast_f64(x) & 0x8000000000000000ull;
    double value = rc_trig_tan_slow(rc_abs_f64(x));

    return rc_bitcast_u64(rc_bitcast_f64(value) ^ sign);
}


// tan x for |x| from pi/4 to below 2^20.
static double rc_tan_short(double x)
{
    rc_trig_reduced reduced = rc_trig_reduce_short(x);

    return rc_trig_tan_quadrant(reduced.r, reduced.quadrant);
}


double rc_tan_f64(double x)
{
    if (rc_trig_is_below(x, RC_TRIG_TINY_BITS_F64)) {
        return x;
    }

    if (rc_trig_is_below(x, RC_TRIG_QUARTER_PI_BITS_F64)) {
        return rc_trig_tan_quadrant((rc_trig_dd) {.hi = x, .lo = 0.0}, 0);
    }

    return rc_trig_is_below(x, RC_TRIG_SHORT_LIMIT_BITS_F64) ? rc_tan_short(x) : rc_tan_slow(x);
}
