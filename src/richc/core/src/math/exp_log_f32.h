// math/exp_log_f32.h
//
// The kernels shared by the f32 exp, log, log2 and pow, all worked in double: 2^(n / 32), and the
// logs of a float. Double carries 29 more bits than the float answers need, so the kernels only
// have to be good to about 2^-30 relative for the double we round to float to land on the right
// side of the halfway point nearly always. Private to core.

#ifndef RC_MATH_EXP_LOG_F32_H_
#define RC_MATH_EXP_LOG_F32_H_

#include "richc/math/math.h"

enum {
    RC_EXP2_F32_TABLE_BITS = 5,
    RC_EXP2_F32_TABLE_SIZE = 1 << RC_EXP2_F32_TABLE_BITS,
    RC_LOG_F32_TABLE_BITS = 7,
    RC_LOG_F32_TABLE_SIZE = 1 << RC_LOG_F32_TABLE_BITS
};

// 1.5 x 2^52: adding it to a double below 2^51 in size rounds that to a whole number, which lands
// in the low bits of the sum, with no float to integer conversion for a NaN to upset.
#define RC_EXP2_F32_SHIFTER 0x1.8p52

// The bits of the float that starts the first of the log table's intervals, 0.705078125. The
// intervals are 2^16 apart in bits, and the 76th (index 75) is centred on 1.0.
#define RC_LOG_F32_OFFSET 0x3f348000u

// Added to the bits less the offset, 192 binades of headroom keep them positive for every positive
// float, the parts of subnormals included, so the shifts are on unsigned values.
#define RC_LOG_F32_HEADROOM 0x60000000u

// A log table entry: scale is near the reciprocal of the middle of its interval, with few enough
// bits that a float mantissa times it is exact in double, and log2_scale is its base 2 log.
typedef struct rc_log_f32_entry {
    double scale;
    double log2_scale;
} rc_log_f32_entry;

// A float split for the logs: x = 2^base (1 + r), with base = exponent - log2(scale) and r within
// 1/256, exactly but for the rounding of base.
typedef struct rc_log_f32_parts {
    double base;
    double r;
} rc_log_f32_parts;

// The bits of 2^(i/32), rounded to double, less i << 47. Adding k << 47 to entry k mod 32 then
// gives the bits of 2^(k/32), for any whole k within 2^16.
extern const uint64_t rc_exp2_f32_table[RC_EXP2_F32_TABLE_SIZE];

// The scale for each interval of mantissas, and its log.
extern const rc_log_f32_entry rc_log_f32_table[RC_LOG_F32_TABLE_SIZE];

// The coefficients of the polynomials: 2^(r / 32) - 1 from r, log(1 + r) - r from r^2, and
// log2(1 + r) from r. They live in memory beside the tables, not in the code: a wasm engine builds
// each double constant in the code with two instructions, where a load from memory takes one.
extern const double rc_exp2_f32_coefficients[3];
extern const double rc_log_f32_coefficients[3];
extern const double rc_log2_f32_coefficients[4];

// 2^(n / 32) for |n| < 32 x 1000, with a relative error under 2^-33, and with sign as its sign
// bit. With k the nearest whole number to n, r = n - k is exact and within 1/2. 2^(k / 32) comes
// from the table and the exponent, and 2^(r / 32) from a cubic: Sollya's fpminimax, absolute error
// 2^-33.2. k is rounded twice over, by the shifter for its bits and directly for r, so r doesn't
// wait on the shifter.
static inline double rc_exp2_f32_kernel(double n, uint64_t sign)
{
    double c1 = rc_exp2_f32_coefficients[0];
    double c2 = rc_exp2_f32_coefficients[1];
    double c3 = rc_exp2_f32_coefficients[2];
    uint64_t k = rc_bitcast_f64(n + RC_EXP2_F32_SHIFTER);
    double r = n - rc_round_even_f64(n);
    uint64_t scale_bits = rc_exp2_f32_table[k & (RC_EXP2_F32_TABLE_SIZE - 1)] + (k << 47) + sign;
    double r2 = r * r;

    return rc_bitcast_u64(scale_bits) * ((c3 * r + c2) * r2 + (c1 * r + 1.0));
}


// True for the bits of anything but a positive normal float: the logs' rare inputs.
static inline bool rc_log_f32_is_special(uint32_t bits)
{
    return bits - 0x00800000u >= 0x7f800000u - 0x00800000u;
}


// The bits of a positive subnormal, given its bits, as the parts of a float scaled up by 2^149 to
// a normal one, less 149 binades: the parts of the subnormal itself. They wrap below zero, which
// the headroom undoes. Converting the bits, a whole number below 2^23, is exact, and no arithmetic
// touches the subnormal, which some processors take many cycles over.
static inline uint32_t rc_log_f32_subnormal_bits(uint32_t bits)
{
    return rc_bitcast_f32((float)(int32_t)bits) - (149u << 23);
}


// The bits of a positive normal float, or of a subnormal from rc_log_f32_subnormal_bits, split
// into its parts. Taking the offset from the bits leaves the exponent in the top bits and the
// table index below them, and the mantissa m in [0.705078125, 1.41015625) is the offset plus the
// low bits. m times its interval's scale is 1 + r, exactly. A power of 2 has m = 1, r = 0 and a
// scale of exactly 1.
static inline rc_log_f32_parts rc_log_f32_split(uint32_t bits)
{
    uint32_t from_offset = bits - RC_LOG_F32_OFFSET + RC_LOG_F32_HEADROOM;
    int32_t exponent = (int32_t)(from_offset >> 23) - (int32_t)(RC_LOG_F32_HEADROOM >> 23);
    float mantissa = rc_bitcast_u32((from_offset & 0x007fffffu) + RC_LOG_F32_OFFSET);
    uint32_t index = (from_offset >> (23 - RC_LOG_F32_TABLE_BITS)) & (RC_LOG_F32_TABLE_SIZE - 1);
    rc_log_f32_entry entry = rc_log_f32_table[index];

    return (rc_log_f32_parts) {
        .base = (double)exponent - entry.log2_scale,
        .r = (double)mantissa * entry.scale - 1.0
    };
}


// base + log2(1 + r) for r in [-0.003876, 0.00390625]: Sollya's fpminimax with b1 = 1 / ln 2,
// relative error 2^-36.3, falling towards 0 with r. Split as (base + b1 r + b2 r^2) +
// r^3 (b3 + b4 r), the two halves are ready together.
static inline double rc_log2_f32_poly(double base, double r)
{
    double b1 = rc_log2_f32_coefficients[0];
    double b2 = rc_log2_f32_coefficients[1];
    double b3 = rc_log2_f32_coefficients[2];
    double b4 = rc_log2_f32_coefficients[3];
    double r2 = r * r;
    double r3 = r2 * r;

    return ((base + b1 * r) + b2 * r2) + r3 * (b3 + b4 * r);
}


// The base 2 log of a float, from its parts, with a relative error under 2^-36.
static inline double rc_log2_f32_kernel(uint32_t bits)
{
    rc_log_f32_parts parts = rc_log_f32_split(bits);

    return rc_log2_f32_poly(parts.base, parts.r);
}

#endif // RC_MATH_EXP_LOG_F32_H_
