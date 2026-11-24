// math/trig_slow.c
//
// The slow paths of the doubles' sin, cos and tan, out of line so the fast ones stay lean:
// arguments of 2^20 or more, which Payne-Hanek reduces (trig_reduce.c) before the kernel the fast
// path uses, and infinity and NaN, which give a NaN at once.

#include "math/trig.h"

// sin(x + offset pi/2) for a finite x of at least 2^20.
static double rc_trig_sin_huge(double x, uint32_t offset)
{
    rc_trig_reduced reduced = rc_trig_reduce_huge(x);

    return rc_trig_sin_quadrant(reduced.r, reduced.quadrant + offset);
}


// tan x for the same x.
static double rc_trig_tan_huge(double x)
{
    rc_trig_reduced reduced = rc_trig_reduce_huge(x);

    return rc_trig_tan_quadrant(reduced.r, reduced.quadrant);
}


double rc_trig_sin_slow(double x, uint32_t offset)
{
    return rc_is_finite_f64(x) ? rc_trig_sin_huge(x, offset) : x - x;
}


double rc_trig_tan_slow(double x)
{
    return rc_is_finite_f64(x) ? rc_trig_tan_huge(x) : x - x;
}
