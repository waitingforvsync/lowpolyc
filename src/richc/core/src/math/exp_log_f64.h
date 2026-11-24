// math/exp_log_f64.h
//
// What rc_exp_f64, rc_log_f64, rc_log2_f64 and rc_pow_f64 share: their tables and constants, the
// log reduction and the exp kernel. The kernels are inline, so each function keeps its own fast
// path, and so is exp's rare scaling near overflow and underflow, so that rc_exp_f64 calls
// nothing and V8 spares it a stack check.
//
// The constants live in the .c files with the tables, out of the compiler's sight, so that it
// loads them rather than writing them into the code. V8 builds a double from an immediate in two
// instructions where a load takes one, which costs wasm up to a tenth of its speed here; natively
// it makes no difference.
//
// log works on x = 2^k m, m in [1, 2). The top b bits of m pick c = j / 2^(b + 1) from a table,
// chosen so that r = m c - 1 is small, and integer arithmetic finds r exactly: m as a 53-bit
// integer times j is under 2^62, and is 2^(53 + b) (1 + r). Less 2^(53 + b) it is at most 2^53 in
// size, so it becomes a double exactly too. Then log(x) = k ln2 - log(c) + log1p(r).
//
// exp works on x = (128 k + j) ln2 / 128 + r, |r| <= ln2 / 256, as 2^k 2^(j/128) e^r, with
// 2^(j/128) from a table as hi (1 + lo) and e^r - 1 from a polynomial.

#ifndef RC_MATH_EXP_LOG_F64_H_
#define RC_MATH_EXP_LOG_F64_H_

#include "richc/math/math.h"

// Beyond these e^x rounds to infinity or to zero. The true thresholds are 709.7827 and -745.1332.
#define RC_EXP_F64_OVERFLOW  709.79
#define RC_EXP_F64_UNDERFLOW -745.2

// The bits of rc_exp_f64_data.fast, 700.
#define RC_EXP_F64_FAST_BITS 0x4085e00000000000ull

// The bits of infinity and of the smallest normal number, and the significand's mask.
#define RC_EXP_LOG_F64_INFINITY_BITS 0x7ff0000000000000ull
#define RC_EXP_LOG_F64_MIN_NORMAL    0x0010000000000000ull
#define RC_EXP_LOG_F64_MANTISSA_BITS 0x000fffffffffffffull

// The bits of 2^52. An exponent field put in their low bits makes the double 2^52 + exponent.
#define RC_EXP_LOG_F64_EXPONENT_BITS 0x4330000000000000ull

// A double carried as the unevaluated sum hi + lo, lo being the smaller.
typedef struct rc_exp_log_f64_pair {
    double hi;
    double lo;
} rc_exp_log_f64_pair;

// The constants of the exp kernel. Below fast in size, e^x and the 2^k it is built on are normal.
// x 128 / ln2 plus shift rounds to the nearest whole n. ln2 / 128 is step_hi, a multiple of 2^-42
// so that n times it is exact, plus step_lo. And e^r = 1 + r + r^2 (poly[0] + ... + poly[3] r^3).
typedef struct rc_exp_f64_constants {
    double fast;
    double inv_step;
    double shift;
    double step_hi;
    double step_lo;
    double poly[4];
} rc_exp_f64_constants;

// The constants of rc_log_f64, which rc_pow_f64 shares. k_bias, 2^52 + 1023, takes k from the
// double that the exponent field makes in the bits of 2^52, and r_scale, 2^-61, turns 2^61 r into
// r. ln2 is ln2_hi, a multiple of 2^-42 so that k times it is exact, plus ln2_lo. And
// log1p(r) = r - r^2 / 2 + r^3 (poly[0] + ... + poly[4] r^4).
typedef struct rc_log_f64_constants {
    double k_bias;
    double r_scale;
    double ln2_hi;
    double ln2_lo;
    double minus_half;
    double poly[5];
} rc_log_f64_constants;

// The constants of rc_log2_f64, which works on R = 2^61 r rather than r: each constant that
// multiplies a power of r is scaled by that power of 2^-61, which changes no rounding. k_bias as
// for log. 2^-61 / ln2 is inv_ln2 rounded, and also inv_ln2_hi, a multiple of 2^-87 so that a
// 26-bit R times it is exact, plus inv_ln2_lo. And
// log1p(r) / ln2 = R inv_ln2 + R^2 (poly[0] + ... + poly[5] R^5).
typedef struct rc_log2_f64_constants {
    double k_bias;
    double inv_ln2;
    double inv_ln2_hi;
    double inv_ln2_lo;
    double poly[6];
} rc_log2_f64_constants;

// The constants of rc_pow_f64's log. log1p(r) = r - r^2 / 2 + r^3 (poly[0] + ... + poly[5] r^5),
// and head_bias and tail_bias turn the doubles that r's two parts make into those parts. They are
// kept apart, or clang packs their two subtractions into one vector operation, costing shuffles.
typedef struct rc_pow_f64_constants {
    double head_bias;
    double poly[6];
    double tail_bias;
} rc_pow_f64_constants;

// e^(hi + lo) = 2^k t_hi (1 + tmp), where t_hi is 2^(j/128) rounded and n_bits are the bits of
// 1.5 2^52 + n, n = 128 k + j.
typedef struct rc_exp_f64_reduced {
    uint64_t n_bits;
    double t_hi;
    double tmp;
} rc_exp_f64_reduced;

// x = 2^k m reduced with a table of 2^b: k, the table index, and scaled, the 53-bit integer m
// times j, which is 2^(53 + b) (1 + r).
typedef struct rc_log_f64_reduced {
    double k;
    uint32_t index;
    uint64_t scaled;
} rc_log_f64_reduced;

// 2^(j/128) for j in [0, 128), as hi (1 + lo), and the exp kernel's constants.
extern const rc_exp_log_f64_pair rc_exp_f64_table[128];
extern const rc_exp_f64_constants rc_exp_f64_data;

// The j of each of log's 256 intervals, c being j / 512, -log(c) for each as a multiple of 2^-42
// and the rest, and the constants of log and of pow's log.
extern const uint16_t rc_log_f64_inverse[256];
extern const rc_exp_log_f64_pair rc_log_f64_table[256];
extern const rc_log_f64_constants rc_log_f64_data;
extern const rc_pow_f64_constants rc_pow_f64_data;

// The same j for log2, with -log2(c), and log2's constants.
extern const uint16_t rc_log2_f64_inverse[256];
extern const rc_exp_log_f64_pair rc_log2_f64_table[256];
extern const rc_log2_f64_constants rc_log2_f64_data;

// ---- exact steps ----

// a + b and its rounding error, exactly, whatever their sizes (Knuth's two-sum).
static inline rc_exp_log_f64_pair rc_exp_log_f64_sum(double a, double b)
{
    double sum = a + b;
    double b_part = sum - a;

    return (rc_exp_log_f64_pair) {.hi = sum, .lo = (a - (sum - b_part)) + (b - b_part)};
}

// x with its low bits cleared, keeping the top 53 - dropped bits of its significand. The rest,
// x minus this, is exact.
static inline double rc_exp_log_f64_head(double x, uint32_t dropped)
{
    return rc_bitcast_u64(rc_bitcast_f64(x) & (~0ull << dropped));
}

// ---- log ----

// True for the bits of a positive normal double: not zero, subnormal, infinite, NaN or negative.
static inline bool rc_exp_log_f64_is_positive_normal(uint64_t bits)
{
    return (uint32_t)(bits >> 52) - 1u < 2046u;
}

// True for the bits of a positive subnormal double.
static inline bool rc_exp_log_f64_is_positive_subnormal(uint64_t bits)
{
    return bits - 1 < RC_EXP_LOG_F64_MIN_NORMAL - 1;
}

// log(x) and log2(x) for the bits of an x zero, negative, infinite or NaN: -infinity, NaN, or
// x + x, which keeps infinity and quietens NaN. Masks choose between them, as branches would be
// mispredicted in mixes of special values.
static inline double rc_exp_log_f64_log_special(uint64_t bits)
{
    uint64_t negative = 0 - (bits >> 63);
    uint64_t zero = 0 - (uint64_t)((bits << 1) == 0);
    uint64_t sum = rc_bitcast_f64(rc_bitcast_u64(bits) + rc_bitcast_u64(bits));
    uint64_t nonzero = (sum & ~negative) | (0x7ff8000000000000ull & negative);

    return rc_bitcast_u64((nonzero & ~zero) | (0xfff0000000000000ull & zero));
}

// The bits of a positive normal x reduced with a table of 2^table_bits intervals. With k_bias
// raised by 1074, the bits of a subnormal x scaled by 2^1074 serve too.
static inline rc_log_f64_reduced rc_log_f64_reduce(uint64_t bits, uint32_t table_bits, const uint16_t *inverse, double k_bias)
{
    uint32_t index = (uint32_t)(bits >> (52 - table_bits)) & ((1u << table_bits) - 1u);
    uint64_t m = (bits & RC_EXP_LOG_F64_MANTISSA_BITS) | RC_EXP_LOG_F64_MIN_NORMAL;

    return (rc_log_f64_reduced) {
        .k = rc_bitcast_u64((bits >> 52) | RC_EXP_LOG_F64_EXPONENT_BITS) - k_bias,
        .index = index,
        .scaled = m * inverse[index]
    };
}

// 2^(53 + table_bits) r, exactly, from the scaled of a table of 2^table_bits.
static inline double rc_log_f64_scaled_r(uint64_t scaled, uint32_t table_bits)
{
    return (double)((int64_t)scaled - ((int64_t)1 << (53 + table_bits)));
}

// r, exactly, from the scaled of a table of 2^table_bits, r_scale being 2^-(53 + table_bits).
static inline double rc_log_f64_r(uint64_t scaled, uint32_t table_bits, double r_scale)
{
    return rc_log_f64_scaled_r(scaled, table_bits) * r_scale;
}

// ---- exp ----

// Reduces hi + lo, for |hi| <= 746 and |lo| <= 2^-14. Adding the shift leaves n in the low bits.
// hi less n step_hi is exact, and it is the only large part of tmp. It takes in the rest of r and
// the table's tail while the polynomial is on its way, and then the polynomial, so tmp rounds
// twice at its own size, under 2^-62.4 of 1 each time; summing the small terms first would round
// once, but take three cycles longer.
static inline rc_exp_f64_reduced rc_exp_f64_reduce(double hi, double lo)
{
    const rc_exp_f64_constants *c = &rc_exp_f64_data;
    double shifted = hi * c->inv_step + c->shift;
    double n = shifted - c->shift;
    uint64_t n_bits = rc_bitcast_f64(shifted);
    double r_head = hi - n * c->step_hi;
    double r_tail = n * c->step_lo - lo;
    double r = r_head - r_tail;
    rc_exp_log_f64_pair t = rc_exp_f64_table[n_bits & 127u];
    double r2 = r * r;
    double poly = r2 * (c->poly[0] + r * c->poly[1]) + (r2 * r2) * (c->poly[2] + r * c->poly[3]);

    return (rc_exp_f64_reduced) {.n_bits = n_bits, .t_hi = t.hi, .tmp = (r_head + (t.lo - r_tail)) + poly};
}

// e^(hi + lo) for |hi| < rc_exp_f64_data.fast and |lo| <= 2^-14, negated if sign is the sign bit
// rather than 0. n_bits shifted down by 7 is k plus a multiple of 2^12, which shifting up by 52
// drops, so adding it to t_hi's bits scales t_hi by 2^k, and adding the sign bit too negates it.
// Only scale (1 + tmp) rounds at the result's size, within 2^-59.4 of it before that.
static inline double rc_exp_f64_kernel(double hi, double lo, uint64_t sign)
{
    rc_exp_f64_reduced reduced = rc_exp_f64_reduce(hi, lo);
    double scale = rc_bitcast_u64(rc_bitcast_f64(reduced.t_hi) + ((reduced.n_bits >> 7) << 52) + sign);

    return scale + scale * reduced.tmp;
}

// 2^k (t_hi + corr) for k below -1021, where the result may be subnormal. Scaling a rounded sum
// would round twice, so we scale the parts first, by 2^(k + 1022), which keeps both normal. If
// their sum is then under 1 the result is subnormal, and adding 1 makes it round to the subnormal
// grid, a multiple of 2^-52 here; the 1 comes off exactly afterwards.
static inline double rc_exp_f64_scale_down(double t_hi, double corr, int32_t k)
{
    double scale = rc_bitcast_u64((uint64_t)(k + 1022 + 1023) << 52);
    double hi = t_hi * scale;
    double lo = corr * scale;
    rc_exp_log_f64_pair shifted = rc_exp_log_f64_sum(1.0, hi);

    if (hi + lo >= 1.0) {
        return (hi + lo) * 0x1p-1022;
    }

    return ((shifted.hi + (shifted.lo + lo)) - 1.0) * 0x1p-1022;
}

// e^(hi + lo) for hi in [RC_EXP_F64_UNDERFLOW, RC_EXP_F64_OVERFLOW], at least rc_exp_f64_data.fast
// in size, and |lo| <= 2^-14: rounded once, to a subnormal if it is one, or to infinity. Above the
// normal range the sum rounds as usual, and the scaling, in two steps since 2^1024 is no double,
// either fits or overflows. n_bits shifted down by 7 is k plus the shift's bits shifted down by 7;
// 2048 keeps the difference unsigned until it is small.
static inline double rc_exp_f64_extreme(double hi, double lo)
{
    rc_exp_f64_reduced reduced = rc_exp_f64_reduce(hi, lo);
    int32_t k = (int32_t)((reduced.n_bits >> 7) - (rc_bitcast_f64(rc_exp_f64_data.shift) >> 7) + 2048u) - 2048;
    double corr = reduced.t_hi * reduced.tmp;

    if (k > 0) {
        return (reduced.t_hi + corr) * rc_bitcast_u64((uint64_t)(k - 1 + 1023) << 52) * 2.0;
    }

    return rc_exp_f64_scale_down(reduced.t_hi, corr, k);
}

#endif // RC_MATH_EXP_LOG_F64_H_
