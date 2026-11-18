// math/mat33f.h
//
// A 3x3 matrix of floats, stored as its columns cx, cy and cz:
//
//   | cx.x  cy.x  cz.x |
//   | cx.y  cy.y  cz.y |
//   | cx.z  cy.z  cz.z |
//
// It multiplies a column vector: m * v = cx * v.x + cy * v.y + cz * v.z.

#ifndef RC_MATH_MAT33F_H_
#define RC_MATH_MAT33F_H_

#include "richc/math/vec3f.h"

typedef struct rc_mat33f {
    rc_vec3f cx;
    rc_vec3f cy;
    rc_vec3f cz;
} rc_mat33f;

// ---- construction ----

// The matrix with columns cx, cy and cz.
static inline rc_mat33f rc_mat33f_make(rc_vec3f cx, rc_vec3f cy, rc_vec3f cz)
{
    return (rc_mat33f) {cx, cy, cz};
}

// The matrix with rows rx, ry and rz, for writing one out row by row.
static inline rc_mat33f rc_mat33f_make_transpose(rc_vec3f rx, rc_vec3f ry, rc_vec3f rz)
{
    return (rc_mat33f) {
        {rx.x, ry.x, rz.x},
        {rx.y, ry.y, rz.y},
        {rx.z, ry.z, rz.z}
    };
}

// The matrix of zeros.
static inline rc_mat33f rc_mat33f_make_zero(void)
{
    return (rc_mat33f) {
        rc_vec3f_make_zero(),
        rc_vec3f_make_zero(),
        rc_vec3f_make_zero()
    };
}

// The identity matrix.
static inline rc_mat33f rc_mat33f_make_identity(void)
{
    return (rc_mat33f) {
        rc_vec3f_make_unitx(),
        rc_vec3f_make_unity(),
        rc_vec3f_make_unitz()
    };
}

// The rotation by angle about the x axis, turning y towards z.
rc_mat33f rc_mat33f_make_rotation_x(float angle);

// The rotation by angle about the y axis, turning z towards x.
rc_mat33f rc_mat33f_make_rotation_y(float angle);

// The rotation by angle about the z axis, turning x towards y.
rc_mat33f rc_mat33f_make_rotation_z(float angle);

// The matrix from nine consecutive floats, column by column.
static inline rc_mat33f rc_mat33f_from_floats(const float *f)
{
    return (rc_mat33f) {
        rc_vec3f_from_floats(f),
        rc_vec3f_from_floats(f + 3),
        rc_vec3f_from_floats(f + 6)
    };
}

// ---- conversion ----

// The elements as a float[9], column by column.
static inline const float *rc_mat33f_as_floats(const rc_mat33f *m)
{
    return &m->cx.x;
}

// ---- arithmetic ----

// a + b.
static inline rc_mat33f rc_mat33f_add(rc_mat33f a, rc_mat33f b)
{
    return (rc_mat33f) {
        rc_vec3f_add(a.cx, b.cx),
        rc_vec3f_add(a.cy, b.cy),
        rc_vec3f_add(a.cz, b.cz)
    };
}

// a - b.
static inline rc_mat33f rc_mat33f_sub(rc_mat33f a, rc_mat33f b)
{
    return (rc_mat33f) {
        rc_vec3f_sub(a.cx, b.cx),
        rc_vec3f_sub(a.cy, b.cy),
        rc_vec3f_sub(a.cz, b.cz)
    };
}

// m with every element scaled by s.
static inline rc_mat33f rc_mat33f_scalar_mul(rc_mat33f m, float s)
{
    return (rc_mat33f) {
        rc_vec3f_scalar_mul(m.cx, s),
        rc_vec3f_scalar_mul(m.cy, s),
        rc_vec3f_scalar_mul(m.cz, s)
    };
}

// m * v.
static inline rc_vec3f rc_mat33f_vec3f_mul(rc_mat33f m, rc_vec3f v)
{
    return rc_vec3f_add3(rc_vec3f_scalar_mul(m.cx, v.x), rc_vec3f_scalar_mul(m.cy, v.y), rc_vec3f_scalar_mul(m.cz, v.z));
}

// a * b: b, then a.
rc_mat33f rc_mat33f_mul(rc_mat33f a, rc_mat33f b);

// ---- inversion ----

// The determinant of m, as the scalar triple product cx . (cy x cz).
static inline float rc_mat33f_determinant(rc_mat33f m)
{
    return rc_vec3f_dot(m.cx, rc_vec3f_cross(m.cy, m.cz));
}

// m with its rows and columns swapped.
static inline rc_mat33f rc_mat33f_transpose(rc_mat33f m)
{
    return (rc_mat33f) {
        {m.cx.x, m.cy.x, m.cz.x},
        {m.cx.y, m.cy.y, m.cz.y},
        {m.cx.z, m.cy.z, m.cz.z}
    };
}

// The inverse of m, whose determinant must not be 0. The cross products of pairs of columns are
// the rows of the adjugate, and dividing that by the determinant gives the inverse.
rc_mat33f rc_mat33f_inverse(rc_mat33f m);

#endif // RC_MATH_MAT33F_H_
