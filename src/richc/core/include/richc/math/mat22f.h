// math/mat22f.h
//
// A 2x2 matrix of floats, stored as its columns cx and cy:
//
//   | cx.x  cy.x |
//   | cx.y  cy.y |
//
// It multiplies a column vector: m * v = cx * v.x + cy * v.y.

#ifndef RC_MATH_MAT22F_H_
#define RC_MATH_MAT22F_H_

#include "richc/math/vec2f.h"

typedef struct rc_mat22f {
    rc_vec2f cx;
    rc_vec2f cy;
} rc_mat22f;

// ---- construction ----

// The matrix with columns cx and cy.
static inline rc_mat22f rc_mat22f_make(rc_vec2f cx, rc_vec2f cy)
{
    return (rc_mat22f) {cx, cy};
}

// The matrix of zeros.
static inline rc_mat22f rc_mat22f_make_zero(void)
{
    return (rc_mat22f) {rc_vec2f_make_zero(), rc_vec2f_make_zero()};
}

// The identity matrix.
static inline rc_mat22f rc_mat22f_make_identity(void)
{
    return (rc_mat22f) {rc_vec2f_make_unitx(), rc_vec2f_make_unity()};
}

// The rotation anticlockwise by angle.
rc_mat22f rc_mat22f_make_rotation(float angle);

// The matrix from four consecutive floats, column by column.
static inline rc_mat22f rc_mat22f_from_floats(const float *f)
{
    return (rc_mat22f) {
        rc_vec2f_from_floats(f),
        rc_vec2f_from_floats(f + 2)
    };
}

// ---- conversion ----

// The elements as a float[4], column by column.
static inline const float *rc_mat22f_as_floats(const rc_mat22f *m)
{
    return &m->cx.x;
}

// ---- arithmetic ----

// a + b.
static inline rc_mat22f rc_mat22f_add(rc_mat22f a, rc_mat22f b)
{
    return (rc_mat22f) {
        rc_vec2f_add(a.cx, b.cx),
        rc_vec2f_add(a.cy, b.cy)
    };
}

// a - b.
static inline rc_mat22f rc_mat22f_sub(rc_mat22f a, rc_mat22f b)
{
    return (rc_mat22f) {
        rc_vec2f_sub(a.cx, b.cx),
        rc_vec2f_sub(a.cy, b.cy)
    };
}

// m with every element scaled by s.
static inline rc_mat22f rc_mat22f_scalar_mul(rc_mat22f m, float s)
{
    return (rc_mat22f) {
        rc_vec2f_scalar_mul(m.cx, s),
        rc_vec2f_scalar_mul(m.cy, s)
    };
}

// m * v.
static inline rc_vec2f rc_mat22f_vec2f_mul(rc_mat22f m, rc_vec2f v)
{
    return rc_vec2f_add(rc_vec2f_scalar_mul(m.cx, v.x), rc_vec2f_scalar_mul(m.cy, v.y));
}

// a * b: b, then a.
static inline rc_mat22f rc_mat22f_mul(rc_mat22f a, rc_mat22f b)
{
    return (rc_mat22f) {
        rc_mat22f_vec2f_mul(a, b.cx),
        rc_mat22f_vec2f_mul(a, b.cy)
    };
}

// ---- inversion ----

// The determinant of m.
static inline float rc_mat22f_determinant(rc_mat22f m)
{
    return m.cx.x * m.cy.y - m.cx.y * m.cy.x;
}

// m with its rows and columns swapped.
static inline rc_mat22f rc_mat22f_transpose(rc_mat22f m)
{
    return (rc_mat22f) {
        {m.cx.x, m.cy.x},
        {m.cx.y, m.cy.y}
    };
}

// The inverse of m, whose determinant must not be 0.
rc_mat22f rc_mat22f_inverse(rc_mat22f m);

#endif // RC_MATH_MAT22F_H_
