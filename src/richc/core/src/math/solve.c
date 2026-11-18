// math/solve.c
//
// Polynomial roots in closed form. The cubic is shifted to the depressed form u^3 + p u + q = 0,
// whose discriminant q^2 / 4 + p^3 / 27 says how many real roots it has: one, by Cardano's formula,
// if it is positive, or otherwise three, by the trigonometric method.

#include "richc/math/solve.h"

#include "richc/math/math.h"

// A leading coefficient smaller than this counts as zero.
static const float rc_solve_epsilon = 1e-6f;

// The root of b t + c = 0, if b isn't near zero.
static rc_quadratic_roots rc_solve_linear(float b, float c)
{
    if (rc_abs_f32(b) < rc_solve_epsilon) {
        return (rc_quadratic_roots) {0};
    }

    return (rc_quadratic_roots) {.num_roots = 1, .root = {-c / b}};
}


// The two roots of a t^2 + b t + c = 0, given a positive discriminant. Its square root is added to
// b's magnitude, never taken from it, so two nearly equal numbers are never subtracted: that gives
// q = -(b + sign(b) sqrt(discriminant)) / 2, and the well-conditioned root q / a. The other comes
// from the product of the roots, c / a, as c / q.
static rc_quadratic_roots rc_solve_two_roots(float a, float b, float c, float discriminant)
{
    float root = rc_sqrt_f32(discriminant);
    float q = (b >= 0.0f ? -b - root : -b + root) * 0.5f;

    return (rc_quadratic_roots) {.num_roots = 2, .root = {q / a, c / q}};
}


rc_quadratic_roots rc_solve_quadratic(float a, float b, float c)
{
    float discriminant = b * b - 4.0f * a * c;

    if (rc_abs_f32(a) < rc_solve_epsilon) {
        return rc_solve_linear(b, c);
    }

    if (discriminant < 0.0f) {
        return (rc_quadratic_roots) {0};
    }

    if (discriminant == 0.0f) {
        return (rc_quadratic_roots) {.num_roots = 1, .root = {-b / (2.0f * a)}};
    }

    return rc_solve_two_roots(a, b, c, discriminant);
}


// The roots of the quadratic b t^2 + c t + d, as a cubic's.
static rc_cubic_roots rc_solve_cubic_as_quadratic(float b, float c, float d)
{
    rc_quadratic_roots roots = rc_solve_quadratic(b, c, d);

    return (rc_cubic_roots) {.num_roots = roots.num_roots, .root = {roots.root[0], roots.root[1]}};
}


// The one real root of u^3 + p u + q = 0 for a positive discriminant, by Cardano's formula. The
// cube root takes negative arguments, so no case is needed for their signs.
static float rc_solve_cardano(float q, float discriminant)
{
    float root = rc_sqrt_f32(discriminant);

    return rc_cbrt_f32(-q * 0.5f + root) + rc_cbrt_f32(-q * 0.5f - root);
}


// x, kept within [-1, 1] for acos. A NaN stays NaN.
static float rc_solve_clamp_unit(float x)
{
    if (x < -1.0f) {
        return -1.0f;
    }

    if (x > 1.0f) {
        return 1.0f;
    }

    return x;
}


// The three real roots of u^3 + p u + q = 0, shifted back by shift, by the trigonometric method.
// The discriminant not being positive makes p negative, so -p / 3 has a square root.
static rc_cubic_roots rc_solve_cubic_trigonometric(float p, float q, float shift)
{
    float third_turn = 2.0943951f;
    float r = rc_sqrt_f32(-p / 3.0f);
    float cos_3phi = rc_solve_clamp_unit(-q / (2.0f * r * r * r));
    float phi = rc_acos_f32(cos_3phi) / 3.0f;
    float twice_r = 2.0f * r;

    return (rc_cubic_roots) {
        .num_roots = 3,
        .root = {
            twice_r * rc_cos_f32(phi) - shift,
            twice_r * rc_cos_f32(phi + third_turn) - shift,
            twice_r * rc_cos_f32(phi + 2.0f * third_turn) - shift
        }
    };
}


// The roots of t^3 + b t^2 + c t + d = 0, through u = t + b / 3, which leaves the depressed cubic.
// When p is near zero there is a nearly triple root at the cube root of -q.
static rc_cubic_roots rc_solve_monic_cubic(float b, float c, float d)
{
    float shift = b / 3.0f;
    float p = c - b * shift;
    float q = d + shift * (2.0f * shift * shift - c);
    float discriminant = q * q * 0.25f + p * p * p / 27.0f;

    if (discriminant > 0.0f) {
        return (rc_cubic_roots) {.num_roots = 1, .root = {rc_solve_cardano(q, discriminant) - shift}};
    }

    if (p > -rc_solve_epsilon) {
        return (rc_cubic_roots) {.num_roots = 1, .root = {rc_cbrt_f32(-q) - shift}};
    }

    return rc_solve_cubic_trigonometric(p, q, shift);
}


rc_cubic_roots rc_solve_cubic(float a, float b, float c, float d)
{
    if (rc_abs_f32(a) < rc_solve_epsilon) {
        return rc_solve_cubic_as_quadratic(b, c, d);
    }

    return rc_solve_monic_cubic(b / a, c / a, d / a);
}
