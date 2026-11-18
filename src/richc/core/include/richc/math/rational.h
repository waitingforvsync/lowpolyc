// math/rational.h
//
// An exact fraction of two int64_ts, always in lowest terms with a positive denominator, so zero is
// 0/1. Dividing by zero gives 0/0, which isn't valid. The members are internal (with a trailing
// underscore), since only the functions here keep that form: read a value through rc_rational_num
// and rc_rational_denom, and build one through the constructors.
//
// Every operation asserts its inputs are valid and its result fits in an int64_t. Products and
// sums cancel common factors first to keep the numbers small, and comparison never multiplies, so
// only a result that really doesn't fit fails.

#ifndef RC_MATH_RATIONAL_H_
#define RC_MATH_RATIONAL_H_

#include "richc/math/math.h"

typedef struct rc_rational {
    int64_t num_;
    int64_t denom_;
} rc_rational;

// ---- checked arithmetic ----

// a * b, which must fit.
static inline int64_t rc_rational_mul_checked_(int64_t a, int64_t b)
{
    RC_ASSERT(!rc_mul_overflows_i64(a, b));
    return a * b;
}

// a + b, which must fit.
static inline int64_t rc_rational_add_checked_(int64_t a, int64_t b)
{
    RC_ASSERT(!rc_add_overflows_i64(a, b));
    return a + b;
}

// a - b, which must fit.
static inline int64_t rc_rational_sub_checked_(int64_t a, int64_t b)
{
    RC_ASSERT(!rc_sub_overflows_i64(a, b));
    return a - b;
}

// ---- construction ----

// num / denom in lowest terms, or the invalid 0/0 if denom is 0.
rc_rational rc_rational_make(int64_t num, int64_t denom);

// The whole number num, as num/1.
static inline rc_rational rc_rational_from_i64(int64_t num)
{
    return (rc_rational) {.num_ = num, .denom_ = 1};
}

// The simplest fraction within threshold of value, by continued fractions. value must be finite
// and less than INT64_MAX in size, and threshold must not be negative.
rc_rational rc_rational_from_double(double value, double threshold);

// ---- accessors ----

// The numerator, which carries the sign.
static inline int64_t rc_rational_num(rc_rational a)
{
    return a.num_;
}

// The denominator, which is positive for a valid fraction.
static inline int64_t rc_rational_denom(rc_rational a)
{
    return a.denom_;
}

// ---- predicates ----

// True if a has a positive denominator and is in lowest terms.
static inline bool rc_rational_is_valid(rc_rational a)
{
    return a.denom_ > 0 && rc_gcd_i64(a.num_, a.denom_) == 1;
}

// True if a is zero.
static inline bool rc_rational_is_zero(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    return a.num_ == 0;
}

// True if a is a whole number.
static inline bool rc_rational_is_integer(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    return a.denom_ == 1;
}

// True if a is above zero.
static inline bool rc_rational_is_positive(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    return a.num_ > 0;
}

// True if a is below zero.
static inline bool rc_rational_is_negative(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    return a.num_ < 0;
}

// ---- arithmetic ----

// -a. Its numerator must not be INT64_MIN.
static inline rc_rational rc_rational_negate(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(a.num_ != INT64_MIN);
    return (rc_rational) {.num_ = -a.num_, .denom_ = a.denom_};
}

// a without its sign. Its numerator must not be INT64_MIN.
static inline rc_rational rc_rational_abs(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(a.num_ != INT64_MIN);
    return (rc_rational) {.num_ = a.num_ < 0 ? -a.num_ : a.num_, .denom_ = a.denom_};
}

// 1 / a, with the sign moved to the numerator. a must not be zero.
static inline rc_rational rc_rational_reciprocal(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(a.num_ != 0);
    if (a.num_ < 0) {
        return (rc_rational) {.num_ = -a.denom_, .denom_ = -a.num_};
    }

    return (rc_rational) {.num_ = a.denom_, .denom_ = a.num_};
}

// a * b.
rc_rational rc_rational_mul(rc_rational a, rc_rational b);

// a * b, for a whole number b.
rc_rational rc_rational_int_mul(rc_rational a, int64_t b);

// a / b. b must not be zero.
rc_rational rc_rational_div(rc_rational a, rc_rational b);

// a / b, for a whole number b, which must not be zero.
rc_rational rc_rational_int_div(rc_rational a, int64_t b);

// a + b.
rc_rational rc_rational_add(rc_rational a, rc_rational b);

// a + b, for a whole number b. That can't make a common factor, so it is cheap.
static inline rc_rational rc_rational_int_add(rc_rational a, int64_t b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    return (rc_rational) {.num_ = rc_rational_add_checked_(a.num_, rc_rational_mul_checked_(b, a.denom_)), .denom_ = a.denom_};
}

// a - b.
rc_rational rc_rational_sub(rc_rational a, rc_rational b);

// a - b, for a whole number b. That can't make a common factor, so it is cheap.
static inline rc_rational rc_rational_int_sub(rc_rational a, int64_t b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    return (rc_rational) {.num_ = rc_rational_sub_checked_(a.num_, rc_rational_mul_checked_(b, a.denom_)), .denom_ = a.denom_};
}

// ---- comparison ----

// -1, 0 or 1, as a is less than, equal to or greater than b. It never multiplies, so it works
// even where a.num * b.denom would overflow.
int32_t rc_rational_compare(rc_rational a, rc_rational b);

// True if a == b. Both being in lowest terms, their members match.
static inline bool rc_rational_is_equal(rc_rational a, rc_rational b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    return a.num_ == b.num_ && a.denom_ == b.denom_;
}

// True if a < b.
static inline bool rc_rational_is_less_than(rc_rational a, rc_rational b)
{
    return rc_rational_compare(a, b) < 0;
}

// True if a > b.
static inline bool rc_rational_is_greater_than(rc_rational a, rc_rational b)
{
    return rc_rational_compare(a, b) > 0;
}

// The smaller of a and b.
static inline rc_rational rc_rational_min(rc_rational a, rc_rational b)
{
    return rc_rational_is_less_than(a, b) ? a : b;
}

// The larger of a and b.
static inline rc_rational rc_rational_max(rc_rational a, rc_rational b)
{
    return rc_rational_is_less_than(a, b) ? b : a;
}

// ---- conversion ----

// a as the nearest double to num / denom, each rounded first, so big ones lose precision.
static inline double rc_rational_to_double(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    return (double)a.num_ / (double)a.denom_;
}

#endif // RC_MATH_RATIONAL_H_
