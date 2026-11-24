// math/exp_f64.c
//
// rc_exp_f64: e to the power x. We write x = (128 k + j) ln2 / 128 + r, |r| <= ln2 / 256, so that
// e^x = 2^k 2^(j/128) e^r, with 2^(j/128) from a table of 128 as hi (1 + lo) and e^r - 1 from a
// degree 5 polynomial within 2^-63.5 (the kernel in exp_log_f64.h). x less n ln2 / 128's 35-bit
// head is exact, and only four errors are anything like the result's size, each under 2^-61.4 of
// it: two sums with that head, its product with 2^k hi, and the lo (e^r - 1) we leave out. Before
// the final rounding we are within 2^-59.4 of e^x, under 0.012 ulp, and the worst error we have
// found is 0.507 ulp. Below 700 in size the result and its 2^k are normal and come in one step;
// beyond, a slower last step, near overflow and among the subnormals, still rounds just once.

#include "richc/math/math.h"

#include "math/exp_log_f64.h"

// e^x for x NaN or at least rc_exp_f64_data.fast in size: NaN, infinity, zero, or the slower
// last step that rounds once.
static double rc_exp_f64_outside(double x)
{
    if (x != x) {
        return x + x;
    }

    if (x > RC_EXP_F64_OVERFLOW) {
        return rc_bitcast_u64(RC_EXP_LOG_F64_INFINITY_BITS);
    }

    if (x < RC_EXP_F64_UNDERFLOW) {
        return 0.0;
    }

    return rc_exp_f64_extreme(x, 0.0);
}


double rc_exp_f64(double x)
{
    if (!(rc_abs_f64(x) < rc_exp_f64_data.fast)) {
        return rc_exp_f64_outside(x);
    }

    return rc_exp_f64_kernel(x, 0.0, 0);
}
