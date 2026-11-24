// math/pow_f64.c
//
// rc_pow_f64: x^y, as e^(y log|x|), with C's rules for the special cases and the sign.
//
// y log|x| can be as large as 745, so log|x| needs to be good to around 2^-69 for the product to
// be good to 2^-60. We reduce as rc_log_f64 does, to k ln2 - log(c) + log1p(r) with r exact and
// |r| < 2^-8, and carry the large terms with their errors: the head of k ln2 - log(c), r, and the
// exact -r^2 / 2 of a 22-bit head of r. r^3 times a degree 5 polynomial within 2^-74 |r| does the
// rest, and its rounding, under 2^-77, is the largest error left. log|x| comes out as a 26-bit
// head and a rest within about 2^-69 |log|x||, the head early, so that the exp kernel in
// exp_log_f64.h can start on y times it, exact, while the rest catches up; it adds four errors
// under 2^-61.4 of the result. Before the final rounding we are within about 2^-58.5 of x^y, under
// 0.025 ulp, and the worst error we have found is 0.511 ulp.
//
// A subnormal x goes the same way, scaled into the normals first, and so does a negative x with an
// integer y, as |x|: the exp kernel puts the sign of an odd power on the scale it builds. The
// special cases come in mixes, so they are sorted with few branches.

#include "richc/math/math.h"

#include "math/exp_log_f64.h"

#define RC_POW_F64_INFINITY rc_bitcast_u64(RC_EXP_LOG_F64_INFINITY_BITS)
#define RC_POW_F64_SIGN_BIT 0x8000000000000000ull

// The bits of 2^23 and of 1.5 2^-9, in whose low bits r's head and tail become doubles.
#define RC_POW_F64_HEAD_BITS 0x4160000000000000ull
#define RC_POW_F64_TAIL_BITS 0x3f68000000000000ull

// The bits of a positive normal x, and the k_bias that finds its exponent k from them.
typedef struct rc_pow_f64_base {
    uint64_t bits;
    double k_bias;
} rc_pow_f64_base;

// r = m c - 1 as head + tail, both exact, from the reduction's scaled, 2^61 (1 + r). scaled
// rounded to a multiple of 2^32 makes, in the low bits of 2^23, 2^23 + 1 + head: head is r to the
// nearest 2^-29, of at most 22 bits. The rest, under 2^31 in size, added to the bits of 1.5 2^-9
// makes 1.5 2^-9 + tail. The integers keep it free of int-to-double conversions, which clang
// leaves with a false dependency that serialises calls in a loop.
static rc_exp_log_f64_pair rc_pow_f64_split_r(uint64_t scaled)
{
    const rc_pow_f64_constants *c = &rc_pow_f64_data;
    uint64_t upper = (scaled + 0x80000000ull) >> 32;
    double head = rc_bitcast_u64(upper | RC_POW_F64_HEAD_BITS) - c->head_bias;
    double tail = rc_bitcast_u64(RC_POW_F64_TAIL_BITS + (scaled - (upper << 32))) - c->tail_bias;

    return (rc_exp_log_f64_pair) {.hi = head, .lo = tail};
}


// log(x) as head + rest, the head of 26 bits and the rest under 2^-25 of it, for a positive normal
// x given as its base. first and second sum the head of k ln2 - log(c), r and the square of r's
// head, each smaller than the sum before it, so a fast two-sum finds each error. The head comes
// early, from second and the polynomial's leading terms. second less it is exact, and the
// polynomial, the errors, the tails of the table and the rest of the square,
// -(r - head) (r + head) / 2, join that to make the rest, which rounds only at its own small size.
static rc_exp_log_f64_pair rc_pow_f64_log(rc_pow_f64_base base)
{
    const rc_log_f64_constants *c = &rc_log_f64_data;
    const double *p = rc_pow_f64_data.poly;
    rc_log_f64_reduced reduced = rc_log_f64_reduce(base.bits, 8, rc_log_f64_inverse, base.k_bias);
    rc_exp_log_f64_pair log_c = rc_log_f64_table[reduced.index];
    rc_exp_log_f64_pair split = rc_pow_f64_split_r(reduced.scaled);
    double r = split.hi + split.lo;
    double head = reduced.k * c->ln2_hi + log_c.hi;
    double first = head + r;
    double square = split.hi * (c->minus_half * split.hi);
    double second = first + square;
    double r2 = r * r;
    double r3 = r2 * r;
    double leading = r3 * (p[0] + r * p[1]);
    double trailing = (r3 * r2) * ((p[2] + r * p[3]) + r2 * (p[4] + r * p[5]));
    double log_head = rc_exp_log_f64_head(second + leading, 27);
    double errors = (r - (first - head)) + (square - (second - first));
    double tail = (c->minus_half * split.lo) * (r + split.hi) + (reduced.k * c->ln2_lo + log_c.lo);
    double rest = ((second - log_head) + (leading + trailing)) + (errors + tail);

    return (rc_exp_log_f64_pair) {.hi = log_head, .lo = rest};
}


// y (head + rest) as hi + lo. The 26-bit heads of y and log(x) have an exact product, and the
// rest is under 2^-24 of it.
static rc_exp_log_f64_pair rc_pow_f64_times(double y, rc_exp_log_f64_pair log_x)
{
    double y_head = rc_exp_log_f64_head(y, 27);

    return (rc_exp_log_f64_pair) {.hi = y_head * log_x.hi, .lo = (y - y_head) * log_x.hi + y * log_x.lo};
}


// e^(hi + lo) for hi at least rc_exp_f64_data.fast in size: infinity, zero or the careful scaling.
static double rc_pow_f64_extreme(rc_exp_log_f64_pair exponent)
{
    if (exponent.hi > RC_EXP_F64_OVERFLOW) {
        return RC_POW_F64_INFINITY;
    }

    if (exponent.hi < RC_EXP_F64_UNDERFLOW) {
        return 0.0;
    }

    return rc_exp_f64_extreme(exponent.hi, exponent.lo);
}


// True if |hi| is below rc_exp_f64_data.fast, for the exp kernel; false for NaN too. The test is
// on the bits less the sign, which order as the magnitudes do, and spares gcc a spilled mask.
static bool rc_pow_f64_is_fast(double hi)
{
    return (rc_bitcast_f64(hi) << 1) < (RC_EXP_F64_FAST_BITS << 1);
}


// x^y for a positive finite x, given as its base, and any finite y, negated if sign is the sign
// bit rather than 0.
static double rc_pow_f64_positive(rc_pow_f64_base base, double y, uint64_t sign)
{
    rc_exp_log_f64_pair exponent = rc_pow_f64_times(y, rc_pow_f64_log(base));

    if (!rc_pow_f64_is_fast(exponent.hi)) {
        return rc_bitcast_u64(rc_bitcast_f64(rc_pow_f64_extreme(exponent)) | sign);
    }

    return rc_exp_f64_kernel(exponent.hi, exponent.lo, sign);
}


// The sign bit if y is an odd integer, else 0, for a finite y. y less twice the integer part of
// y / 2 is exact, and 1 in size just when y is odd: every double of 2^53 or more is even, as is its
// half.
static uint64_t rc_pow_f64_odd_sign(double y)
{
    double rest = y - 2.0 * rc_trunc_f64(y * 0.5);

    return (uint64_t)(rc_abs_f64(rest) == 1.0) << 63;
}


// x^y for y zero, infinite or NaN. Beyond the cases that give 1 or NaN, y is infinite, and
// x^y is 1 for |x| = 1, and otherwise infinity or 0 by whether |x| < 1 and y < 0 agree.
static double rc_pow_f64_special_y(double x, double y)
{
    double magnitude = rc_abs_f64(x);

    if (y == 0.0 || x == 1.0) {
        return 1.0;
    }

    if (x != x || y != y) {
        return x + y;
    }

    if (magnitude == 1.0) {
        return 1.0;
    }

    return (magnitude < 1.0) == (y < 0.0) ? RC_POW_F64_INFINITY : 0.0;
}


// True if y is finite and not zero: its bits without the sign, less 1, are below infinity's.
static bool rc_pow_f64_is_ordinary(double y)
{
    return (rc_bitcast_f64(y) << 1) - 1 < (RC_EXP_LOG_F64_INFINITY_BITS << 1) - 1;
}


// True for the bits of a positive finite x, normal or subnormal.
static bool rc_pow_f64_is_positive_finite(uint64_t bits)
{
    return bits - 1 < RC_EXP_LOG_F64_INFINITY_BITS - 1;
}


// True for the bits of a negative finite x, normal or subnormal.
static bool rc_pow_f64_is_negative_finite(uint64_t bits)
{
    return bits - (RC_POW_F64_SIGN_BIT + 1) < RC_EXP_LOG_F64_INFINITY_BITS - 1;
}


// The bits of a positive finite x, scaled into the normals if it is subnormal, and the k_bias
// that finds its k from them. A subnormal's bits, as an integer, are x 2^1074, and converting them
// makes a normal double with no arithmetic on a subnormal, which some processors take a slow path
// for.
static rc_pow_f64_base rc_pow_f64_base_of(uint64_t bits)
{
    if (bits < RC_EXP_LOG_F64_MIN_NORMAL) {
        return (rc_pow_f64_base) {.bits = rc_bitcast_f64((double)(int64_t)bits), .k_bias = rc_log_f64_data.k_bias + 1074.0};
    }

    return (rc_pow_f64_base) {.bits = bits, .k_bias = rc_log_f64_data.k_bias};
}


// True if x is negative and finite and y an integer, given the bits of x.
static bool rc_pow_f64_is_negative_to_integer(uint64_t x_bits, double y)
{
    return rc_pow_f64_is_negative_finite(x_bits) && rc_trunc_f64(y) == y;
}


// x^y for a finite, nonzero y and x zero, infinite, NaN, or negative and finite with y no
// integer, which has no real power. Otherwise |x| is 0 or infinity, and if y is negative, flipping
// the bits of infinity in it turns it into the other; the sign of x stays if y is odd.
static double rc_pow_f64_special_x(double x, double y)
{
    uint64_t x_bits = rc_bitcast_f64(x);
    uint64_t flip = (rc_bitcast_f64(y) >> 63) * RC_EXP_LOG_F64_INFINITY_BITS;

    if (rc_pow_f64_is_negative_finite(x_bits)) {
        return rc_bitcast_u64(0x7ff8000000000000ull);
    }

    if (x != x) {
        return x + y;
    }

    return rc_bitcast_u64(((x_bits & ~RC_POW_F64_SIGN_BIT) ^ flip) | (x_bits & rc_pow_f64_odd_sign(y)));
}


// A negative x with an integer y takes the fast path as |x|, its sign put back by the exp kernel
// if y is odd.
double rc_pow_f64(double x, double y)
{
    uint64_t x_bits = rc_bitcast_f64(x);
    uint64_t sign = 0;

    if (!rc_pow_f64_is_ordinary(y)) {
        return rc_pow_f64_special_y(x, y);
    }

    if (!rc_pow_f64_is_positive_finite(x_bits)) {
        if (!rc_pow_f64_is_negative_to_integer(x_bits, y)) {
            return rc_pow_f64_special_x(x, y);
        }

        sign = rc_pow_f64_odd_sign(y);
        x_bits -= RC_POW_F64_SIGN_BIT;
    }

    return rc_pow_f64_positive(rc_pow_f64_base_of(x_bits), y, sign);
}
