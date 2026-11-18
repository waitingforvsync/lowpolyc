// math/exp_log_f64.h
//
// The tables behind rc_exp_f64, rc_log_f64, rc_log2_f64 and rc_pow_f64, and the parts they share:
// exp's kernel, which pow ends with too, and the reduction of x to 2^k z that every log starts
// with. Private to core. Ported from musl 1.2.5 (MIT), src/math/exp_data.h, log_data.h,
// log2_data.h, pow_data.h, exp.c and pow.c, which are Arm's optimized-routines. Their notice:
//
// Copyright (c) 2018, Arm Limited.
// SPDX-License-Identifier: MIT

#ifndef RC_MATH_EXP_LOG_F64_H_
#define RC_MATH_EXP_LOG_F64_H_

#include "richc/math/math.h"

// ---- tables ----

#define RC_EXP_TABLE_BITS_F64     7
#define RC_LOG_TABLE_BITS_F64     7
#define RC_LOG2_TABLE_BITS_F64    6
#define RC_POW_LOG_TABLE_BITS_F64 7

// The constants for exp, which pow shares.
typedef struct rc_exp_table_f64 {
    double inv_ln2_n;
    double shift;
    double neg_ln2_hi_n;
    double neg_ln2_lo_n;
    double poly[4];

    // 2^(k/N) ~= H[k] (1 + T[k]) for k in [0, N), as the bits of T[k] then of H[k] - (k << 52) / N
    uint64_t scales[2 * (1 << RC_EXP_TABLE_BITS_F64)];
} rc_exp_table_f64;

// 1/c and log(c) for the c near the centre of one subinterval of z, for log and log2.
typedef struct rc_log_centre_f64 {
    double inv_c;
    double log_c;
} rc_log_centre_f64;

// That c as the sum of two doubles, for working out z/c - 1 without fma.
typedef struct rc_log_centre_split_f64 {
    double c_hi;
    double c_lo;
} rc_log_centre_split_f64;

// The constants for log.
typedef struct rc_log_table_f64 {
    double ln2_hi;
    double ln2_lo;
    double poly[5];
    double poly_near_one[11];
    rc_log_centre_f64 centres[1 << RC_LOG_TABLE_BITS_F64];
    rc_log_centre_split_f64 centre_splits[1 << RC_LOG_TABLE_BITS_F64];
} rc_log_table_f64;

// The constants for log2.
typedef struct rc_log2_table_f64 {
    double inv_ln2_hi;
    double inv_ln2_lo;
    double poly[6];
    double poly_near_one[10];
    rc_log_centre_f64 centres[1 << RC_LOG2_TABLE_BITS_F64];
    rc_log_centre_split_f64 centre_splits[1 << RC_LOG2_TABLE_BITS_F64];
} rc_log2_table_f64;

// 1/c and log(c) for pow's log, with log(c) split in two. The pad is unused, but makes each entry
// a power of two in size, for quicker indexing.
typedef struct rc_pow_log_centre_f64 {
    double inv_c;
    double pad;
    double log_c;
    double log_c_tail;
} rc_pow_log_centre_f64;

// The constants for the log inside pow.
typedef struct rc_pow_log_table_f64 {
    double ln2_hi;
    double ln2_lo;
    double poly[7];
    rc_pow_log_centre_f64 centres[1 << RC_POW_LOG_TABLE_BITS_F64];
} rc_pow_log_table_f64;

extern const rc_exp_table_f64 rc_exp_data_f64;
extern const rc_log_table_f64 rc_log_data_f64;
extern const rc_log2_table_f64 rc_log2_data_f64;
extern const rc_pow_log_table_f64 rc_pow_log_data_f64;

// ---- bits ----

// The top 12 bits of x: its sign and exponent.
static inline uint32_t rc_top12_f64(double x)
{
    return (uint32_t)(rc_bitcast_f64(x) >> 52);
}

// The top 12 bits of bits read as a signed number: an arithmetic shift down by 52, without
// shifting a negative value.
static inline int32_t rc_top12_signed_f64(uint64_t bits)
{
    return (int32_t)((bits >> 52) ^ 0x800u) - 0x800;
}

// ---- exp, for exp and pow ----

// What exp's argument reduction leaves: the result is scale (1 + tmp), where the bits of scale are
// scale_bits, whose exponent may have wrapped into the sign bit, and k is the low 32 bits of ki.
typedef struct rc_exp_reduction_f64 {
    double tmp;
    uint64_t scale_bits;
    uint64_t ki;
} rc_exp_reduction_f64;

// x = k ln2/N + r, with r in [-ln2/2N, ln2/2N], so that exp(x) = 2^(k/N) exp(r), with exp(r) in
// [2^(-1/2N), 2^(1/2N)], and 2^(k/N) exp(r) ~= scale + scale (tail + exp(r) - 1). xtail is added
// to r. sign_bias, 0 or 0x800 << 7, lands on scale's sign bit.
static inline rc_exp_reduction_f64 rc_exp_reduce_f64(double x, double xtail, uint64_t sign_bias)
{
    const double *poly = rc_exp_data_f64.poly;
    double z = rc_exp_data_f64.inv_ln2_n * x;
    double shifted = z + rc_exp_data_f64.shift;
    uint64_t ki = rc_bitcast_f64(shifted);
    double kd = shifted - rc_exp_data_f64.shift;
    double r = x + kd * rc_exp_data_f64.neg_ln2_hi_n + kd * rc_exp_data_f64.neg_ln2_lo_n + xtail;
    uint64_t pair = 2 * (ki % (1u << RC_EXP_TABLE_BITS_F64));
    uint64_t top = (ki + sign_bias) << (52 - RC_EXP_TABLE_BITS_F64);
    double tail = rc_bitcast_u64(rc_exp_data_f64.scales[pair]);
    double r2 = r * r;

    // Only a valid scale when -1023 N < k < 1024 N. Without fma the worst case error is 0.25/N ulp
    // larger: less than 0.5 + 1.11/N + (abs poly error * 2^53) ulp in all.
    return (rc_exp_reduction_f64) {
        .tmp = tail + r + r2 * (poly[0] + r * poly[1]) + r2 * r2 * (poly[2] + r * poly[3]),
        .scale_bits = rc_exp_data_f64.scales[pair + 1] + top,
        .ki = ki
    };
}

// scale (1 + tmp) for a scale and tmp from a reduction that stayed in range.
static inline double rc_exp_scaled_f64(rc_exp_reduction_f64 reduced)
{
    double scale = rc_bitcast_u64(reduced.scale_bits);

    return scale + scale * reduced.tmp;
}

// y = scale (1 + tmp), below 1 in size, rounded to the precision it will have once scaled into the
// subnormals. Rounding it there first avoids a double rounding, which would add up to half an ulp
// to the worst-case error.
static inline double rc_exp_round_to_subnormal_f64(double scale, double tmp, double y, uint64_t scale_bits)
{
    double one = y < 0.0 ? -1.0 : 1.0;
    double lo = scale - y + scale * tmp;
    double hi = one + y;
    double lo_sum = one - hi + y + lo;
    double rounded = hi + lo_sum - one;

    return rounded == 0.0 ? rc_bitcast_u64(scale_bits & 0x8000000000000000ull) : rounded;
}

// scale (1 + tmp) for a negative k, with scale_bits already raised by 2^1022, and rounded for the
// subnormals if that's where it will land.
static inline double rc_exp_scaled_up_f64(double tmp, uint64_t scale_bits)
{
    double scale = rc_bitcast_u64(scale_bits);
    double y = scale + scale * tmp;

    if (rc_abs_f64(y) >= 1.0) {
        return y;
    }

    return rc_exp_round_to_subnormal_f64(scale, tmp, y, scale_bits);
}

// scale (1 + tmp) without intermediate rounding, where it may overflow or underflow. A positive k
// may overflow, and has an exponent in scale_bits that overflowed by at most 460. A negative k may
// underflow, and needs care in the subnormal range.
static inline double rc_exp_scaled_extreme_f64(rc_exp_reduction_f64 reduced)
{
    if ((reduced.ki & 0x80000000u) == 0) {
        double scale = rc_bitcast_u64(reduced.scale_bits - (1009ull << 52));

        return 0x1p1009 * (scale + scale * reduced.tmp);
    }

    return 0x1p-1022 * rc_exp_scaled_up_f64(reduced.tmp, reduced.scale_bits + (1022ull << 52));
}

// True unless 2^-54 <= |x| < 512, given the top 12 bits of |x|.
static inline bool rc_exp_is_unusual_f64(uint32_t abstop)
{
    return abstop - rc_top12_f64(0x1p-54) >= rc_top12_f64(512.0) - rc_top12_f64(0x1p-54);
}

// Infinity for a positive x and zero for a negative one, negated for a nonzero sign_bias.
static inline double rc_exp_out_of_range_f64(double x, uint64_t sign_bias)
{
    double magnitude = (rc_bitcast_f64(x) >> 63) != 0 ? 0.0 : rc_bitcast_u64(0x7ff0000000000000ull);

    return sign_bias != 0 ? -magnitude : magnitude;
}

// exp(x + xtail) for a finite x outside [2^-54, 512) in size: 1 + x for a tiny x, overflow or
// underflow from 1024 up, and a careful scaling between.
static inline double rc_exp_unusual_f64(double x, double xtail, uint64_t sign_bias, uint32_t abstop)
{
    if (abstop < rc_top12_f64(0x1p-54)) {
        return sign_bias != 0 ? -(1.0 + x) : 1.0 + x;
    }

    if (abstop >= rc_top12_f64(1024.0)) {
        return rc_exp_out_of_range_f64(x, sign_bias);
    }

    return rc_exp_scaled_extreme_f64(rc_exp_reduce_f64(x, xtail, sign_bias));
}

// exp(x + xtail), negated for a nonzero sign_bias, for a finite x with |xtail| < 2^-8/N and
// |xtail| <= |x|.
static inline double rc_exp_kernel_f64(double x, double xtail, uint64_t sign_bias)
{
    uint32_t abstop = rc_top12_f64(x) & 0x7ffu;

    if (rc_exp_is_unusual_f64(abstop)) {
        return rc_exp_unusual_f64(x, xtail, sign_bias, abstop);
    }

    return rc_exp_scaled_f64(rc_exp_reduce_f64(x, xtail, sign_bias));
}

// ---- the logs' reduction ----

// x = 2^k z, with z in [offset, 2 offset) and exact, and the index of the subinterval z falls in,
// of the 2^table_bits that split that range.
typedef struct rc_log_reduction_f64 {
    uint32_t index;
    double k;
    double z;
    uint64_t z_bits;
} rc_log_reduction_f64;

// The reduction of the positive normal x with the given bits.
static inline rc_log_reduction_f64 rc_log_reduce_f64(uint64_t bits, uint64_t offset, uint32_t table_bits)
{
    uint64_t tmp = bits - offset;
    uint64_t z_bits = bits - (tmp & (0xfffull << 52));

    return (rc_log_reduction_f64) {
        .index = (uint32_t)((tmp >> (52 - table_bits)) % (1u << table_bits)),
        .k = (double)rc_top12_signed_f64(tmp),
        .z = rc_bitcast_u64(z_bits),
        .z_bits = z_bits
    };
}

// True for the x log and log2 can't reduce as they are: below 2^-1022 (zero, subnormal or
// negative), infinite or NaN.
static inline bool rc_log_is_unusual_f64(uint64_t bits)
{
    uint32_t top = (uint32_t)(bits >> 48);

    return top - 0x0010u >= 0x7ff0u - 0x0010u;
}

// True for the bits of a negative number or a NaN.
static inline bool rc_log_is_negative_or_nan_f64(uint64_t bits)
{
    return (bits >> 63) != 0 || (bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull;
}

// The bits of the positive subnormal x, normalised: x scaled up by 2^52, then 52 taken off its
// exponent, which wraps below zero. Only the reduction makes sense of them.
static inline uint64_t rc_log_normalised_bits_f64(double x)
{
    return rc_bitcast_f64(x * 0x1p52) - (52ull << 52);
}

#endif // RC_MATH_EXP_LOG_F64_H_
