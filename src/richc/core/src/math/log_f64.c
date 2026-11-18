// math/log_f64.c
//
// The natural logarithm of a double. Ported from musl 1.2.5 (MIT), src/math/log.c, which is Arm's
// optimized-routines. Its notice:
//
// Copyright (c) 2018, Arm Limited.
// SPDX-License-Identifier: MIT

#include "richc/math/math.h"

#include "exp_log_f64.h"

// The start of the range [offset, 2 offset) of z, which x's bits are reduced to by taking away k.
#define RC_LOG_OFFSET_F64 0x3fe6000000000000ull

// True for an x in [1 - 2^-4, 1 + 0x1.09p-4), given its bits.
static bool rc_log_is_near_one_f64(uint64_t bits)
{
    return bits - rc_bitcast_f64(1.0 - 0x1p-4) < rc_bitcast_f64(1.0 + 0x1.09p-4) - rc_bitcast_f64(1.0 - 0x1p-4);
}


// log(x) for an x near 1 but not 1, from log(1 + r) as a polynomial in r = x - 1. The worst-case
// error is around 0.507 ulp.
static double rc_log_near_one_f64(double x)
{
    const double *b = rc_log_data_f64.poly_near_one;
    double r = x - 1.0;
    double r2 = r * r;
    double r3 = r * r2;
    double poly = r3 * (b[1] + r * b[2] + r2 * b[3] + r3 * (b[4] + r * b[5] + r2 * b[6] + r3 * (b[7] + r * b[8] + r2 * b[9] + r3 * b[10])));
    double split = r * 0x1p27;
    double r_hi = r + split - split;
    double r_lo = r - r_hi;

    // r + b[0] r^2, where b[0] is -0.5, in two parts
    double square = r_hi * r_hi * b[0];
    double hi = r + square;
    double lo = r - hi + square + b[0] * r_lo * (r_hi + r);

    return poly + lo + hi;
}


// log(x) for a positive normal x, given its bits: log(x) = k ln2 + log(c) + log1p(z/c - 1).
static double rc_log_normal_f64(uint64_t bits)
{
    rc_log_reduction_f64 reduced = rc_log_reduce_f64(bits, RC_LOG_OFFSET_F64, RC_LOG_TABLE_BITS_F64);
    rc_log_centre_f64 centre = rc_log_data_f64.centres[reduced.index];
    rc_log_centre_split_f64 split = rc_log_data_f64.centre_splits[reduced.index];
    const double *a = rc_log_data_f64.poly;

    // r ~= z/c - 1, with |r| < 1/2N, and a rounding error of 2^-55/N + 2^-66
    double r = (reduced.z - split.c_hi - split.c_lo) * centre.inv_c;

    // hi + lo = r + log(c) + k ln2
    double w = reduced.k * rc_log_data_f64.ln2_hi + centre.log_c;
    double hi = w + r;
    double lo = w - hi + r + reduced.k * rc_log_data_f64.ln2_lo;

    // log(x) = lo + (log1p(r) - r) + hi. The worst-case error is 0.5 + 4.13/N + abs poly error
    // 2^57 ulp (+ 0.002 ulp without fma) for |log(x)| > 2^-5, and 0.5 + 2.06/N + abs poly error
    // 2^56 ulp (+ 0.001 ulp without fma) for |log(x)| > 2^-4.
    double r2 = r * r;

    return lo + r2 * a[0] + r * r2 * (a[1] + r * a[2] + r2 * (a[3] + r * a[4])) + hi;
}


// log(x) for an x below 2^-1022 (zero, subnormal or negative), infinite or NaN.
static double rc_log_unusual_f64(double x)
{
    uint64_t bits = rc_bitcast_f64(x);

    if ((bits << 1) == 0) {
        return rc_bitcast_u64(0xfff0000000000000ull);
    }

    if (bits == 0x7ff0000000000000ull) {
        return x;
    }

    if (rc_log_is_negative_or_nan_f64(bits)) {
        return (x - x) / (x - x);
    }

    return rc_log_normal_f64(rc_log_normalised_bits_f64(x));
}


double rc_log_f64(double x)
{
    uint64_t bits = rc_bitcast_f64(x);

    if (rc_log_is_near_one_f64(bits)) {
        return x == 1.0 ? 0.0 : rc_log_near_one_f64(x);
    }

    if (rc_log_is_unusual_f64(bits)) {
        return rc_log_unusual_f64(x);
    }

    return rc_log_normal_f64(bits);
}
