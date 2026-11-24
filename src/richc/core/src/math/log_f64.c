// math/log_f64.c
//
// rc_log_f64: the natural logarithm. We write x = 2^k m and take c, near 1 / m, from a table of
// 256, so that log(x) = k ln2 - log(c) + log1p(r) with r = m c - 1 found exactly and |r| < 2^-8
// (the reduction in exp_log_f64.h). The heads of k ln2 and -log(c) sum exactly, and adding r keeps
// its error, so only the small terms round: log1p(r) - r from a degree 7 polynomial within
// 2^-65.9 |r|, and the tails. Before the final rounding we are within about 2^-60 of log(x), under
// 0.01 ulp, and the worst error we have found is 0.503 ulp.

#include "richc/math/math.h"

#include "math/exp_log_f64.h"

// log(x) for the bits of a positive normal x, with the k_bias that finds its k. head + r is the
// sum of the large terms, and r less its rounding error is exact (a fast two-sum: head is 0 or at
// least as big a power of 2 as any r of its interval). The polynomial comes in two parts, its
// terms to r^3 and r^4 times the rest, to keep the chain from r to the result short.
static double rc_log_f64_normal(uint64_t bits, double k_bias)
{
    const rc_log_f64_constants *c = &rc_log_f64_data;
    rc_log_f64_reduced reduced = rc_log_f64_reduce(bits, 8, rc_log_f64_inverse, k_bias);
    rc_exp_log_f64_pair log_c = rc_log_f64_table[reduced.index];
    double r = rc_log_f64_r(reduced.scaled, 8, c->r_scale);
    double head = reduced.k * c->ln2_hi + log_c.hi;
    double sum = head + r;
    double r2 = r * r;
    double upper = (c->poly[1] + r * c->poly[2]) + r2 * (c->poly[3] + r * c->poly[4]);
    double tail = (r - (sum - head)) + (reduced.k * c->ln2_lo + log_c.lo);
    double lower = tail + r2 * (c->minus_half + r * c->poly[0]);

    return sum + (lower + (r2 * r2) * upper);
}


// A subnormal x joins the normal path as its bits, which as an integer are x 2^1074: converting
// them makes a normal double with no arithmetic on a subnormal, which some processors take a slow
// path for.
double rc_log_f64(double x)
{
    uint64_t bits = rc_bitcast_f64(x);
    double k_bias = rc_log_f64_data.k_bias;

    if (!rc_exp_log_f64_is_positive_normal(bits)) {
        if (!rc_exp_log_f64_is_positive_subnormal(bits)) {
            return rc_exp_log_f64_log_special(bits);
        }

        bits = rc_bitcast_f64((double)(int64_t)bits);
        k_bias += 1074.0;
    }

    return rc_log_f64_normal(bits, k_bias);
}
