// math/vec2f.h
//
// A 2D vector of floats.

#ifndef RC_MATH_VEC2F_H_
#define RC_MATH_VEC2F_H_

#include "richc/math/vec2i.h"

typedef struct rc_vec2f {
    float x;
    float y;
} rc_vec2f;

// ---- construction ----

// The vector (x, y).
static inline rc_vec2f rc_vec2f_make(float x, float y)
{
    return (rc_vec2f) {x, y};
}

// The vector (0, 0).
static inline rc_vec2f rc_vec2f_make_zero(void)
{
    return (rc_vec2f) {0.0f, 0.0f};
}

// The vector (1, 0).
static inline rc_vec2f rc_vec2f_make_unitx(void)
{
    return (rc_vec2f) {1.0f, 0.0f};
}

// The vector (0, 1).
static inline rc_vec2f rc_vec2f_make_unity(void)
{
    return (rc_vec2f) {0.0f, 1.0f};
}

// The vector (sin(angle), cos(angle)): the imaginary and real parts of the unit complex number that
// turns by angle.
static inline rc_vec2f rc_vec2f_make_sincos(float angle)
{
    return (rc_vec2f) {rc_sin_f32(angle), rc_cos_f32(angle)};
}

// The vector (cos(angle), sin(angle)): the x axis turned anticlockwise by angle.
static inline rc_vec2f rc_vec2f_make_cossin(float angle)
{
    return (rc_vec2f) {rc_cos_f32(angle), rc_sin_f32(angle)};
}

// The vector from two consecutive floats.
static inline rc_vec2f rc_vec2f_from_floats(const float *f)
{
    return (rc_vec2f) {f[0], f[1]};
}

// v converted to floats.
static inline rc_vec2f rc_vec2f_from_vec2i(rc_vec2i v)
{
    return (rc_vec2f) {(float)v.x, (float)v.y};
}

// ---- conversion ----

// The components as a float[2].
static inline const float *rc_vec2f_as_floats(const rc_vec2f *v)
{
    return &v->x;
}

// ---- arithmetic ----

// a + b.
static inline rc_vec2f rc_vec2f_add(rc_vec2f a, rc_vec2f b)
{
    return (rc_vec2f) {
        a.x + b.x,
        a.y + b.y
    };
}

// a + b + c.
static inline rc_vec2f rc_vec2f_add3(rc_vec2f a, rc_vec2f b, rc_vec2f c)
{
    return (rc_vec2f) {
        a.x + b.x + c.x,
        a.y + b.y + c.y
    };
}

// a + b + c + d.
static inline rc_vec2f rc_vec2f_add4(rc_vec2f a, rc_vec2f b, rc_vec2f c, rc_vec2f d)
{
    return (rc_vec2f) {
        a.x + b.x + c.x + d.x,
        a.y + b.y + c.y + d.y
    };
}

// a - b.
static inline rc_vec2f rc_vec2f_sub(rc_vec2f a, rc_vec2f b)
{
    return (rc_vec2f) {
        a.x - b.x,
        a.y - b.y
    };
}

// v scaled by s.
static inline rc_vec2f rc_vec2f_scalar_mul(rc_vec2f v, float s)
{
    return (rc_vec2f) {
        v.x * s,
        v.y * s
    };
}

// v divided by s.
static inline rc_vec2f rc_vec2f_scalar_div(rc_vec2f v, float s)
{
    return (rc_vec2f) {
        v.x / s,
        v.y / s
    };
}

// a and b multiplied component by component.
static inline rc_vec2f rc_vec2f_component_mul(rc_vec2f a, rc_vec2f b)
{
    return (rc_vec2f) {
        a.x * b.x,
        a.y * b.y
    };
}

// The smaller of a and b in each component.
static inline rc_vec2f rc_vec2f_component_min(rc_vec2f a, rc_vec2f b)
{
    return (rc_vec2f) {
        rc_min_f32(a.x, b.x),
        rc_min_f32(a.y, b.y)
    };
}

// The larger of a and b in each component.
static inline rc_vec2f rc_vec2f_component_max(rc_vec2f a, rc_vec2f b)
{
    return (rc_vec2f) {
        rc_max_f32(a.x, b.x),
        rc_max_f32(a.y, b.y)
    };
}

// Each component rounded down.
static inline rc_vec2f rc_vec2f_component_floor(rc_vec2f v)
{
    return (rc_vec2f) {
        rc_floor_f32(v.x),
        rc_floor_f32(v.y)
    };
}

// Each component rounded up.
static inline rc_vec2f rc_vec2f_component_ceil(rc_vec2f v)
{
    return (rc_vec2f) {
        rc_ceil_f32(v.x),
        rc_ceil_f32(v.y)
    };
}

// Each component without its sign.
static inline rc_vec2f rc_vec2f_component_abs(rc_vec2f v)
{
    return (rc_vec2f) {
        rc_abs_f32(v.x),
        rc_abs_f32(v.y)
    };
}

// The point t of the way from a to b: a + (b - a) * t.
static inline rc_vec2f rc_vec2f_lerp(rc_vec2f a, rc_vec2f b, float t)
{
    return (rc_vec2f) {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t
    };
}

// -v.
static inline rc_vec2f rc_vec2f_negate(rc_vec2f v)
{
    return (rc_vec2f) {
        -v.x,
        -v.y
    };
}

// v turned a quarter anticlockwise: (-y, x).
static inline rc_vec2f rc_vec2f_perp(rc_vec2f v)
{
    return (rc_vec2f) {
        -v.y,
        v.x
    };
}

// ---- products and lengths ----

// The dot product of a and b.
static inline float rc_vec2f_dot(rc_vec2f a, rc_vec2f b)
{
    return a.x * b.x + a.y * b.y;
}

// The 2D cross product a.x * b.y - a.y * b.x: positive when b is anticlockwise of a.
static inline float rc_vec2f_wedge(rc_vec2f a, rc_vec2f b)
{
    return a.x * b.y - a.y * b.x;
}

// The squared length of v.
static inline float rc_vec2f_lengthsqr(rc_vec2f v)
{
    return rc_vec2f_dot(v, v);
}

// The length of v.
static inline float rc_vec2f_length(rc_vec2f v)
{
    return rc_sqrt_f32(rc_vec2f_lengthsqr(v));
}

// v scaled to length 1. v must not be zero.
static inline rc_vec2f rc_vec2f_normalize(rc_vec2f v)
{
    float length = rc_vec2f_length(v);

    RC_ASSERT(length != 0.0f);
    return rc_vec2f_scalar_div(v, length);
}

// v scaled to length 1, or zero if it is shorter than tolerance.
static inline rc_vec2f rc_vec2f_normalize_safe(rc_vec2f v, float tolerance)
{
    float length = rc_vec2f_length(v);

    return length >= tolerance ? rc_vec2f_scalar_div(v, length) : rc_vec2f_make_zero();
}

// ---- comparison ----

// True if a and b are the same vector.
static inline bool rc_vec2f_is_equal(rc_vec2f a, rc_vec2f b)
{
    return a.x == b.x && a.y == b.y;
}

// True if a and b are less than tolerance apart.
static inline bool rc_vec2f_is_nearly_equal(rc_vec2f a, rc_vec2f b, float tolerance)
{
    return rc_vec2f_lengthsqr(rc_vec2f_sub(a, b)) < tolerance * tolerance;
}

#endif // RC_MATH_VEC2F_H_
