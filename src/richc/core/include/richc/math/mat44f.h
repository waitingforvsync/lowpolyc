// math/mat44f.h
//
// A 4x4 matrix of floats, stored as its columns cx, cy, cz and cw:
//
//   | cx.x  cy.x  cz.x  cw.x |
//   | cx.y  cy.y  cz.y  cw.y |
//   | cx.z  cy.z  cz.z  cw.z |
//   | cx.w  cy.w  cz.w  cw.w |
//
// It multiplies a column vector: m * v = cx * v.x + cy * v.y + cz * v.z + cw * v.w.
//
// Every projection maps to the one clip space we have: x right and y up in [-1, 1], and depth in
// [0, 1] reversed, so the near plane is at 1 and the far plane at 0. View space is right-handed,
// looking down -z. The graphics backends conform to this, not the other way round.

#ifndef RC_MATH_MAT44F_H_
#define RC_MATH_MAT44F_H_

#include "richc/math/mat22f.h"
#include "richc/math/mat34f.h"
#include "richc/math/vec4f.h"

typedef struct rc_mat44f {
    rc_vec4f cx;
    rc_vec4f cy;
    rc_vec4f cz;
    rc_vec4f cw;
} rc_mat44f;

// ---- construction ----

// The matrix with columns cx, cy, cz and cw.
static inline rc_mat44f rc_mat44f_make(rc_vec4f cx, rc_vec4f cy, rc_vec4f cz, rc_vec4f cw)
{
    return (rc_mat44f) {cx, cy, cz, cw};
}

// The matrix with rows rx, ry, rz and rw, for writing one out row by row.
static inline rc_mat44f rc_mat44f_make_transpose(rc_vec4f rx, rc_vec4f ry, rc_vec4f rz, rc_vec4f rw)
{
    return (rc_mat44f) {
        {rx.x, ry.x, rz.x, rw.x},
        {rx.y, ry.y, rz.y, rw.y},
        {rx.z, ry.z, rz.z, rw.z},
        {rx.w, ry.w, rz.w, rw.w}
    };
}

// The matrix of zeros.
static inline rc_mat44f rc_mat44f_make_zero(void)
{
    return (rc_mat44f) {
        rc_vec4f_make_zero(),
        rc_vec4f_make_zero(),
        rc_vec4f_make_zero(),
        rc_vec4f_make_zero()
    };
}

// The identity matrix.
static inline rc_mat44f rc_mat44f_make_identity(void)
{
    return (rc_mat44f) {
        rc_vec4f_make_unitx(),
        rc_vec4f_make_unity(),
        rc_vec4f_make_unitz(),
        rc_vec4f_make_unitw()
    };
}

// The matrix that moves every point by trans.
static inline rc_mat44f rc_mat44f_make_translation(rc_vec3f trans)
{
    return (rc_mat44f) {
        rc_vec4f_make_unitx(),
        rc_vec4f_make_unity(),
        rc_vec4f_make_unitz(),
        rc_vec4f_from_vec3f(trans, 1.0f)
    };
}

// The orthographic projection of the box from left to right, bottom to top, and z_near to z_far
// in front of the camera (view z from -z_near to -z_far). The near face lands on depth 1, and the
// far face on depth 0.
rc_mat44f rc_mat44f_make_ortho(float left, float right, float top, float bottom, float z_near, float z_far);

// The orthographic projection of a width by height rectangle of pixels, with the origin at the
// top left and y down. Points at z = 0 land on the near plane (depth 1).
static inline rc_mat44f rc_mat44f_make_ortho_2d(float width, float height)
{
    return rc_mat44f_make_ortho(0.0f, width, 0.0f, height, 0.0f, 1.0f);
}

// The perspective projection of a camera looking down -z, with a vertical field of view of y_fov
// and a width to height ratio of aspect, and its near and far planes z_near and z_far in front of
// it. View z = -z_near lands on depth 1 and -z_far on depth 0, exactly, and clip w is -z.
rc_mat44f rc_mat44f_make_perspective(float y_fov, float aspect, float z_near, float z_far);

// rc_mat44f_make_perspective with the far plane at infinity: depth is z_near / -z, approaching 0
// far away. The one to reach for in 3D: there is no far plane to tune, and reversed depth keeps
// the precision.
rc_mat44f rc_mat44f_make_perspective_inf(float y_fov, float aspect, float z_near);

// m in the top left, with the identity elsewhere.
static inline rc_mat44f rc_mat44f_from_mat22f(rc_mat22f m)
{
    return (rc_mat44f) {
        rc_vec4f_from_vec2f(m.cx, 0.0f, 0.0f),
        rc_vec4f_from_vec2f(m.cy, 0.0f, 0.0f),
        rc_vec4f_make_unitz(),
        rc_vec4f_make_unitw()
    };
}

// m in the top left, with the identity elsewhere.
static inline rc_mat44f rc_mat44f_from_mat33f(rc_mat33f m)
{
    return (rc_mat44f) {
        rc_vec4f_from_vec3f(m.cx, 0.0f),
        rc_vec4f_from_vec3f(m.cy, 0.0f),
        rc_vec4f_from_vec3f(m.cz, 0.0f),
        rc_vec4f_make_unitw()
    };
}

// The transform m, with a bottom row of (0, 0, 0, 1).
static inline rc_mat44f rc_mat44f_from_mat34f(rc_mat34f m)
{
    return (rc_mat44f) {
        rc_vec4f_from_vec3f(m.rot.cx, 0.0f),
        rc_vec4f_from_vec3f(m.rot.cy, 0.0f),
        rc_vec4f_from_vec3f(m.rot.cz, 0.0f),
        rc_vec4f_from_vec3f(m.trans, 1.0f)
    };
}

// The matrix from sixteen consecutive floats, column by column.
static inline rc_mat44f rc_mat44f_from_floats(const float *f)
{
    return (rc_mat44f) {
        rc_vec4f_from_floats(f),
        rc_vec4f_from_floats(f + 4),
        rc_vec4f_from_floats(f + 8),
        rc_vec4f_from_floats(f + 12)
    };
}

// ---- conversion ----

// The elements as a float[16], column by column.
static inline const float *rc_mat44f_as_floats(const rc_mat44f *m)
{
    return &m->cx.x;
}

// ---- arithmetic ----

// a + b.
static inline rc_mat44f rc_mat44f_add(rc_mat44f a, rc_mat44f b)
{
    return (rc_mat44f) {
        rc_vec4f_add(a.cx, b.cx),
        rc_vec4f_add(a.cy, b.cy),
        rc_vec4f_add(a.cz, b.cz),
        rc_vec4f_add(a.cw, b.cw)
    };
}

// a - b.
static inline rc_mat44f rc_mat44f_sub(rc_mat44f a, rc_mat44f b)
{
    return (rc_mat44f) {
        rc_vec4f_sub(a.cx, b.cx),
        rc_vec4f_sub(a.cy, b.cy),
        rc_vec4f_sub(a.cz, b.cz),
        rc_vec4f_sub(a.cw, b.cw)
    };
}

// m with every element scaled by s.
static inline rc_mat44f rc_mat44f_scalar_mul(rc_mat44f m, float s)
{
    return (rc_mat44f) {
        rc_vec4f_scalar_mul(m.cx, s),
        rc_vec4f_scalar_mul(m.cy, s),
        rc_vec4f_scalar_mul(m.cz, s),
        rc_vec4f_scalar_mul(m.cw, s)
    };
}

// m * v.
static inline rc_vec4f rc_mat44f_vec4f_mul(rc_mat44f m, rc_vec4f v)
{
    return rc_vec4f_add4(
        rc_vec4f_scalar_mul(m.cx, v.x),
        rc_vec4f_scalar_mul(m.cy, v.y),
        rc_vec4f_scalar_mul(m.cz, v.z),
        rc_vec4f_scalar_mul(m.cw, v.w)
    );
}

// a * b: b, then a.
rc_mat44f rc_mat44f_mul(rc_mat44f a, rc_mat44f b);

// ---- inversion ----

// m with its rows and columns swapped.
static inline rc_mat44f rc_mat44f_transpose(rc_mat44f m)
{
    return (rc_mat44f) {
        {m.cx.x, m.cy.x, m.cz.x, m.cw.x},
        {m.cx.y, m.cy.y, m.cz.y, m.cw.y},
        {m.cx.z, m.cy.z, m.cz.z, m.cw.z},
        {m.cx.w, m.cy.w, m.cz.w, m.cw.w}
    };
}

// The determinant of m.
float rc_mat44f_determinant(rc_mat44f m);

// The inverse of m, whose determinant must not be 0.
rc_mat44f rc_mat44f_inverse(rc_mat44f m);

#endif // RC_MATH_MAT44F_H_
