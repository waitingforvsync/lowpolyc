// math/exp_f64.c
//
// e to the power of a double. Ported from musl 1.2.5 (MIT), src/math/exp.c, which is Arm's
// optimized-routines. Its notice:
//
// Copyright (c) 2018, Arm Limited.
// SPDX-License-Identifier: MIT

#include "richc/math/math.h"

#include "exp_log_f64.h"

// exp of an infinity or NaN: 0 for -infinity, infinity for infinity, and NaN for NaN.
static double rc_exp_non_finite_f64(double x)
{
    if (rc_bitcast_f64(x) == 0xfff0000000000000ull) {
        return 0.0;
    }

    return 1.0 + x;
}


double rc_exp_f64(double x)
{
    if (!rc_is_finite_f64(x)) {
        return rc_exp_non_finite_f64(x);
    }

    return rc_exp_kernel_f64(x, 0.0, 0);
}
