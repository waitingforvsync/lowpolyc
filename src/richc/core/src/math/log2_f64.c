// math/log2_f64.c
//
// rc_log2_f64: the base 2 logarithm. We write x = 2^k m and take c, near 1 / m, from a table of
// 256, so that log2(x) = k - log2(c) + log1p(r) / ln2 with r = m c - 1 found exactly and
// |r| < 2^-8 (the reduction in exp_log_f64.h), and log1p(r) / ln2 - r / ln2 from a degree 7
// polynomial within 2^-64.7 |r|. k and the head of -log2(c) sum exactly. We work on 2^61 r, which
// the reduction leaves as an integer, rather than r, saving a multiplication.
//
// Outside [1/2, 2), |log2(x)| is at least 1, so the rest can join that sum rounded: r / ln2,
// rounded with 1 / ln2, and the sum it joins each add under 2^-61, and before the final rounding
// we are within 2^-59.9 of log2(x), under 0.005 ulp. Inside, where log2(x) can be as small as
// r / ln2, a 26-bit head of r times a 27-bit head of 1 / ln2 is exact, and adding it keeps its
// error, so only small terms round: the rest of r / ln2, the tail of the table and the polynomial.
// There we are within about 2^-59.5 of log2(x), under 0.01 ulp. An exact power of 2 lands on k
// exactly either way. The worst error we have found is 0.504 ulp.

#include "richc/math/math.h"

#include "math/exp_log_f64.h"

// The bits of 1/2, and the width of [1/2, 2) in bits.
#define RC_LOG2_F64_NEAR_BITS  0x3fe0000000000000ull
#define RC_LOG2_F64_NEAR_WIDTH 0x0020000000000000ull

// log2(x) for the bits of a positive normal x outside [1/2, 2), with the k_bias that finds its k.
// r is 2^61 times the r above, and the constants are scaled to match. The polynomial comes in two
// parts, its terms to r^3 and r^4 times the rest, to keep the chain from r to the result short.
static double rc_log2_f64_far(uint64_t bits, double k_bias)
{
    const rc_log2_f64_constants *c = &rc_log2_f64_data;
    rc_log_f64_reduced reduced = rc_log_f64_reduce(bits, 8, rc_log2_f64_inverse, k_bias);
    rc_exp_log_f64_pair log2_c = rc_log2_f64_table[reduced.index];
    double r = rc_log_f64_scaled_r(reduced.scaled, 8);
    double head = reduced.k + log2_c.hi;
    double r2 = r * r;
    double upper = (c->poly[2] + r * c->poly[3]) + r2 * (c->poly[4] + r * c->poly[5]);
    double lower = log2_c.lo + r2 * (c->poly[0] + r * c->poly[1]);

    return head + (r * c->inv_ln2 + (lower + (r2 * r2) * upper));
}


// log2(x) for the bits of an x in [1/2, 2), r and the constants scaled as for the far path. The
// large terms sum to head + scaled, and scaled less its rounding error is exact (a fast two-sum:
// head is 0 or at least as big a power of 2 as any scaled of its interval).
static double rc_log2_f64_near(uint64_t bits)
{
    const rc_log2_f64_constants *c = &rc_log2_f64_data;
    rc_log_f64_reduced reduced = rc_log_f64_reduce(bits, 8, rc_log2_f64_inverse, c->k_bias);
    rc_exp_log_f64_pair log2_c = rc_log2_f64_table[reduced.index];
    double r = rc_log_f64_scaled_r(reduced.scaled, 8);
    double r_head = rc_exp_log_f64_head(r, 27);
    double head = reduced.k + log2_c.hi;
    double scaled = r_head * c->inv_ln2_hi;
    double sum = head + scaled;
    double r2 = r * r;
    double upper = (c->poly[2] + r * c->poly[3]) + r2 * (c->poly[4] + r * c->poly[5]);
    double lower = r2 * (c->poly[0] + r * c->poly[1]);
    double tail = ((r - r_head) * c->inv_ln2_hi + r * c->inv_ln2_lo) + log2_c.lo;

    return sum + (((scaled - (sum - head)) + tail) + (lower + (r2 * r2) * upper));
}


// log2(x) for the bits of an x zero, negative, subnormal, infinite or NaN. A subnormal's bits, as
// an integer, are x 2^1074, and converting them makes a normal double with no arithmetic on a
// subnormal, which some processors take a slow path for.
static double rc_log2_f64_outside(uint64_t bits)
{
    if (rc_exp_log_f64_is_positive_subnormal(bits)) {
        return rc_log2_f64_far(rc_bitcast_f64((double)(int64_t)bits), rc_log2_f64_data.k_bias + 1074.0);
    }

    return rc_exp_log_f64_log_special(bits);
}


double rc_log2_f64(double x)
{
    uint64_t bits = rc_bitcast_f64(x);

    if (!rc_exp_log_f64_is_positive_normal(bits)) {
        return rc_log2_f64_outside(bits);
    }

    if (bits - RC_LOG2_F64_NEAR_BITS < RC_LOG2_F64_NEAR_WIDTH) {
        return rc_log2_f64_near(bits);
    }

    return rc_log2_f64_far(bits, rc_log2_f64_data.k_bias);
}
