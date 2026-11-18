// math/vec3i.h
//
// A 3D vector of int32_t. The products are worked out in int64_t: dot and lengthsqr return one,
// and cross asserts each component of its result fits back into an int32_t. There is no length,
// since it is rarely a whole number.

#ifndef RC_MATH_VEC3I_H_
#define RC_MATH_VEC3I_H_

#include "richc/math/vec2i.h"

typedef struct rc_vec3i {
    int32_t x;
    int32_t y;
    int32_t z;
} rc_vec3i;

// ---- construction ----

// The vector (x, y, z).
static inline rc_vec3i rc_vec3i_make(int32_t x, int32_t y, int32_t z)
{
    return (rc_vec3i) {x, y, z};
}

// The vector (0, 0, 0).
static inline rc_vec3i rc_vec3i_make_zero(void)
{
    return (rc_vec3i) {0, 0, 0};
}

// The vector (1, 0, 0).
static inline rc_vec3i rc_vec3i_make_unitx(void)
{
    return (rc_vec3i) {1, 0, 0};
}

// The vector (0, 1, 0).
static inline rc_vec3i rc_vec3i_make_unity(void)
{
    return (rc_vec3i) {0, 1, 0};
}

// The vector (0, 0, 1).
static inline rc_vec3i rc_vec3i_make_unitz(void)
{
    return (rc_vec3i) {0, 0, 1};
}

// The vector from three consecutive int32_ts.
static inline rc_vec3i rc_vec3i_from_i32s(const int32_t *i)
{
    return (rc_vec3i) {i[0], i[1], i[2]};
}

// v with z added.
static inline rc_vec3i rc_vec3i_from_vec2i(rc_vec2i v, int32_t z)
{
    return (rc_vec3i) {v.x, v.y, z};
}

// ---- conversion ----

// The components as an int32_t[3].
static inline const int32_t *rc_vec3i_as_i32s(const rc_vec3i *v)
{
    return &v->x;
}

// ---- arithmetic ----

// a + b.
static inline rc_vec3i rc_vec3i_add(rc_vec3i a, rc_vec3i b)
{
    return (rc_vec3i) {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z
    };
}

// a + b + c.
static inline rc_vec3i rc_vec3i_add3(rc_vec3i a, rc_vec3i b, rc_vec3i c)
{
    return (rc_vec3i) {
        a.x + b.x + c.x,
        a.y + b.y + c.y,
        a.z + b.z + c.z
    };
}

// a + b + c + d.
static inline rc_vec3i rc_vec3i_add4(rc_vec3i a, rc_vec3i b, rc_vec3i c, rc_vec3i d)
{
    return (rc_vec3i) {
        a.x + b.x + c.x + d.x,
        a.y + b.y + c.y + d.y,
        a.z + b.z + c.z + d.z
    };
}

// a - b.
static inline rc_vec3i rc_vec3i_sub(rc_vec3i a, rc_vec3i b)
{
    return (rc_vec3i) {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}

// v scaled by s.
static inline rc_vec3i rc_vec3i_scalar_mul(rc_vec3i v, int32_t s)
{
    return (rc_vec3i) {
        v.x * s,
        v.y * s,
        v.z * s
    };
}

// v divided by s, rounding each component towards zero. s must not be 0.
static inline rc_vec3i rc_vec3i_scalar_div(rc_vec3i v, int32_t s)
{
    RC_ASSERT(s != 0);
    return (rc_vec3i) {
        v.x / s,
        v.y / s,
        v.z / s
    };
}

// a and b multiplied component by component.
static inline rc_vec3i rc_vec3i_component_mul(rc_vec3i a, rc_vec3i b)
{
    return (rc_vec3i) {
        a.x * b.x,
        a.y * b.y,
        a.z * b.z
    };
}

// The smaller of a and b in each component.
static inline rc_vec3i rc_vec3i_component_min(rc_vec3i a, rc_vec3i b)
{
    return (rc_vec3i) {
        rc_min_i32(a.x, b.x),
        rc_min_i32(a.y, b.y),
        rc_min_i32(a.z, b.z)
    };
}

// The larger of a and b in each component.
static inline rc_vec3i rc_vec3i_component_max(rc_vec3i a, rc_vec3i b)
{
    return (rc_vec3i) {
        rc_max_i32(a.x, b.x),
        rc_max_i32(a.y, b.y),
        rc_max_i32(a.z, b.z)
    };
}

// -v.
static inline rc_vec3i rc_vec3i_negate(rc_vec3i v)
{
    return (rc_vec3i) {
        -v.x,
        -v.y,
        -v.z
    };
}

// ---- products ----

// The dot product of a and b. It asserts that the sum fits, which it does unless the components
// are near INT32_MIN.
static inline int64_t rc_vec3i_dot(rc_vec3i a, rc_vec3i b)
{
    int64_t xx = (int64_t)a.x * b.x;
    int64_t yy = (int64_t)a.y * b.y;
    int64_t zz = (int64_t)a.z * b.z;

    RC_ASSERT(!rc_add_overflows_i64(xx, yy));
    RC_ASSERT(!rc_add_overflows_i64(xx + yy, zz));
    return xx + yy + zz;
}

// True if value fits in an int32_t.
static inline bool rc_vec3i_fits_i32_(int64_t value)
{
    return value >= INT32_MIN && value <= INT32_MAX;
}

// The cross product a x b. Each component is worked out in int64_t, where the products and their
// difference always fit, and must fit back into an int32_t.
static inline rc_vec3i rc_vec3i_cross(rc_vec3i a, rc_vec3i b)
{
    int64_t x = (int64_t)a.y * b.z - (int64_t)a.z * b.y;
    int64_t y = (int64_t)a.z * b.x - (int64_t)a.x * b.z;
    int64_t z = (int64_t)a.x * b.y - (int64_t)a.y * b.x;

    RC_ASSERT(rc_vec3i_fits_i32_(x));
    RC_ASSERT(rc_vec3i_fits_i32_(y));
    RC_ASSERT(rc_vec3i_fits_i32_(z));
    return (rc_vec3i) {
        (int32_t)x,
        (int32_t)y,
        (int32_t)z
    };
}

// The squared length of v.
static inline int64_t rc_vec3i_lengthsqr(rc_vec3i v)
{
    return rc_vec3i_dot(v, v);
}

// ---- comparison ----

// True if a and b are the same vector.
static inline bool rc_vec3i_is_equal(rc_vec3i a, rc_vec3i b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

#endif // RC_MATH_VEC3I_H_
