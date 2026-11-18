// math/log2_f64.c
//
// The base 2 logarithm of a double. Ported from musl 1.2.5 (MIT), src/math/log2.c, which is Arm's
// optimized-routines. Its notice:
//
// Copyright (c) 2018, Arm Limited.
// SPDX-License-Identifier: MIT

#include "richc/math/math.h"

#include "exp_log_f64.h"

// The start of the range [offset, 2 offset) of z, which x's bits are reduced to by taking away k.
#define RC_LOG2_OFFSET_F64 0x3fe6000000000000ull

// True for an x in [1 - 0x1.5b51p-5, 1 + 0x1.6ab2p-5), given its bits.
static bool rc_log2_is_near_one_f64(uint64_t bits)
{
    return bits - rc_bitcast_f64(1.0 - 0x1.5b51p-5) < rc_bitcast_f64(1.0 + 0x1.6ab2p-5) - rc_bitcast_f64(1.0 - 0x1.5b51p-5);
}


// x with the low 32 bits of its significand cleared, for splitting a product in two.
static double rc_log2_high_part_f64(double x)
{
    return rc_bitcast_u64(rc_bitcast_f64(x) & (UINT64_MAX << 32));
}


// log2(x) for an x near 1 but not 1, from log2(1 + r) as a polynomial in r = x - 1. The
// worst-case error is less than 0.55 ulp.
static double rc_log2_near_one_f64(double x)
{
    const double *b = rc_log2_data_f64.poly_near_one;
    double r = x - 1.0;
    double r_hi = rc_log2_high_part_f64(r);
    double r_lo = r - r_hi;

    // r/ln2 in two parts
    double hi = r_hi * rc_log2_data_f64.inv_ln2_hi;
    double lo = r_lo * rc_log2_data_f64.inv_ln2_hi + r * rc_log2_data_f64.inv_ln2_lo;

    // r2 has a rounding error of 2^-62
    double r2 = r * r;
    double r4 = r2 * r2;
    double p = r2 * (b[0] + r * b[1]);
    double y = hi + p;
    double tail = r4 * (b[2] + r * b[3] + r2 * (b[4] + r * b[5]) + r4 * (b[6] + r * b[7] + r2 * (b[8] + r * b[9])));
    double lo_sum = lo + (hi - y + p) + tail;

    return y + lo_sum;
}


// log2(x) for a positive normal x, given its bits: log2(x) = k + log2(c) + log2(z/c).
static double rc_log2_normal_f64(uint64_t bits)
{
    rc_log_reduction_f64 reduced = rc_log_reduce_f64(bits, RC_LOG2_OFFSET_F64, RC_LOG2_TABLE_BITS_F64);
    rc_log_centre_f64 centre = rc_log2_data_f64.centres[reduced.index];
    rc_log_centre_split_f64 split = rc_log2_data_f64.centre_splits[reduced.index];
    const double *a = rc_log2_data_f64.poly;

    // r ~= z/c - 1, with |r| < 1/2N, and a rounding error of 2^-55/N + 2^-65
    double r = (reduced.z - split.c_hi - split.c_lo) * centre.inv_c;
    double r_hi = rc_log2_high_part_f64(r);
    double r_lo = r - r_hi;
    double t1 = r_hi * rc_log2_data_f64.inv_ln2_hi;
    double t2 = r_lo * rc_log2_data_f64.inv_ln2_hi + r * rc_log2_data_f64.inv_ln2_lo;

    // hi + lo = r/ln2 + log2(c) + k
    double t3 = reduced.k + centre.log_c;
    double hi = t3 + t1;
    double lo = t3 - hi + t1 + t2;

    // log2(1 + r) = r/ln2 + r^2 poly(r), where r2 has a rounding error of 2^-54/N^2. The
    // worst-case error is 0.550 ulp for |log2(x)| > 2^-4: ~ 0.5 + 2/N/ln2 + abs poly error 2^56
    // ulp (+ 0.003 ulp without fma).
    double r2 = r * r;
    double r4 = r2 * r2;
    double p = a[0] + r * a[1] + r2 * (a[2] + r * a[3]) + r4 * (a[4] + r * a[5]);

    return lo + r2 * p + hi;
}


// log2(x) for an x below 2^-1022 (zero, subnormal or negative), infinite or NaN.
static double rc_log2_unusual_f64(double x)
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

    return rc_log2_normal_f64(rc_log_normalised_bits_f64(x));
}


double rc_log2_f64(double x)
{
    uint64_t bits = rc_bitcast_f64(x);

    if (rc_log2_is_near_one_f64(bits)) {
        return x == 1.0 ? 0.0 : rc_log2_near_one_f64(x);
    }

    if (rc_log_is_unusual_f64(bits)) {
        return rc_log2_unusual_f64(x);
    }

    return rc_log2_normal_f64(bits);
}
