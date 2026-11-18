// math/exp_log_f32.h
//
// The tables behind the float exp, log, log2 and pow, and the steps they share. Private to core.
// Ported from musl 1.2.5 (MIT), src/math/exp2f_data.h, logf_data.h, log2f_data.h and powf_data.h,
// which are Arm's optimized-routines. Their notice:
//
// Copyright (c) 2017-2018, Arm Limited.
// SPDX-License-Identifier: MIT
//
// exp works in powers of 2^(1/32): it splits x into k/32 + r, takes 2^(k/32) from a table and
// 2^r from a cubic. The logs split x into 2^k z, with z near one of 16 table centres c, and add
// log(c) to a polynomial for log1p(z/c - 1). pow is 2^(y log2(x)), from both halves.

#ifndef RC_MATH_EXP_LOG_F32_H_
#define RC_MATH_EXP_LOG_F32_H_

#include "richc/math/math.h"

// The exp table has 2^5 entries, for steps of 2^(1/32).
#define RC_EXP_TABLE_BITS_F32 5
#define RC_EXP_TABLE_SIZE_F32 (1 << RC_EXP_TABLE_BITS_F32)

// The log tables have 2^4 entries, splitting [RC_LOG_OFFSET_F32, 2 RC_LOG_OFFSET_F32) into 16.
#define RC_LOG_TABLE_BITS_F32 4
#define RC_LOG_TABLE_SIZE_F32 (1 << RC_LOG_TABLE_BITS_F32)

// The bits of 0x1.66p-1, the bottom of the range that z is brought into.
#define RC_LOG_OFFSET_F32 0x3f330000u

// The data for exp, shared with pow. exp scales its argument by 32 / ln2, so its polynomial is
// scaled to match; pow has a power of 2 already, so takes the unscaled one.
typedef struct rc_exp_table_f32 {
    // The bits of 2^(i/32), less i << 47, so that adding k << 47 for any k with the same low bits
    // gives 2^(k/32).
    uint64_t powers[RC_EXP_TABLE_SIZE_F32];

    // Adding shift, 0x1.8p52, rounds a double to a whole number and leaves it in the low bits;
    // shift_scaled, a 32nd of it, rounds to a multiple of 1/32 in the same way.
    double shift_scaled;
    double poly[3];
    double shift;
    double inv_ln2_scaled;
    double poly_scaled[3];
} rc_exp_table_f32;

// A centre c of one of the 16 subintervals, near which log(z) is log(c) + log1p(z/c - 1).
typedef struct rc_log_centre_f32 {
    double inv_c;
    double log_c;
} rc_log_centre_f32;

// The data for log. The polynomial approximates log1p(r), less the r term, whose coefficient is 1.
typedef struct rc_log_table_f32 {
    rc_log_centre_f32 centres[RC_LOG_TABLE_SIZE_F32];
    double ln2;
    double poly[3];
} rc_log_table_f32;

// The data for log2, where log_c is log2(c) and the polynomial approximates log1p(r) / ln2.
typedef struct rc_log2_table_f32 {
    rc_log_centre_f32 centres[RC_LOG_TABLE_SIZE_F32];
    double poly[4];
} rc_log2_table_f32;

// The data for pow's log2, which is as log2's but with one more term.
typedef struct rc_pow_table_f32 {
    rc_log_centre_f32 centres[RC_LOG_TABLE_SIZE_F32];
    double poly[5];
} rc_pow_table_f32;

// x as 2^exponent z, with z exact and in [RC_LOG_OFFSET_F32, 2 RC_LOG_OFFSET_F32), and the
// subinterval holding z.
typedef struct rc_log_parts_f32 {
    double z;
    int32_t exponent;
    uint32_t index;
} rc_log_parts_f32;

extern const rc_exp_table_f32 rc_exp_data_f32;
extern const rc_log_table_f32 rc_log_data_f32;
extern const rc_log2_table_f32 rc_log2_data_f32;
extern const rc_pow_table_f32 rc_pow_data_f32;

// What exp and pow give for a result too big for a float: infinity, negative if sign_bias is set.
static inline float rc_exp_overflow_f32(uint32_t sign_bias)
{
    return rc_bitcast_u32(sign_bias != 0 ? 0xff800000u : 0x7f800000u);
}

// What exp and pow give for a result too small for a float: zero, negative if sign_bias is set.
static inline float rc_exp_underflow_f32(uint32_t sign_bias)
{
    return sign_bias != 0 ? -0.0f : 0.0f;
}

// 2^(k/32) 2^r ~= s (c0 r^3 + c1 r^2 + c2 r + 1), where s = 2^(k/32) comes from the table and k is
// the low bits of k_bits. sign_bias, added to k above the bits that reach the exponent, sets the
// sign of the result.
static inline float rc_exp_from_reduced_f32(uint64_t k_bits, uint32_t sign_bias, double r, const double *poly)
{
    uint64_t scale_bits = rc_exp_data_f32.powers[k_bits % RC_EXP_TABLE_SIZE_F32] + ((k_bits + sign_bias) << (52 - RC_EXP_TABLE_BITS_F32));
    double scale = rc_bitcast_u64(scale_bits);
    double high = poly[0] * r + poly[1];
    double r2 = r * r;
    double low = poly[2] * r + 1.0;

    return (float)((high * r2 + low) * scale);
}

// True if the x with these bits is below 2^-126, negative, infinite or NaN, which the logs' main
// path can't take as it is.
static inline bool rc_log_is_special_f32(uint32_t bits)
{
    return bits - 0x00800000u >= 0x7f800000u - 0x00800000u;
}

// The bits of a positive subnormal x scaled up to a normal, then with 23 taken off the exponent,
// which leaves it below zero.
static inline uint32_t rc_log_normalise_subnormal_f32(float x)
{
    return (rc_bitcast_f32(x * 0x1p23f) & 0x7fffffffu) - (23u << 23);
}

// The top 9 bits of bits as a signed number, as an arithmetic shift right by 23 would give.
static inline int32_t rc_log_top_bits_signed_f32(uint32_t bits)
{
    return (int32_t)(bits >> 23) - (int32_t)((bits >> 31) << 9);
}

// x_bits, the bits of a normal x or a normalised subnormal one, split into its parts.
static inline rc_log_parts_f32 rc_log_parts_of_f32(uint32_t x_bits)
{
    uint32_t offset = x_bits - RC_LOG_OFFSET_F32;

    return (rc_log_parts_f32) {
        .z = (double)rc_bitcast_u32(x_bits - (offset & 0xff800000u)),
        .exponent = rc_log_top_bits_signed_f32(offset),
        .index = (offset >> (23 - RC_LOG_TABLE_BITS_F32)) % RC_LOG_TABLE_SIZE_F32
    };
}

#endif // RC_MATH_EXP_LOG_F32_H_
