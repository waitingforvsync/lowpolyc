// test_solve.c
//
// Tests for the closed-form polynomial roots in math/solve.h.

#include "richc/math/solve.h"

#include "richc/test.h"


// True if value is within 1e-4 of one of the roots, in whichever order they came.
static bool has_root(rc_quadratic_roots roots, float value)
{
    for (uint32_t i = 0; i < roots.num_roots; i++) {
        float difference = roots.root[i] - value;

        if (difference > -1e-4f && difference < 1e-4f) {
            return true;
        }
    }

    return false;
}


// True if value is within 1e-4 of one of the cubic's roots.
static bool has_cubic_root(rc_cubic_roots roots, float value)
{
    for (uint32_t i = 0; i < roots.num_roots; i++) {
        float difference = roots.root[i] - value;

        if (difference > -1e-4f && difference < 1e-4f) {
            return true;
        }
    }

    return false;
}


// a t^3 + b t^2 + c t + d, by Horner's rule.
static float cubic_at(float a, float b, float c, float d, float t)
{
    return ((a * t + b) * t + c) * t + d;
}


// a t^2 + b t + c, by Horner's rule.
static float quadratic_at(float a, float b, float c, float t)
{
    return (a * t + b) * t + c;
}


RC_TEST(solve, two_roots)
{
    rc_quadratic_roots roots = rc_solve_quadratic(1.0f, -5.0f, 6.0f);

    // (t - 2)(t - 3)
    RC_CHECK(roots.num_roots, ==, 2u);
    RC_CHECK_TRUE(has_root(roots, 2.0f));
    RC_CHECK_TRUE(has_root(roots, 3.0f));
    RC_CHECK(quadratic_at(1.0f, -5.0f, 6.0f, roots.root[0]), ~=, 0.0f);
    RC_CHECK(quadratic_at(1.0f, -5.0f, 6.0f, roots.root[1]), ~=, 0.0f);
}


RC_TEST(solve, two_roots_whatever_the_signs)
{
    rc_quadratic_roots positive_b = rc_solve_quadratic(1.0f, 5.0f, 6.0f);
    rc_quadratic_roots negative_a = rc_solve_quadratic(-1.0f, 5.0f, -6.0f);
    rc_quadratic_roots no_b = rc_solve_quadratic(1.0f, 0.0f, -4.0f);
    rc_quadratic_roots no_c = rc_solve_quadratic(1.0f, -3.0f, 0.0f);

    RC_CHECK(positive_b.num_roots, ==, 2u);
    RC_CHECK_TRUE(has_root(positive_b, -2.0f));
    RC_CHECK_TRUE(has_root(positive_b, -3.0f));
    RC_CHECK(negative_a.num_roots, ==, 2u);
    RC_CHECK_TRUE(has_root(negative_a, 2.0f));
    RC_CHECK_TRUE(has_root(negative_a, 3.0f));
    RC_CHECK(no_b.num_roots, ==, 2u);
    RC_CHECK_TRUE(has_root(no_b, 2.0f));
    RC_CHECK_TRUE(has_root(no_b, -2.0f));
    RC_CHECK(no_c.num_roots, ==, 2u);
    RC_CHECK_TRUE(has_root(no_c, 0.0f));
    RC_CHECK_TRUE(has_root(no_c, 3.0f));
}


RC_TEST(solve, double_root)
{
    rc_quadratic_roots roots = rc_solve_quadratic(1.0f, -8.0f, 16.0f);
    rc_quadratic_roots at_zero = rc_solve_quadratic(2.0f, 0.0f, 0.0f);

    // (t - 4)^2, then 2 t^2
    RC_CHECK(roots.num_roots, ==, 1u);
    RC_CHECK(roots.root[0], ==, 4.0f);
    RC_CHECK(at_zero.num_roots, ==, 1u);
    RC_CHECK(at_zero.root[0], ==, 0.0f);
}


RC_TEST(solve, no_roots)
{
    RC_CHECK(rc_solve_quadratic(1.0f, 0.0f, 1.0f).num_roots, ==, 0u);
    RC_CHECK(rc_solve_quadratic(-1.0f, 0.0f, -1.0f).num_roots, ==, 0u);
    RC_CHECK(rc_solve_quadratic(1.0f, 2.0f, 1.0001f).num_roots, ==, 0u);
}


RC_TEST(solve, linear_when_a_is_zero)
{
    rc_quadratic_roots linear = rc_solve_quadratic(0.0f, 2.0f, -6.0f);
    rc_quadratic_roots nearly_linear = rc_solve_quadratic(5e-7f, 2.0f, -6.0f);
    rc_quadratic_roots negative_tiny_a = rc_solve_quadratic(-5e-7f, 2.0f, -6.0f);

    // 2t - 6, and a below the epsilon counts as zero either side of it
    RC_CHECK(linear.num_roots, ==, 1u);
    RC_CHECK(linear.root[0], ==, 3.0f);
    RC_CHECK(nearly_linear.num_roots, ==, 1u);
    RC_CHECK(nearly_linear.root[0], ==, 3.0f);
    RC_CHECK(negative_tiny_a.num_roots, ==, 1u);
    RC_CHECK(negative_tiny_a.root[0], ==, 3.0f);

    // Just above the epsilon, it is a quadratic again
    RC_CHECK(rc_solve_quadratic(2e-6f, 2.0f, -6.0f).num_roots, ==, 2u);
}


RC_TEST(solve, constant_has_no_roots)
{
    RC_CHECK(rc_solve_quadratic(0.0f, 0.0f, 5.0f).num_roots, ==, 0u);
    RC_CHECK(rc_solve_quadratic(0.0f, 5e-7f, 5.0f).num_roots, ==, 0u);
    RC_CHECK(rc_solve_quadratic(0.0f, 0.0f, 0.0f).num_roots, ==, 0u);
}


RC_TEST(solve, stable_for_far_apart_roots)
{
    rc_quadratic_roots roots = rc_solve_quadratic(1.0f, 1.0e8f, 1.0f);

    // Roots near -1e8 and -1e-8. The textbook formula takes two nearly equal numbers apart and
    // loses the small root to zero; this keeps it, so the product of the roots stays at c / a = 1.
    RC_CHECK(roots.num_roots, ==, 2u);
    RC_CHECK(roots.root[0] * roots.root[1], ~=, 1.0f);
}


RC_TEST(solve, cubic_three_roots)
{
    rc_cubic_roots roots = rc_solve_cubic(1.0f, -6.0f, 11.0f, -6.0f);

    // (t - 1)(t - 2)(t - 3)
    RC_CHECK(roots.num_roots, ==, 3u);
    RC_CHECK_TRUE(has_cubic_root(roots, 1.0f));
    RC_CHECK_TRUE(has_cubic_root(roots, 2.0f));
    RC_CHECK_TRUE(has_cubic_root(roots, 3.0f));
    RC_CHECK(cubic_at(1.0f, -6.0f, 11.0f, -6.0f, roots.root[0]), ~=, 0.0f);
    RC_CHECK(cubic_at(1.0f, -6.0f, 11.0f, -6.0f, roots.root[1]), ~=, 0.0f);
    RC_CHECK(cubic_at(1.0f, -6.0f, 11.0f, -6.0f, roots.root[2]), ~=, 0.0f);
}


RC_TEST(solve, cubic_one_root)
{
    rc_cubic_roots roots = rc_solve_cubic(1.0f, -2.0f, 1.0f, -2.0f);
    rc_cubic_roots scaled = rc_solve_cubic(-2.0f, 4.0f, -2.0f, 4.0f);

    // (t - 2)(t^2 + 1), whose only real root is 2, and the same scaled by -2
    RC_CHECK(roots.num_roots, ==, 1u);
    RC_CHECK(roots.root[0], ~=, 2.0f);
    RC_CHECK(scaled.num_roots, ==, 1u);
    RC_CHECK(scaled.root[0], ~=, 2.0f);
}


RC_TEST(solve, cubic_triple_root)
{
    rc_cubic_roots roots = rc_solve_cubic(1.0f, -3.0f, 3.0f, -1.0f);
    rc_cubic_roots at_zero = rc_solve_cubic(1.0f, 0.0f, 0.0f, 0.0f);

    // (t - 1)^3, and t^3
    RC_CHECK(roots.num_roots, ==, 1u);
    RC_CHECK(roots.root[0], ~=, 1.0f);
    RC_CHECK(at_zero.num_roots, ==, 1u);
    RC_CHECK(at_zero.root[0], ==, 0.0f);
}


RC_TEST(solve, cubic_repeated_root)
{
    rc_cubic_roots roots = rc_solve_cubic(1.0f, -6.0f, 9.0f, -4.0f);

    // (t - 1)^2 (t - 4): three real roots, two of them the same
    RC_CHECK(roots.num_roots, ==, 3u);
    RC_CHECK_TRUE(has_cubic_root(roots, 1.0f));
    RC_CHECK_TRUE(has_cubic_root(roots, 4.0f));
    RC_CHECK(cubic_at(1.0f, -6.0f, 9.0f, -4.0f, roots.root[0]), ~=, 0.0f);
    RC_CHECK(cubic_at(1.0f, -6.0f, 9.0f, -4.0f, roots.root[1]), ~=, 0.0f);
    RC_CHECK(cubic_at(1.0f, -6.0f, 9.0f, -4.0f, roots.root[2]), ~=, 0.0f);
}


RC_TEST(solve, cubic_as_quadratic)
{
    rc_cubic_roots roots = rc_solve_cubic(0.0f, 1.0f, -5.0f, 6.0f);
    rc_cubic_roots linear = rc_solve_cubic(0.0f, 0.0f, 2.0f, -6.0f);

    // With no t^3 term it is the quadratic (t - 2)(t - 3), or the line 2 t - 6
    RC_CHECK(roots.num_roots, ==, 2u);
    RC_CHECK_TRUE(has_cubic_root(roots, 2.0f));
    RC_CHECK_TRUE(has_cubic_root(roots, 3.0f));
    RC_CHECK(linear.num_roots, ==, 1u);
    RC_CHECK(linear.root[0], ==, 3.0f);
}
