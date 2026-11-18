// math/vec2i.h
//
// A 2D vector of int32_t, for pixels, cells and window sizes. The products (dot, wedge, lengthsqr)
// are int64_t, which only components at the very ends of the range can overflow. There is no
// length, since it is rarely a whole number.

#ifndef RC_MATH_VEC2I_H_
#define RC_MATH_VEC2I_H_

#include "richc/math/math.h"

typedef struct rc_vec2i {
    int32_t x;
    int32_t y;
} rc_vec2i;

// ---- construction ----

// The vector (x, y).
static inline rc_vec2i rc_vec2i_make(int32_t x, int32_t y)
{
    return (rc_vec2i) {x, y};
}

// The vector (0, 0).
static inline rc_vec2i rc_vec2i_make_zero(void)
{
    return (rc_vec2i) {0, 0};
}

// The vector (1, 0).
static inline rc_vec2i rc_vec2i_make_unitx(void)
{
    return (rc_vec2i) {1, 0};
}

// The vector (0, 1).
static inline rc_vec2i rc_vec2i_make_unity(void)
{
    return (rc_vec2i) {0, 1};
}

// The vector from two consecutive int32_ts.
static inline rc_vec2i rc_vec2i_from_i32s(const int32_t *i)
{
    return (rc_vec2i) {i[0], i[1]};
}

// ---- conversion ----

// The components as an int32_t[2].
static inline const int32_t *rc_vec2i_as_i32s(const rc_vec2i *v)
{
    return &v->x;
}

// ---- arithmetic ----

// a + b.
static inline rc_vec2i rc_vec2i_add(rc_vec2i a, rc_vec2i b)
{
    return (rc_vec2i) {
        a.x + b.x,
        a.y + b.y
    };
}

// a + b + c.
static inline rc_vec2i rc_vec2i_add3(rc_vec2i a, rc_vec2i b, rc_vec2i c)
{
    return (rc_vec2i) {
        a.x + b.x + c.x,
        a.y + b.y + c.y
    };
}

// a + b + c + d.
static inline rc_vec2i rc_vec2i_add4(rc_vec2i a, rc_vec2i b, rc_vec2i c, rc_vec2i d)
{
    return (rc_vec2i) {
        a.x + b.x + c.x + d.x,
        a.y + b.y + c.y + d.y
    };
}

// a - b.
static inline rc_vec2i rc_vec2i_sub(rc_vec2i a, rc_vec2i b)
{
    return (rc_vec2i) {
        a.x - b.x,
        a.y - b.y
    };
}

// v scaled by s.
static inline rc_vec2i rc_vec2i_scalar_mul(rc_vec2i v, int32_t s)
{
    return (rc_vec2i) {
        v.x * s,
        v.y * s
    };
}

// v divided by s, rounding each component towards zero. s must not be 0.
static inline rc_vec2i rc_vec2i_scalar_div(rc_vec2i v, int32_t s)
{
    RC_ASSERT(s != 0);
    return (rc_vec2i) {
        v.x / s,
        v.y / s
    };
}

// a and b multiplied component by component.
static inline rc_vec2i rc_vec2i_component_mul(rc_vec2i a, rc_vec2i b)
{
    return (rc_vec2i) {
        a.x * b.x,
        a.y * b.y
    };
}

// The smaller of a and b in each component.
static inline rc_vec2i rc_vec2i_component_min(rc_vec2i a, rc_vec2i b)
{
    return (rc_vec2i) {
        rc_min_i32(a.x, b.x),
        rc_min_i32(a.y, b.y)
    };
}

// The larger of a and b in each component.
static inline rc_vec2i rc_vec2i_component_max(rc_vec2i a, rc_vec2i b)
{
    return (rc_vec2i) {
        rc_max_i32(a.x, b.x),
        rc_max_i32(a.y, b.y)
    };
}

// -v.
static inline rc_vec2i rc_vec2i_negate(rc_vec2i v)
{
    return (rc_vec2i) {
        -v.x,
        -v.y
    };
}

// v turned a quarter anticlockwise: (-y, x).
static inline rc_vec2i rc_vec2i_perp(rc_vec2i v)
{
    return (rc_vec2i) {
        -v.y,
        v.x
    };
}

// ---- products ----

// The dot product of a and b. Only two vectors of INT32_MIN components overflow it.
static inline int64_t rc_vec2i_dot(rc_vec2i a, rc_vec2i b)
{
    int64_t xx = (int64_t)a.x * b.x;
    int64_t yy = (int64_t)a.y * b.y;

    RC_ASSERT(!rc_add_overflows_i64(xx, yy));
    return xx + yy;
}

// The 2D cross product a.x * b.y - a.y * b.x: positive when b is anticlockwise of a.
static inline int64_t rc_vec2i_wedge(rc_vec2i a, rc_vec2i b)
{
    int64_t xy = (int64_t)a.x * b.y;
    int64_t yx = (int64_t)a.y * b.x;

    RC_ASSERT(!rc_sub_overflows_i64(xy, yx));
    return xy - yx;
}

// The squared length of v.
static inline int64_t rc_vec2i_lengthsqr(rc_vec2i v)
{
    return rc_vec2i_dot(v, v);
}

// ---- comparison ----

// True if a and b are the same vector.
static inline bool rc_vec2i_is_equal(rc_vec2i a, rc_vec2i b)
{
    return a.x == b.x && a.y == b.y;
}

#endif // RC_MATH_VEC2I_H_
