// math/mat34f.h
//
// A 3D affine transform: a linear part, rot, which rotates, scales and shears, then a translation,
// trans. As a 3x4 matrix:
//
//   | rot.cx.x  rot.cy.x  rot.cz.x  trans.x |
//   | rot.cx.y  rot.cy.y  rot.cz.y  trans.y |
//   | rot.cx.z  rot.cy.z  rot.cz.z  trans.z |
//
// It moves a point v to rot * v + trans.

#ifndef RC_MATH_MAT34F_H_
#define RC_MATH_MAT34F_H_

#include "richc/math/mat33f.h"

typedef struct rc_mat34f {
    rc_mat33f rot;
    rc_vec3f trans;
} rc_mat34f;

// ---- construction ----

// The transform rot, then trans.
static inline rc_mat34f rc_mat34f_make(rc_mat33f rot, rc_vec3f trans)
{
    return (rc_mat34f) {.rot = rot, .trans = trans};
}

// The transform that leaves every point where it is.
static inline rc_mat34f rc_mat34f_make_identity(void)
{
    return (rc_mat34f) {.rot = rc_mat33f_make_identity(), .trans = rc_vec3f_make_zero()};
}

// The transform that moves every point by trans.
static inline rc_mat34f rc_mat34f_make_translation(rc_vec3f trans)
{
    return (rc_mat34f) {.rot = rc_mat33f_make_identity(), .trans = trans};
}

// The view transform of a camera at eye looking at focus, right-handed: it moves eye to the origin
// and looks down -z, with y as near to up as it can be. up only has to be roughly right, since the
// true up is worked out from the side and forward directions.
rc_mat34f rc_mat34f_make_lookat(rc_vec3f eye, rc_vec3f focus, rc_vec3f up);

// The linear map rot, as a transform.
static inline rc_mat34f rc_mat34f_from_mat33f(rc_mat33f rot)
{
    return (rc_mat34f) {.rot = rot, .trans = rc_vec3f_make_zero()};
}

// The transform from twelve consecutive floats, column by column: rot.cx, rot.cy, rot.cz, trans.
static inline rc_mat34f rc_mat34f_from_floats(const float *f)
{
    return (rc_mat34f) {
        .rot = rc_mat33f_from_floats(f),
        .trans = rc_vec3f_from_floats(f + 9)
    };
}

// ---- conversion ----

// The elements as a float[12], column by column.
static inline const float *rc_mat34f_as_floats(const rc_mat34f *m)
{
    return &m->rot.cx.x;
}

// ---- operations ----

// The point v transformed by m.
static inline rc_vec3f rc_mat34f_vec3f_mul(rc_mat34f m, rc_vec3f v)
{
    return rc_vec3f_add(rc_mat33f_vec3f_mul(m.rot, v), m.trans);
}

// a * b: b, then a.
static inline rc_mat34f rc_mat34f_mul(rc_mat34f a, rc_mat34f b)
{
    return (rc_mat34f) {
        .rot = rc_mat33f_mul(a.rot, b.rot),
        .trans = rc_mat34f_vec3f_mul(a, b.trans)
    };
}

// a * b: the transform b, then the linear map a.
static inline rc_mat34f rc_mat33f_mat34f_mul(rc_mat33f a, rc_mat34f b)
{
    return (rc_mat34f) {
        .rot = rc_mat33f_mul(a, b.rot),
        .trans = rc_mat33f_vec3f_mul(a, b.trans)
    };
}

// a * b: the linear map b, then the transform a.
static inline rc_mat34f rc_mat34f_mat33f_mul(rc_mat34f a, rc_mat33f b)
{
    return (rc_mat34f) {
        .rot = rc_mat33f_mul(a.rot, b),
        .trans = a.trans
    };
}

// The transform that undoes m. Its linear part must have an inverse.
rc_mat34f rc_mat34f_inverse(rc_mat34f m);

#endif // RC_MATH_MAT34F_H_
