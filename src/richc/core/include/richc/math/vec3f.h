// math/vec3f.h
//
// A 3D vector of floats.

#ifndef RC_MATH_VEC3F_H_
#define RC_MATH_VEC3F_H_

#include "richc/math/vec2f.h"
#include "richc/math/vec3i.h"

typedef struct rc_vec3f {
    float x;
    float y;
    float z;
} rc_vec3f;

// ---- construction ----

// The vector (x, y, z).
static inline rc_vec3f rc_vec3f_make(float x, float y, float z)
{
    return (rc_vec3f) {x, y, z};
}

// The vector (0, 0, 0).
static inline rc_vec3f rc_vec3f_make_zero(void)
{
    return (rc_vec3f) {0.0f, 0.0f, 0.0f};
}

// The vector (1, 0, 0).
static inline rc_vec3f rc_vec3f_make_unitx(void)
{
    return (rc_vec3f) {1.0f, 0.0f, 0.0f};
}

// The vector (0, 1, 0).
static inline rc_vec3f rc_vec3f_make_unity(void)
{
    return (rc_vec3f) {0.0f, 1.0f, 0.0f};
}

// The vector (0, 0, 1).
static inline rc_vec3f rc_vec3f_make_unitz(void)
{
    return (rc_vec3f) {0.0f, 0.0f, 1.0f};
}

// The vector from three consecutive floats.
static inline rc_vec3f rc_vec3f_from_floats(const float *f)
{
    return (rc_vec3f) {f[0], f[1], f[2]};
}

// v with z added.
static inline rc_vec3f rc_vec3f_from_vec2f(rc_vec2f v, float z)
{
    return (rc_vec3f) {v.x, v.y, z};
}

// v converted to floats.
static inline rc_vec3f rc_vec3f_from_vec3i(rc_vec3i v)
{
    return (rc_vec3f) {(float)v.x, (float)v.y, (float)v.z};
}

// ---- conversion ----

// The components as a float[3].
static inline const float *rc_vec3f_as_floats(const rc_vec3f *v)
{
    return &v->x;
}

// ---- arithmetic ----

// a + b.
static inline rc_vec3f rc_vec3f_add(rc_vec3f a, rc_vec3f b)
{
    return (rc_vec3f) {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z
    };
}

// a + b + c.
static inline rc_vec3f rc_vec3f_add3(rc_vec3f a, rc_vec3f b, rc_vec3f c)
{
    return (rc_vec3f) {
        a.x + b.x + c.x,
        a.y + b.y + c.y,
        a.z + b.z + c.z
    };
}

// a + b + c + d.
static inline rc_vec3f rc_vec3f_add4(rc_vec3f a, rc_vec3f b, rc_vec3f c, rc_vec3f d)
{
    return (rc_vec3f) {
        a.x + b.x + c.x + d.x,
        a.y + b.y + c.y + d.y,
        a.z + b.z + c.z + d.z
    };
}

// a - b.
static inline rc_vec3f rc_vec3f_sub(rc_vec3f a, rc_vec3f b)
{
    return (rc_vec3f) {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}

// v scaled by s.
static inline rc_vec3f rc_vec3f_scalar_mul(rc_vec3f v, float s)
{
    return (rc_vec3f) {
        v.x * s,
        v.y * s,
        v.z * s
    };
}

// v divided by s.
static inline rc_vec3f rc_vec3f_scalar_div(rc_vec3f v, float s)
{
    return (rc_vec3f) {
        v.x / s,
        v.y / s,
        v.z / s
    };
}

// a and b multiplied component by component.
static inline rc_vec3f rc_vec3f_component_mul(rc_vec3f a, rc_vec3f b)
{
    return (rc_vec3f) {
        a.x * b.x,
        a.y * b.y,
        a.z * b.z
    };
}

// The smaller of a and b in each component.
static inline rc_vec3f rc_vec3f_component_min(rc_vec3f a, rc_vec3f b)
{
    return (rc_vec3f) {
        rc_min_f32(a.x, b.x),
        rc_min_f32(a.y, b.y),
        rc_min_f32(a.z, b.z)
    };
}

// The larger of a and b in each component.
static inline rc_vec3f rc_vec3f_component_max(rc_vec3f a, rc_vec3f b)
{
    return (rc_vec3f) {
        rc_max_f32(a.x, b.x),
        rc_max_f32(a.y, b.y),
        rc_max_f32(a.z, b.z)
    };
}

// Each component rounded down.
static inline rc_vec3f rc_vec3f_component_floor(rc_vec3f v)
{
    return (rc_vec3f) {
        rc_floor_f32(v.x),
        rc_floor_f32(v.y),
        rc_floor_f32(v.z)
    };
}

// Each component rounded up.
static inline rc_vec3f rc_vec3f_component_ceil(rc_vec3f v)
{
    return (rc_vec3f) {
        rc_ceil_f32(v.x),
        rc_ceil_f32(v.y),
        rc_ceil_f32(v.z)
    };
}

// Each component without its sign.
static inline rc_vec3f rc_vec3f_component_abs(rc_vec3f v)
{
    return (rc_vec3f) {
        rc_abs_f32(v.x),
        rc_abs_f32(v.y),
        rc_abs_f32(v.z)
    };
}

// The point t of the way from a to b: a + (b - a) * t.
static inline rc_vec3f rc_vec3f_lerp(rc_vec3f a, rc_vec3f b, float t)
{
    return (rc_vec3f) {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

// -v.
static inline rc_vec3f rc_vec3f_negate(rc_vec3f v)
{
    return (rc_vec3f) {
        -v.x,
        -v.y,
        -v.z
    };
}

// ---- products and lengths ----

// The dot product of a and b.
static inline float rc_vec3f_dot(rc_vec3f a, rc_vec3f b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// The cross product a x b.
static inline rc_vec3f rc_vec3f_cross(rc_vec3f a, rc_vec3f b)
{
    return (rc_vec3f) {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// The squared length of v.
static inline float rc_vec3f_lengthsqr(rc_vec3f v)
{
    return rc_vec3f_dot(v, v);
}

// The length of v.
static inline float rc_vec3f_length(rc_vec3f v)
{
    return rc_sqrt_f32(rc_vec3f_lengthsqr(v));
}

// v scaled to length 1. v must not be zero.
static inline rc_vec3f rc_vec3f_normalize(rc_vec3f v)
{
    float length = rc_vec3f_length(v);

    RC_ASSERT(length != 0.0f);
    return rc_vec3f_scalar_div(v, length);
}

// v scaled to length 1, or zero if it is shorter than tolerance.
static inline rc_vec3f rc_vec3f_normalize_safe(rc_vec3f v, float tolerance)
{
    float length = rc_vec3f_length(v);

    return length >= tolerance ? rc_vec3f_scalar_div(v, length) : rc_vec3f_make_zero();
}

// ---- comparison ----

// True if a and b are the same vector.
static inline bool rc_vec3f_is_equal(rc_vec3f a, rc_vec3f b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

// True if a and b are less than tolerance apart.
static inline bool rc_vec3f_is_nearly_equal(rc_vec3f a, rc_vec3f b, float tolerance)
{
    return rc_vec3f_lengthsqr(rc_vec3f_sub(a, b)) < tolerance * tolerance;
}

#endif // RC_MATH_VEC3F_H_
