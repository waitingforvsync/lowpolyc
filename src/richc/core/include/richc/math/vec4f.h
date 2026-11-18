// math/vec4f.h
//
// A 4D vector of floats: a homogeneous point, a colour, or a column of a 4x4 matrix.

#ifndef RC_MATH_VEC4F_H_
#define RC_MATH_VEC4F_H_

#include "richc/math/vec3f.h"

typedef struct rc_vec4f {
    float x;
    float y;
    float z;
    float w;
} rc_vec4f;

// ---- construction ----

// The vector (x, y, z, w).
static inline rc_vec4f rc_vec4f_make(float x, float y, float z, float w)
{
    return (rc_vec4f) {x, y, z, w};
}

// The vector (0, 0, 0, 0).
static inline rc_vec4f rc_vec4f_make_zero(void)
{
    return (rc_vec4f) {0.0f, 0.0f, 0.0f, 0.0f};
}

// The vector (1, 0, 0, 0).
static inline rc_vec4f rc_vec4f_make_unitx(void)
{
    return (rc_vec4f) {1.0f, 0.0f, 0.0f, 0.0f};
}

// The vector (0, 1, 0, 0).
static inline rc_vec4f rc_vec4f_make_unity(void)
{
    return (rc_vec4f) {0.0f, 1.0f, 0.0f, 0.0f};
}

// The vector (0, 0, 1, 0).
static inline rc_vec4f rc_vec4f_make_unitz(void)
{
    return (rc_vec4f) {0.0f, 0.0f, 1.0f, 0.0f};
}

// The vector (0, 0, 0, 1).
static inline rc_vec4f rc_vec4f_make_unitw(void)
{
    return (rc_vec4f) {0.0f, 0.0f, 0.0f, 1.0f};
}

// The vector from four consecutive floats.
static inline rc_vec4f rc_vec4f_from_floats(const float *f)
{
    return (rc_vec4f) {f[0], f[1], f[2], f[3]};
}

// v with z and w added.
static inline rc_vec4f rc_vec4f_from_vec2f(rc_vec2f v, float z, float w)
{
    return (rc_vec4f) {v.x, v.y, z, w};
}

// v with w added.
static inline rc_vec4f rc_vec4f_from_vec3f(rc_vec3f v, float w)
{
    return (rc_vec4f) {v.x, v.y, v.z, w};
}

// ---- conversion ----

// The components as a float[4].
static inline const float *rc_vec4f_as_floats(const rc_vec4f *v)
{
    return &v->x;
}

// ---- arithmetic ----

// a + b.
static inline rc_vec4f rc_vec4f_add(rc_vec4f a, rc_vec4f b)
{
    return (rc_vec4f) {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z,
        a.w + b.w
    };
}

// a + b + c.
static inline rc_vec4f rc_vec4f_add3(rc_vec4f a, rc_vec4f b, rc_vec4f c)
{
    return (rc_vec4f) {
        a.x + b.x + c.x,
        a.y + b.y + c.y,
        a.z + b.z + c.z,
        a.w + b.w + c.w
    };
}

// a + b + c + d.
static inline rc_vec4f rc_vec4f_add4(rc_vec4f a, rc_vec4f b, rc_vec4f c, rc_vec4f d)
{
    return (rc_vec4f) {
        a.x + b.x + c.x + d.x,
        a.y + b.y + c.y + d.y,
        a.z + b.z + c.z + d.z,
        a.w + b.w + c.w + d.w
    };
}

// a - b.
static inline rc_vec4f rc_vec4f_sub(rc_vec4f a, rc_vec4f b)
{
    return (rc_vec4f) {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z,
        a.w - b.w
    };
}

// v scaled by s.
static inline rc_vec4f rc_vec4f_scalar_mul(rc_vec4f v, float s)
{
    return (rc_vec4f) {
        v.x * s,
        v.y * s,
        v.z * s,
        v.w * s
    };
}

// v divided by s.
static inline rc_vec4f rc_vec4f_scalar_div(rc_vec4f v, float s)
{
    return (rc_vec4f) {
        v.x / s,
        v.y / s,
        v.z / s,
        v.w / s
    };
}

// a and b multiplied component by component.
static inline rc_vec4f rc_vec4f_component_mul(rc_vec4f a, rc_vec4f b)
{
    return (rc_vec4f) {
        a.x * b.x,
        a.y * b.y,
        a.z * b.z,
        a.w * b.w
    };
}

// The smaller of a and b in each component.
static inline rc_vec4f rc_vec4f_component_min(rc_vec4f a, rc_vec4f b)
{
    return (rc_vec4f) {
        rc_min_f32(a.x, b.x),
        rc_min_f32(a.y, b.y),
        rc_min_f32(a.z, b.z),
        rc_min_f32(a.w, b.w)
    };
}

// The larger of a and b in each component.
static inline rc_vec4f rc_vec4f_component_max(rc_vec4f a, rc_vec4f b)
{
    return (rc_vec4f) {
        rc_max_f32(a.x, b.x),
        rc_max_f32(a.y, b.y),
        rc_max_f32(a.z, b.z),
        rc_max_f32(a.w, b.w)
    };
}

// Each component rounded down.
static inline rc_vec4f rc_vec4f_component_floor(rc_vec4f v)
{
    return (rc_vec4f) {
        rc_floor_f32(v.x),
        rc_floor_f32(v.y),
        rc_floor_f32(v.z),
        rc_floor_f32(v.w)
    };
}

// Each component rounded up.
static inline rc_vec4f rc_vec4f_component_ceil(rc_vec4f v)
{
    return (rc_vec4f) {
        rc_ceil_f32(v.x),
        rc_ceil_f32(v.y),
        rc_ceil_f32(v.z),
        rc_ceil_f32(v.w)
    };
}

// Each component without its sign.
static inline rc_vec4f rc_vec4f_component_abs(rc_vec4f v)
{
    return (rc_vec4f) {
        rc_abs_f32(v.x),
        rc_abs_f32(v.y),
        rc_abs_f32(v.z),
        rc_abs_f32(v.w)
    };
}

// The point t of the way from a to b: a + (b - a) * t.
static inline rc_vec4f rc_vec4f_lerp(rc_vec4f a, rc_vec4f b, float t)
{
    return (rc_vec4f) {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    };
}

// -v.
static inline rc_vec4f rc_vec4f_negate(rc_vec4f v)
{
    return (rc_vec4f) {
        -v.x,
        -v.y,
        -v.z,
        -v.w
    };
}

// ---- products and lengths ----

// The dot product of a and b.
static inline float rc_vec4f_dot(rc_vec4f a, rc_vec4f b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

// The squared length of v.
static inline float rc_vec4f_lengthsqr(rc_vec4f v)
{
    return rc_vec4f_dot(v, v);
}

// The length of v.
static inline float rc_vec4f_length(rc_vec4f v)
{
    return rc_sqrt_f32(rc_vec4f_lengthsqr(v));
}

// v scaled to length 1. v must not be zero.
static inline rc_vec4f rc_vec4f_normalize(rc_vec4f v)
{
    float length = rc_vec4f_length(v);

    RC_ASSERT(length != 0.0f);
    return rc_vec4f_scalar_div(v, length);
}

// v scaled to length 1, or zero if it is shorter than tolerance.
static inline rc_vec4f rc_vec4f_normalize_safe(rc_vec4f v, float tolerance)
{
    float length = rc_vec4f_length(v);

    return length >= tolerance ? rc_vec4f_scalar_div(v, length) : rc_vec4f_make_zero();
}

// ---- comparison ----

// True if a and b are the same vector.
static inline bool rc_vec4f_is_equal(rc_vec4f a, rc_vec4f b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

// True if a and b are less than tolerance apart.
static inline bool rc_vec4f_is_nearly_equal(rc_vec4f a, rc_vec4f b, float tolerance)
{
    return rc_vec4f_lengthsqr(rc_vec4f_sub(a, b)) < tolerance * tolerance;
}

#endif // RC_MATH_VEC4F_H_
