// math/rational.c
//
// The rc_rational operations too big to inline. Products cancel the common factors across the two
// fractions before multiplying, and sums use the AHU method, so both land in lowest terms without
// a gcd of the whole result. Comparison and the conversion from double walk continued fractions.

#include "richc/math/rational.h"

// The quotient of a division rounded down, and the remainder that leaves, in [0, denominator).
typedef struct rc_rational_floor_division {
    int64_t quotient;
    int64_t remainder;
} rc_rational_floor_division;

// The next whole term of a continued fraction, and the fraction left after it.
typedef struct rc_rational_term {
    int64_t whole;
    double fraction;
} rc_rational_term;

// The last two convergents of a continued fraction, as numerators and denominators.
typedef struct rc_rational_convergents {
    int64_t num;
    int64_t denom;
    int64_t num_prev;
    int64_t denom_prev;
} rc_rational_convergents;

// num / denom, given a positive denom, divided through by their gcd.
static rc_rational rc_rational_reduce(int64_t num, int64_t denom)
{
    int64_t divisor = rc_gcd_i64(num, denom);

    return (rc_rational) {.num_ = num / divisor, .denom_ = denom / divisor};
}


rc_rational rc_rational_make(int64_t num, int64_t denom)
{
    if (denom == 0) {
        return (rc_rational) {0};
    }

    if (denom < 0) {
        RC_ASSERT(num != INT64_MIN);
        RC_ASSERT(denom != INT64_MIN);
        return rc_rational_reduce(-num, -denom);
    }

    return rc_rational_reduce(num, denom);
}


// num / denom with the sign moved to the numerator, both already being in lowest terms.
static rc_rational rc_rational_signed(int64_t num, int64_t denom)
{
    if (denom < 0) {
        RC_ASSERT(num != INT64_MIN);
        RC_ASSERT(denom != INT64_MIN);
        return (rc_rational) {.num_ = -num, .denom_ = -denom};
    }

    return (rc_rational) {.num_ = num, .denom_ = denom};
}


// a * b, where a is valid and b is in lowest terms but may have a negative denominator. Cancelling
// a.num against b.denom, and b.num against a.denom, first leaves a product in lowest terms.
static rc_rational rc_rational_product(rc_rational a, rc_rational b)
{
    int64_t gcd_an_bd = rc_gcd_i64(a.num_, b.denom_);
    int64_t gcd_bn_ad = rc_gcd_i64(b.num_, a.denom_);
    int64_t num = rc_rational_mul_checked_(a.num_ / gcd_an_bd, b.num_ / gcd_bn_ad);
    int64_t denom = rc_rational_mul_checked_(a.denom_ / gcd_bn_ad, b.denom_ / gcd_an_bd);

    return rc_rational_signed(num, denom);
}


rc_rational rc_rational_mul(rc_rational a, rc_rational b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    return rc_rational_product(a, b);
}


rc_rational rc_rational_int_mul(rc_rational a, int64_t b)
{
    return rc_rational_mul(a, rc_rational_from_i64(b));
}


// a times b turned upside down, which keeps b's sign on its new denominator until the end.
rc_rational rc_rational_div(rc_rational a, rc_rational b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    RC_ASSERT(b.num_ != 0);
    return rc_rational_product(a, (rc_rational) {.num_ = b.denom_, .denom_ = b.num_});
}


rc_rational rc_rational_int_div(rc_rational a, int64_t b)
{
    return rc_rational_div(a, rc_rational_from_i64(b));
}


// The end of the AHU method. numerator is over the least common denominator of a and b, which is
// a_part * b_denom / common, where common is the gcd of the denominators and a_part is a's
// denominator divided by it. Only common can share a factor with numerator, so dividing that out
// leaves lowest terms.
static rc_rational rc_rational_over_common(int64_t numerator, int64_t common, int64_t a_part, int64_t b_denom)
{
    int64_t divisor = rc_gcd_i64(numerator, common);

    return (rc_rational) {.num_ = numerator / divisor, .denom_ = rc_rational_mul_checked_(a_part, b_denom / divisor)};
}


rc_rational rc_rational_add(rc_rational a, rc_rational b)
{
    int64_t common = rc_gcd_i64(a.denom_, b.denom_);
    int64_t a_part = a.denom_ / common;
    int64_t b_part = b.denom_ / common;
    int64_t numerator = rc_rational_add_checked_(rc_rational_mul_checked_(a.num_, b_part), rc_rational_mul_checked_(b.num_, a_part));

    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    return rc_rational_over_common(numerator, common, a_part, b.denom_);
}


rc_rational rc_rational_sub(rc_rational a, rc_rational b)
{
    int64_t common = rc_gcd_i64(a.denom_, b.denom_);
    int64_t a_part = a.denom_ / common;
    int64_t b_part = b.denom_ / common;
    int64_t numerator = rc_rational_sub_checked_(rc_rational_mul_checked_(a.num_, b_part), rc_rational_mul_checked_(b.num_, a_part));

    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    return rc_rational_over_common(numerator, common, a_part, b.denom_);
}


// num / denom for a positive denom, rounded down rather than towards zero.
static rc_rational_floor_division rc_rational_floor_divide(rc_rational a)
{
    int64_t quotient = a.num_ / a.denom_;
    int64_t remainder = a.num_ % a.denom_;

    if (remainder < 0) {
        return (rc_rational_floor_division) {.quotient = quotient - 1, .remainder = remainder + a.denom_};
    }

    return (rc_rational_floor_division) {.quotient = quotient, .remainder = remainder};
}


// We walk the two continued fractions together. While the whole parts agree we move on to the
// fractional parts, both in (0, 1), by comparing their reciprocals, which reverses the order. The
// remainders shrink every time, Euclid-style, so it ends; and it only divides, so nothing
// overflows. Floor division keeps the remainders in [0, denom), which orders negatives properly.
int32_t rc_rational_compare(rc_rational a, rc_rational b)
{
    rc_rational x = a;
    rc_rational y = b;
    int32_t sign = 1;

    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    for (;;) {
        rc_rational_floor_division x_parts = rc_rational_floor_divide(x);
        rc_rational_floor_division y_parts = rc_rational_floor_divide(y);

        if (x_parts.quotient != y_parts.quotient) {
            return x_parts.quotient < y_parts.quotient ? -sign : sign;
        }

        if (x_parts.remainder == 0 && y_parts.remainder == 0) {
            return 0;
        }

        // With the whole parts equal, the one with nothing left over is the smaller
        if (x_parts.remainder == 0) {
            return -sign;
        }

        if (y_parts.remainder == 0) {
            return sign;
        }

        x = (rc_rational) {.num_ = x.denom_, .denom_ = x_parts.remainder};
        y = (rc_rational) {.num_ = y.denom_, .denom_ = y_parts.remainder};
        sign = -sign;
    }
}


// The whole part of 1 / fraction, and what is left. fraction is at least 1e-10, so the whole
// part fits.
static rc_rational_term rc_rational_next_term(double fraction)
{
    double reciprocal = 1.0 / fraction;
    int64_t whole = (int64_t)reciprocal;

    return (rc_rational_term) {.whole = whole, .fraction = reciprocal - (double)whole};
}


// True if whole * current + previous, the next numerator or denominator, would overflow.
static bool rc_rational_next_overflows(int64_t whole, int64_t current, int64_t previous)
{
    return current > 0 && whole > (INT64_MAX - previous) / current;
}


// True if the convergents after c, whose next term is whole, would overflow.
static bool rc_rational_convergents_overflow(rc_rational_convergents c, int64_t whole)
{
    return rc_rational_next_overflows(whole, c.num, c.num_prev) || rc_rational_next_overflows(whole, c.denom, c.denom_prev);
}


// The convergents after c, whose next term is whole.
static rc_rational_convergents rc_rational_next_convergents(rc_rational_convergents c, int64_t whole)
{
    return (rc_rational_convergents) {
        .num = whole * c.num + c.num_prev,
        .denom = whole * c.denom + c.denom_prev,
        .num_prev = c.num,
        .denom_prev = c.denom
    };
}


// True if the convergent c is within threshold of x.
static bool rc_rational_is_close(rc_rational_convergents c, double x, double threshold)
{
    return rc_abs_f64(x - (double)c.num / (double)c.denom) <= threshold;
}


// The simplest fraction within threshold of x, for a finite x in [0, INT64_MAX). Each convergent
// is the best approximation for the size of its denominator, so the first within threshold is the
// simplest. We stop early if the fractional part gets too small to invert, meaning x is rational at
// double precision, or the next convergent would overflow.
static rc_rational rc_rational_simplest_near(double x, double threshold)
{
    int64_t whole = (int64_t)x;
    double fraction = x - (double)whole;
    rc_rational_convergents c = {.num = whole, .denom = 1, .num_prev = 1, .denom_prev = 0};

    while (!rc_rational_is_close(c, x, threshold) && fraction >= 1e-10) {
        rc_rational_term next = rc_rational_next_term(fraction);

        if (rc_rational_convergents_overflow(c, next.whole)) {
            break;
        }

        c = rc_rational_next_convergents(c, next.whole);
        fraction = next.fraction;
    }

    return rc_rational_make(c.num, c.denom);
}


rc_rational rc_rational_from_double(double value, double threshold)
{
    double magnitude = rc_abs_f64(value);

    RC_ASSERT(rc_is_finite_f64(value));
    RC_ASSERT(magnitude < (double)INT64_MAX);
    RC_ASSERT(threshold >= 0.0);
    if (value < 0.0) {
        return rc_rational_negate(rc_rational_simplest_near(magnitude, threshold));
    }

    return rc_rational_simplest_near(magnitude, threshold);
}
