// math/pow_f64.c
//
// A double to the power of a double. Ported from musl 1.2.5 (MIT), src/math/pow.c, which is Arm's
// optimized-routines. Its notice:
//
// Copyright (c) 2018, Arm Limited.
// SPDX-License-Identifier: MIT
//
// x^y = exp(y log(x)), with log(x) worked out to about 15 bits more than a double holds. The
// worst-case error is 0.54 ulp: about exp's ulp error, 0.511 without fma, plus 1024 ln2 2^53 times
// log's relative error, 1.5 2^-68 without fma.

#include "richc/math/math.h"

#include "exp_log_f64.h"

// The start of the range [offset, 2 offset) of z, which x's bits are reduced to by taking away k.
#define RC_POW_LOG_OFFSET_F64 0x3fe6955500000000ull

// Passed to exp's kernel to make the result negative.
#define RC_POW_SIGN_BIAS_F64 ((uint64_t)0x800 << RC_EXP_TABLE_BITS_F64)

// The bits of 1.0.
#define RC_POW_ONE_BITS_F64 0x3ff0000000000000ull

// The bits of infinity.
#define RC_POW_INFINITY_BITS_F64 0x7ff0000000000000ull

// Whether a y is a whole number, and if so whether it's odd or even.
typedef enum rc_pow_whole {
    RC_POW_WHOLE_NONE,
    RC_POW_WHOLE_ODD,
    RC_POW_WHOLE_EVEN
} rc_pow_whole;

// log(x) as the sum of a double and a tail of about 15 more bits.
typedef struct rc_pow_log_parts_f64 {
    double hi;
    double lo;
} rc_pow_log_parts_f64;

// log(x) for a positive normal x, given its bits, or those of a subnormal x normalised with its
// exponent going negative.
static rc_pow_log_parts_f64 rc_pow_log_f64(uint64_t bits)
{
    rc_log_reduction_f64 reduced = rc_log_reduce_f64(bits, RC_POW_LOG_OFFSET_F64, RC_POW_LOG_TABLE_BITS_F64);
    rc_pow_log_centre_f64 centre = rc_pow_log_data_f64.centres[reduced.index];
    const double *a = rc_pow_log_data_f64.poly;

    // 1/c is j/N or j/N/2 for a whole j in [N, 2N), and |z/c - 1| < 1/N, so r = z/c - 1 is exact.
    // Splitting z makes r_hi, r_lo and r_hi^2 exact, and |r_lo| <= |r|.
    double z_hi = rc_bitcast_u64((reduced.z_bits + (1ull << 31)) & (UINT64_MAX << 32));
    double z_lo = reduced.z - z_hi;
    double r_hi = z_hi * centre.inv_c - 1.0;
    double r_lo = z_lo * centre.inv_c;
    double r = r_hi + r_lo;

    // k ln2 + log(c) + r
    double t1 = reduced.k * rc_pow_log_data_f64.ln2_hi + centre.log_c;
    double t2 = t1 + r;
    double lo1 = reduced.k * rc_pow_log_data_f64.ln2_lo + centre.log_c_tail;
    double lo2 = t1 - t2 + r;

    // k ln2 + log(c) + r + a[0] r^2, where a[0] is -0.5
    double ar = a[0] * r;
    double ar2 = r * ar;
    double ar3 = r * ar2;
    double ar_hi = a[0] * r_hi;
    double ar_hi2 = r_hi * ar_hi;
    double hi = t2 + ar_hi2;
    double lo3 = r_lo * (ar + ar_hi);
    double lo4 = t2 - hi + ar_hi2;

    // log1p(r) - r - a[0] r^2
    double p = ar3 * (a[1] + r * a[2] + ar2 * (a[3] + r * a[4] + ar2 * (a[5] + r * a[6])));
    double lo = lo1 + lo2 + lo3 + lo4 + p;
    double y = hi + lo;

    return (rc_pow_log_parts_f64) {.hi = y, .lo = hi - y + lo};
}


// x^y for a positive x, given its bits, or those of a subnormal x normalised with its exponent
// going negative, and a |y| in [2^-65, 2^63). A nonzero sign_bias negates it.
static double rc_pow_core_f64(uint64_t x_bits, double y, uint64_t sign_bias)
{
    rc_pow_log_parts_f64 log_x = rc_pow_log_f64(x_bits);

    // y log(x) in two parts, |e_lo| < |e_hi| 2^-25
    double y_hi = rc_bitcast_u64(rc_bitcast_f64(y) & (UINT64_MAX << 27));
    double y_lo = y - y_hi;
    double log_hi = rc_bitcast_u64(rc_bitcast_f64(log_x.hi) & (UINT64_MAX << 27));
    double log_lo = log_x.hi - log_hi + log_x.lo;
    double e_hi = y_hi * log_hi;
    double e_lo = y_lo * log_hi + y * log_lo;

    return rc_exp_kernel_f64(e_hi, e_lo, sign_bias);
}


// Whether the nonzero finite y, given its bits, is a whole number whose units bit is unit_shift
// bits up, and if so whether it's odd or even.
static rc_pow_whole rc_pow_whole_at_f64(uint64_t y_bits, uint32_t unit_shift)
{
    uint64_t unit = 1ull << unit_shift;

    if ((y_bits & (unit - 1)) != 0) {
        return RC_POW_WHOLE_NONE;
    }

    return (y_bits & unit) != 0 ? RC_POW_WHOLE_ODD : RC_POW_WHOLE_EVEN;
}


// Whether the nonzero finite y, given its bits, is a whole number, and if so whether it is
// odd or even.
static rc_pow_whole rc_pow_whole_of_f64(uint64_t y_bits)
{
    uint32_t exponent = (uint32_t)(y_bits >> 52) & 0x7ffu;

    if (exponent < 0x3ff) {
        return RC_POW_WHOLE_NONE;
    }

    if (exponent > 0x3ff + 52) {
        return RC_POW_WHOLE_EVEN;
    }

    return rc_pow_whole_at_f64(y_bits, 0x3ff + 52 - exponent);
}


// True for the bits of zero, infinity or NaN, of either sign.
static bool rc_pow_is_zero_inf_nan_f64(uint64_t bits)
{
    return 2 * bits - 1 >= 2 * RC_POW_INFINITY_BITS_F64 - 1;
}


// True for the bits of a NaN.
static bool rc_pow_is_nan_f64(uint64_t bits)
{
    return 2 * bits > 2 * RC_POW_INFINITY_BITS_F64;
}


// True if x^y needs more than the core: x is negative, zero, subnormal, infinite or NaN, or |y| is
// below 2^-65, at least 2^63 or NaN. Beyond |y| ~= 0x1.749p62 (1075 ln2 2^53), x^y is infinity or
// zero, and below 0x1.e7b6p-65 (2^-54 / 1075) it's +-1.
static bool rc_pow_is_unusual_f64(uint64_t x_bits, uint64_t y_bits)
{
    uint32_t x_top = (uint32_t)(x_bits >> 52);
    uint32_t y_top = (uint32_t)(y_bits >> 52) & 0x7ffu;

    return x_top - 0x001u >= 0x7ffu - 0x001u ||
        y_top - 0x3beu >= 0x43eu - 0x3beu;
}


// x^y for a y of zero, infinity or NaN.
static double rc_pow_special_y_f64(double x, double y)
{
    uint64_t x_bits = rc_bitcast_f64(x);
    uint64_t y_bits = rc_bitcast_f64(y);
    bool x_inside_one = 2 * x_bits < 2 * RC_POW_ONE_BITS_F64;
    bool y_positive = (y_bits >> 63) == 0;

    if (2 * y_bits == 0) {
        return 1.0;
    }

    if (x_bits == RC_POW_ONE_BITS_F64) {
        return 1.0;
    }

    if (rc_pow_is_nan_f64(x_bits) || rc_pow_is_nan_f64(y_bits)) {
        return x + y;
    }

    if (2 * x_bits == 2 * RC_POW_ONE_BITS_F64) {
        return 1.0;
    }

    // |x| < 1 and y is infinity, or |x| > 1 and y is -infinity
    if (x_inside_one == y_positive) {
        return 0.0;
    }

    return y * y;
}


// True if x is negative and y an odd whole number, given their bits, for a finite nonzero y.
static bool rc_pow_is_negative_to_odd_f64(uint64_t x_bits, uint64_t y_bits)
{
    return (x_bits >> 63) != 0 && rc_pow_whole_of_f64(y_bits) == RC_POW_WHOLE_ODD;
}


// x^y for an x of zero, infinity or NaN, and a finite nonzero y.
static double rc_pow_special_x_f64(double x, double y)
{
    uint64_t x_bits = rc_bitcast_f64(x);
    uint64_t y_bits = rc_bitcast_f64(y);
    double square = rc_pow_is_negative_to_odd_f64(x_bits, y_bits) ? -(x * x) : x * x;

    return (y_bits >> 63) != 0 ? 1.0 / square : square;
}


// x^y where |y| is below 2^-65 or at least 2^63, for a nonzero finite x, given the bits of |x|, and
// y finite and nonzero. Such a y is even if it's whole, so the result is positive.
static double rc_pow_extreme_y_f64(uint64_t x_abs_bits, double y)
{
    uint32_t y_top = rc_top12_f64(y);

    if (x_abs_bits == RC_POW_ONE_BITS_F64) {
        return 1.0;
    }

    // x^y ~= 1 + y log(x) for |y| < 2^-65
    if ((y_top & 0x7ffu) < 0x3beu) {
        return x_abs_bits > RC_POW_ONE_BITS_F64 ? 1.0 + y : 1.0 - y;
    }

    if ((x_abs_bits > RC_POW_ONE_BITS_F64) == (y_top < 0x800u)) {
        return rc_bitcast_u64(RC_POW_INFINITY_BITS_F64);
    }

    return 0.0;
}


// x^y for a nonzero finite x and y, given the bits of |x|. A nonzero sign_bias negates it.
static double rc_pow_magnitude_f64(double x, uint64_t x_abs_bits, double y, uint64_t sign_bias)
{
    uint32_t y_top = rc_top12_f64(y) & 0x7ffu;

    if (y_top - 0x3beu >= 0x43eu - 0x3beu) {
        return rc_pow_extreme_y_f64(x_abs_bits, y);
    }

    // A subnormal x is normalised so that its exponent goes negative
    if ((x_abs_bits >> 52) == 0) {
        return rc_pow_core_f64((rc_bitcast_f64(x * 0x1p52) & 0x7fffffffffffffffull) - (52ull << 52), y, sign_bias);
    }

    return rc_pow_core_f64(x_abs_bits, y, sign_bias);
}


// x^y for a negative finite x and a finite nonzero y: NaN unless y is whole, and negative if
// it is odd.
static double rc_pow_negative_x_f64(double x, double y)
{
    uint64_t x_abs_bits = rc_bitcast_f64(x) & 0x7fffffffffffffffull;
    rc_pow_whole whole = rc_pow_whole_of_f64(rc_bitcast_f64(y));

    if (whole == RC_POW_WHOLE_NONE) {
        return (x - x) / (x - x);
    }

    return rc_pow_magnitude_f64(x, x_abs_bits, y, whole == RC_POW_WHOLE_ODD ? RC_POW_SIGN_BIAS_F64 : 0);
}


// x^y where x is negative, zero, subnormal, infinite or NaN, or y is extreme in size or NaN.
static double rc_pow_unusual_f64(double x, double y)
{
    uint64_t x_bits = rc_bitcast_f64(x);
    uint64_t y_bits = rc_bitcast_f64(y);

    if (rc_pow_is_zero_inf_nan_f64(y_bits)) {
        return rc_pow_special_y_f64(x, y);
    }

    if (rc_pow_is_zero_inf_nan_f64(x_bits)) {
        return rc_pow_special_x_f64(x, y);
    }

    if ((x_bits >> 63) != 0) {
        return rc_pow_negative_x_f64(x, y);
    }

    return rc_pow_magnitude_f64(x, x_bits, y, 0);
}


double rc_pow_f64(double x, double y)
{
    uint64_t x_bits = rc_bitcast_f64(x);

    if (rc_pow_is_unusual_f64(x_bits, rc_bitcast_f64(y))) {
        return rc_pow_unusual_f64(x, y);
    }

    return rc_pow_core_f64(x_bits, y, 0);
}
