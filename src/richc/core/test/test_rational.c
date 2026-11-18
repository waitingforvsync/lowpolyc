// test_rational.c
//
// Tests for rc_rational, the exact fraction kept in lowest terms.

#include "richc/math/rational.h"

#include "richc/test.h"

// Consecutive Fibonacci numbers, the largest that fit in an int64_t. Ratios of them are the
// slowest to tell apart by continued fractions, and multiplying any two across overflows.
static const int64_t fib_90 = INT64_C(2880067194370816120);
static const int64_t fib_91 = INT64_C(4660046610375530309);
static const int64_t fib_92 = INT64_C(7540113804746346429);


// ---- construction ----

RC_TEST(rational, make_reduces)
{
    RC_CHECK(rc_rational_make(4, 2), ==, rc_rational_from_i64(2));
    RC_CHECK(rc_rational_make(6, 9), ==, rc_rational_make(2, 3));
    RC_CHECK(rc_rational_num(rc_rational_make(6, 9)), ==, 2);
    RC_CHECK(rc_rational_denom(rc_rational_make(6, 9)), ==, 3);
    RC_CHECK(rc_rational_make(0, 5), ==, rc_rational_from_i64(0));
    RC_CHECK(rc_rational_denom(rc_rational_make(0, -5)), ==, 1);
    RC_CHECK(rc_rational_make(INT64_MAX, INT64_MAX), ==, rc_rational_from_i64(1));
    RC_CHECK(rc_rational_make(INT64_MIN, 2), ==, rc_rational_from_i64(INT64_MIN / 2));
}


RC_TEST(rational, make_moves_the_sign_up)
{
    RC_CHECK(rc_rational_make(6, -8), ==, rc_rational_make(-3, 4));
    RC_CHECK(rc_rational_make(-6, -8), ==, rc_rational_make(3, 4));
    RC_CHECK(rc_rational_make(-6, 8), ==, rc_rational_make(-3, 4));
    RC_CHECK(rc_rational_num(rc_rational_make(6, -8)), ==, -3);
    RC_CHECK(rc_rational_denom(rc_rational_make(6, -8)), ==, 4);
    RC_CHECK(rc_rational_make(INT64_MAX, -1), ==, rc_rational_from_i64(-INT64_MAX));
}


RC_TEST(rational, make_by_zero_is_invalid)
{
    rc_rational invalid = rc_rational_make(5, 0);

    // Read through the accessors, since comparing an invalid fraction asserts
    RC_CHECK_FALSE(rc_rational_is_valid(invalid));
    RC_CHECK(rc_rational_num(invalid), ==, 0);
    RC_CHECK(rc_rational_denom(invalid), ==, 0);
    RC_CHECK_FALSE(rc_rational_is_valid(rc_rational_make(0, 0)));
    RC_CHECK_FALSE(rc_rational_is_valid((rc_rational) {0}));
}


RC_TEST(rational, from_i64)
{
    RC_CHECK(rc_rational_from_i64(7), ==, rc_rational_make(7, 1));
    RC_CHECK(rc_rational_from_i64(-5), ==, rc_rational_make(-5, 1));
    RC_CHECK(rc_rational_denom(rc_rational_from_i64(INT64_MIN)), ==, 1);
    RC_CHECK_TRUE(rc_rational_is_valid(rc_rational_from_i64(INT64_MIN)));
    RC_CHECK_TRUE(rc_rational_is_valid(rc_rational_from_i64(0)));
}


RC_TEST(rational, from_double)
{
    RC_CHECK(rc_rational_from_double(0.5, 1e-9), ==, rc_rational_make(1, 2));
    RC_CHECK(rc_rational_from_double(0.1, 1e-9), ==, rc_rational_make(1, 10));
    RC_CHECK(rc_rational_from_double(-0.75, 1e-9), ==, rc_rational_make(-3, 4));
    RC_CHECK(rc_rational_from_double(2.5, 0.0), ==, rc_rational_make(5, 2));
    RC_CHECK(rc_rational_from_double(3.0, 0.0), ==, rc_rational_from_i64(3));
    RC_CHECK(rc_rational_from_double(-3.0, 0.0), ==, rc_rational_from_i64(-3));
    RC_CHECK(rc_rational_from_double(0.0, 0.0), ==, rc_rational_from_i64(0));
    RC_CHECK(rc_rational_from_double(1e15, 0.0), ==, rc_rational_from_i64(INT64_C(1000000000000000)));

    // A loose threshold finds the simplest fraction nearby
    RC_CHECK(rc_rational_from_double(0.3333333, 1e-4), ==, rc_rational_make(1, 3));
    RC_CHECK(rc_rational_from_double(3.14159265358979, 0.01), ==, rc_rational_make(22, 7));
    RC_CHECK(rc_rational_from_double(3.14159265358979, 1e-6), ==, rc_rational_make(355, 113));
    RC_CHECK(rc_rational_from_double(-3.14159265358979, 0.5), ==, rc_rational_from_i64(-3));
}


// ---- predicates ----

RC_TEST(rational, predicates)
{
    RC_CHECK_TRUE(rc_rational_is_valid(rc_rational_make(3, 4)));
    RC_CHECK_TRUE(rc_rational_is_zero(rc_rational_from_i64(0)));
    RC_CHECK_TRUE(rc_rational_is_zero(rc_rational_make(0, -7)));
    RC_CHECK_FALSE(rc_rational_is_zero(rc_rational_make(1, 2)));
    RC_CHECK_TRUE(rc_rational_is_integer(rc_rational_make(8, 4)));
    RC_CHECK_TRUE(rc_rational_is_integer(rc_rational_make(-8, 4)));
    RC_CHECK_TRUE(rc_rational_is_integer(rc_rational_from_i64(0)));
    RC_CHECK_FALSE(rc_rational_is_integer(rc_rational_make(3, 2)));
    RC_CHECK_TRUE(rc_rational_is_positive(rc_rational_make(1, 2)));
    RC_CHECK_FALSE(rc_rational_is_positive(rc_rational_make(-1, 2)));
    RC_CHECK_FALSE(rc_rational_is_positive(rc_rational_from_i64(0)));
    RC_CHECK_TRUE(rc_rational_is_negative(rc_rational_make(-1, 2)));
    RC_CHECK_TRUE(rc_rational_is_negative(rc_rational_make(1, -2)));
    RC_CHECK_FALSE(rc_rational_is_negative(rc_rational_from_i64(0)));
}


// ---- arithmetic ----

RC_TEST(rational, negate_abs_reciprocal)
{
    RC_CHECK(rc_rational_negate(rc_rational_make(3, 4)), ==, rc_rational_make(-3, 4));
    RC_CHECK(rc_rational_negate(rc_rational_make(-3, 4)), ==, rc_rational_make(3, 4));
    RC_CHECK(rc_rational_negate(rc_rational_from_i64(0)), ==, rc_rational_from_i64(0));
    RC_CHECK(rc_rational_negate(rc_rational_from_i64(INT64_MAX)), ==, rc_rational_from_i64(-INT64_MAX));
    RC_CHECK(rc_rational_abs(rc_rational_make(-3, 4)), ==, rc_rational_make(3, 4));
    RC_CHECK(rc_rational_abs(rc_rational_make(3, 4)), ==, rc_rational_make(3, 4));
    RC_CHECK(rc_rational_abs(rc_rational_from_i64(-INT64_MAX)), ==, rc_rational_from_i64(INT64_MAX));

    // The sign stays on the numerator
    RC_CHECK(rc_rational_reciprocal(rc_rational_make(3, 4)), ==, rc_rational_make(4, 3));
    RC_CHECK(rc_rational_reciprocal(rc_rational_make(-3, 4)), ==, rc_rational_make(-4, 3));
    RC_CHECK(rc_rational_denom(rc_rational_reciprocal(rc_rational_make(-3, 4))), ==, 3);
    RC_CHECK(rc_rational_reciprocal(rc_rational_from_i64(-INT64_MAX)), ==, rc_rational_make(-1, INT64_MAX));
}


RC_TEST(rational, mul)
{
    RC_CHECK(rc_rational_mul(rc_rational_make(2, 3), rc_rational_make(3, 4)), ==, rc_rational_make(1, 2));
    RC_CHECK(rc_rational_mul(rc_rational_make(-2, 3), rc_rational_make(3, 4)), ==, rc_rational_make(-1, 2));
    RC_CHECK(rc_rational_mul(rc_rational_make(-2, 3), rc_rational_make(-3, 4)), ==, rc_rational_make(1, 2));
    RC_CHECK(rc_rational_mul(rc_rational_make(2, 3), rc_rational_from_i64(0)), ==, rc_rational_from_i64(0));
    RC_CHECK(rc_rational_int_mul(rc_rational_make(3, 4), 2), ==, rc_rational_make(3, 2));
    RC_CHECK(rc_rational_int_mul(rc_rational_make(3, 4), -2), ==, rc_rational_make(-3, 2));
    RC_CHECK(rc_rational_int_mul(rc_rational_make(3, 4), 0), ==, rc_rational_from_i64(0));

    // Cancelling across first keeps products in range that would overflow multiplied out
    RC_CHECK(rc_rational_int_mul(rc_rational_make(INT64_MAX, 2), 2), ==, rc_rational_from_i64(INT64_MAX));
    RC_CHECK(rc_rational_mul(rc_rational_make(fib_92, fib_91), rc_rational_make(fib_91, fib_92)), ==, rc_rational_from_i64(1));
}


RC_TEST(rational, div)
{
    RC_CHECK(rc_rational_div(rc_rational_make(2, 3), rc_rational_make(4, 5)), ==, rc_rational_make(5, 6));
    RC_CHECK(rc_rational_div(rc_rational_make(2, 3), rc_rational_make(-4, 5)), ==, rc_rational_make(-5, 6));
    RC_CHECK(rc_rational_div(rc_rational_make(-2, 3), rc_rational_make(-4, 5)), ==, rc_rational_make(5, 6));
    RC_CHECK(rc_rational_div(rc_rational_from_i64(0), rc_rational_make(-4, 5)), ==, rc_rational_from_i64(0));
    RC_CHECK(rc_rational_div(rc_rational_make(fib_92, 3), rc_rational_make(fib_92, 3)), ==, rc_rational_from_i64(1));
}


RC_TEST(rational, int_div)
{
    RC_CHECK(rc_rational_int_div(rc_rational_make(3, 4), 2), ==, rc_rational_make(3, 8));
    RC_CHECK(rc_rational_int_div(rc_rational_make(3, 4), 3), ==, rc_rational_make(1, 4));

    // By a negative, the sign still ends on the numerator
    RC_CHECK(rc_rational_int_div(rc_rational_make(3, 4), -2), ==, rc_rational_make(-3, 8));
    RC_CHECK(rc_rational_int_div(rc_rational_make(-3, 4), -3), ==, rc_rational_make(1, 4));
    RC_CHECK(rc_rational_int_div(rc_rational_make(6, 1), -3), ==, rc_rational_from_i64(-2));
    RC_CHECK(rc_rational_denom(rc_rational_int_div(rc_rational_make(3, 4), -2)), ==, 8);
    RC_CHECK(rc_rational_int_div(rc_rational_from_i64(0), -3), ==, rc_rational_from_i64(0));
    RC_CHECK(rc_rational_int_div(rc_rational_from_i64(INT64_MAX), -INT64_MAX), ==, rc_rational_from_i64(-1));
}


RC_TEST(rational, add_sub)
{
    RC_CHECK(rc_rational_add(rc_rational_make(1, 2), rc_rational_make(1, 3)), ==, rc_rational_make(5, 6));
    RC_CHECK(rc_rational_sub(rc_rational_make(1, 2), rc_rational_make(1, 3)), ==, rc_rational_make(1, 6));
    RC_CHECK(rc_rational_add(rc_rational_make(-1, 2), rc_rational_make(1, 3)), ==, rc_rational_make(-1, 6));
    RC_CHECK(rc_rational_sub(rc_rational_make(1, 3), rc_rational_make(1, 2)), ==, rc_rational_make(-1, 6));

    // The results land in lowest terms, through denominators that share a factor
    RC_CHECK(rc_rational_add(rc_rational_make(1, 6), rc_rational_make(1, 6)), ==, rc_rational_make(1, 3));
    RC_CHECK(rc_rational_add(rc_rational_make(1, 6), rc_rational_make(1, 10)), ==, rc_rational_make(4, 15));
    RC_CHECK(rc_rational_add(rc_rational_make(1, 4), rc_rational_make(3, 4)), ==, rc_rational_from_i64(1));
    RC_CHECK(rc_rational_sub(rc_rational_make(1, 2), rc_rational_make(1, 2)), ==, rc_rational_from_i64(0));
}


RC_TEST(rational, int_add_sub)
{
    RC_CHECK(rc_rational_int_add(rc_rational_make(3, 4), 2), ==, rc_rational_make(11, 4));
    RC_CHECK(rc_rational_int_sub(rc_rational_make(3, 4), 2), ==, rc_rational_make(-5, 4));
    RC_CHECK(rc_rational_int_add(rc_rational_make(3, 4), -1), ==, rc_rational_make(-1, 4));
    RC_CHECK(rc_rational_int_sub(rc_rational_make(-3, 4), -1), ==, rc_rational_make(1, 4));

    // Up to the limits, but not past them
    RC_CHECK(rc_rational_int_add(rc_rational_from_i64(INT64_MAX - 1), 1), ==, rc_rational_from_i64(INT64_MAX));
    RC_CHECK(rc_rational_int_sub(rc_rational_from_i64(INT64_MIN + 1), 1), ==, rc_rational_from_i64(INT64_MIN));
    RC_CHECK(rc_rational_add(rc_rational_from_i64(INT64_MAX), rc_rational_from_i64(INT64_MIN)), ==, rc_rational_from_i64(-1));
}


// ---- comparison ----

RC_TEST(rational, compare)
{
    rc_rational half = rc_rational_make(1, 2);
    rc_rational third = rc_rational_make(1, 3);

    RC_CHECK(rc_rational_compare(half, third), ==, 1);
    RC_CHECK(rc_rational_compare(third, half), ==, -1);
    RC_CHECK(rc_rational_compare(half, rc_rational_make(2, 4)), ==, 0);
    RC_CHECK(half, >, third);
    RC_CHECK(third, <, half);
    RC_CHECK(half, ==, rc_rational_make(2, 4));
    RC_CHECK(half, !=, third);
    RC_CHECK(half, >=, rc_rational_make(2, 4));
    RC_CHECK(half, <=, rc_rational_make(2, 4));
    RC_CHECK(third, <=, half);
    RC_CHECK_TRUE(rc_rational_is_less_than(third, half));
    RC_CHECK_FALSE(rc_rational_is_less_than(half, half));
    RC_CHECK_TRUE(rc_rational_is_greater_than(half, third));
    RC_CHECK_FALSE(rc_rational_is_greater_than(half, half));
    RC_CHECK(rc_rational_min(half, third), ==, third);
    RC_CHECK(rc_rational_max(half, third), ==, half);
    RC_CHECK(rc_rational_min(half, half), ==, half);
}


RC_TEST(rational, compare_negatives)
{
    // Floor division keeps the whole parts in order below zero
    RC_CHECK(rc_rational_make(-3, 4), <, rc_rational_make(-2, 3));
    RC_CHECK(rc_rational_make(-1, 2), <, rc_rational_make(-1, 3));
    RC_CHECK(rc_rational_make(-1, 2), <, rc_rational_make(1, 3));
    RC_CHECK(rc_rational_make(-1, 3), <, rc_rational_from_i64(0));
    RC_CHECK(rc_rational_make(-1, 3), >, rc_rational_from_i64(-1));
    RC_CHECK(rc_rational_make(-4, 3), <, rc_rational_from_i64(-1));
    RC_CHECK(rc_rational_make(-7, 2), <, rc_rational_make(-10, 3));
    RC_CHECK(rc_rational_compare(rc_rational_make(-7, 2), rc_rational_make(-14, 4)), ==, 0);
}


RC_TEST(rational, compare_huge)
{
    rc_rational below_phi = rc_rational_make(fib_92, fib_91);
    rc_rational above_phi = rc_rational_make(fib_91, fib_90);

    // Same denominator, numerators near INT64_MAX: num * denom would overflow
    RC_CHECK(rc_rational_make(INT64_MAX, 5), >, rc_rational_make(INT64_MAX - 1, 5));
    RC_CHECK(rc_rational_make(INT64_MAX - 1, 5), <, rc_rational_make(INT64_MAX, 5));

    // Different denominators, where the cross products are near 5e19
    RC_CHECK(rc_rational_make(INT64_C(7000000000000000000), 3), >, rc_rational_make(INT64_C(7000000000000000001), 7));

    // The two ratios share about 90 terms of their continued fractions
    RC_CHECK(below_phi, <, above_phi);
    RC_CHECK(above_phi, >, below_phi);
    RC_CHECK(below_phi, ==, below_phi);
    RC_CHECK(rc_rational_compare(below_phi, below_phi), ==, 0);
    RC_CHECK(rc_rational_negate(below_phi), >, rc_rational_negate(above_phi));

    // The far ends of the range
    RC_CHECK(rc_rational_from_i64(INT64_MIN), <, rc_rational_from_i64(INT64_MAX));
    RC_CHECK(rc_rational_from_i64(INT64_MIN), <, rc_rational_from_i64(INT64_MIN + 1));
    RC_CHECK(rc_rational_make(INT64_MIN + 1, 7), <, rc_rational_make(INT64_MIN + 2, 7));
    RC_CHECK(rc_rational_make(1, INT64_MAX), >, rc_rational_from_i64(0));
    RC_CHECK(rc_rational_make(1, INT64_MAX), <, rc_rational_make(1, INT64_MAX - 1));
    RC_CHECK(rc_rational_make(-1, INT64_MAX), >, rc_rational_make(-1, INT64_MAX - 1));
}


// ---- conversion ----

RC_TEST(rational, to_double)
{
    RC_CHECK(rc_rational_to_double(rc_rational_make(1, 2)), ==, 0.5);
    RC_CHECK(rc_rational_to_double(rc_rational_make(-3, 4)), ==, -0.75);
    RC_CHECK(rc_rational_to_double(rc_rational_from_i64(7)), ==, 7.0);
    RC_CHECK(rc_rational_to_double(rc_rational_from_i64(0)), ==, 0.0);
    RC_CHECK(rc_rational_to_double(rc_rational_make(1, 3)), ==, 1.0 / 3.0);
    RC_CHECK(rc_rational_to_double(rc_rational_from_i64(INT64_MIN)), ==, -9223372036854775808.0);
}
