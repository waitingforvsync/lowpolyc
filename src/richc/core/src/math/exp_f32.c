// math/exp_f32.c
//
// e to the power of a float. Ported from musl 1.2.5 (MIT), src/math/expf.c, which is Arm's
// optimized-routines. Its notice:
//
// Copyright (c) 2017-2018, Arm Limited.
// SPDX-License-Identifier: MIT
//
// The error is at most 0.502 ulp. Before rounding, the relative error of the cubic is 1.69 x 2^-34
// on [-ln2/64, ln2/64].

#include "exp_log_f32.h"

// The sign, the exponent and the top 3 bits of the mantissa of x.
static uint32_t rc_exp_top12_f32(float x)
{
    return rc_bitcast_f32(x) >> 20;
}


// e^x for x in [-0x1.9fe368p6, 0x1.62e42ep6], from log(2^-150) to log(2^128). We write
// z = 32 x / ln2 as k + r, with k whole and r in [-1/2, 1/2]: adding the shift rounds z to k, ties
// to even, and leaves k in the low bits. Then e^x is 2^(k/32) 2^(r/32).
static float rc_exp_in_range_f32(float x)
{
    double z = rc_exp_data_f32.inv_ln2_scaled * (double)x;
    double shifted = z + rc_exp_data_f32.shift;
    double k = shifted - rc_exp_data_f32.shift;

    return rc_exp_from_reduced_f32(rc_bitcast_f64(shifted), 0, z - k, rc_exp_data_f32.poly_scaled);
}


float rc_exp_f32(float x)
{
    uint32_t abs_top = rc_exp_top12_f32(x) & 0x7ffu;

    if (abs_top < rc_exp_top12_f32(88.0f)) {
        return rc_exp_in_range_f32(x);
    }

    // |x| >= 88, or x is infinite or NaN
    if (rc_bitcast_f32(x) == 0xff800000u) {
        return 0.0f;
    }

    // +infinity stays as it is, and NaN stays NaN
    if (abs_top >= 0x7f8u) {
        return x + x;
    }

    if (x > 0x1.62e42ep6f) {
        return rc_exp_overflow_f32(0);
    }

    if (x < -0x1.9fe368p6f) {
        return rc_exp_underflow_f32(0);
    }

    return rc_exp_in_range_f32(x);
}
