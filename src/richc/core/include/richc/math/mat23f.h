// math/mat23f.h
//
// A 2D affine transform: a linear part, rot, which rotates, scales and shears, then a translation,
// trans. As a 2x3 matrix:
//
//   | rot.cx.x  rot.cy.x  trans.x |
//   | rot.cx.y  rot.cy.y  trans.y |
//
// It moves a point v to rot * v + trans.

#ifndef RC_MATH_MAT23F_H_
#define RC_MATH_MAT23F_H_

#include "richc/math/mat22f.h"

typedef struct rc_mat23f {
    rc_mat22f rot;
    rc_vec2f trans;
} rc_mat23f;

// ---- construction ----

// The transform rot, then trans.
static inline rc_mat23f rc_mat23f_make(rc_mat22f rot, rc_vec2f trans)
{
    return (rc_mat23f) {.rot = rot, .trans = trans};
}

// The transform that leaves every point where it is.
static inline rc_mat23f rc_mat23f_make_identity(void)
{
    return (rc_mat23f) {.rot = rc_mat22f_make_identity(), .trans = rc_vec2f_make_zero()};
}

// The transform that moves every point by trans.
static inline rc_mat23f rc_mat23f_make_translation(rc_vec2f trans)
{
    return (rc_mat23f) {.rot = rc_mat22f_make_identity(), .trans = trans};
}

// The linear map rot, as a transform.
static inline rc_mat23f rc_mat23f_from_mat22f(rc_mat22f rot)
{
    return (rc_mat23f) {.rot = rot, .trans = rc_vec2f_make_zero()};
}

// The transform from six consecutive floats, column by column: rot.cx, rot.cy, trans.
static inline rc_mat23f rc_mat23f_from_floats(const float *f)
{
    return (rc_mat23f) {
        .rot = rc_mat22f_from_floats(f),
        .trans = rc_vec2f_from_floats(f + 4)
    };
}

// ---- conversion ----

// The elements as a float[6], column by column.
static inline const float *rc_mat23f_as_floats(const rc_mat23f *m)
{
    return &m->rot.cx.x;
}

// ---- operations ----

// The point v transformed by m.
static inline rc_vec2f rc_mat23f_vec2f_mul(rc_mat23f m, rc_vec2f v)
{
    return rc_vec2f_add(rc_mat22f_vec2f_mul(m.rot, v), m.trans);
}

// a * b: b, then a.
static inline rc_mat23f rc_mat23f_mul(rc_mat23f a, rc_mat23f b)
{
    return (rc_mat23f) {
        .rot = rc_mat22f_mul(a.rot, b.rot),
        .trans = rc_mat23f_vec2f_mul(a, b.trans)
    };
}

// a * b: the transform b, then the linear map a.
static inline rc_mat23f rc_mat22f_mat23f_mul(rc_mat22f a, rc_mat23f b)
{
    return (rc_mat23f) {
        .rot = rc_mat22f_mul(a, b.rot),
        .trans = rc_mat22f_vec2f_mul(a, b.trans)
    };
}

// a * b: the linear map b, then the transform a.
static inline rc_mat23f rc_mat23f_mat22f_mul(rc_mat23f a, rc_mat22f b)
{
    return (rc_mat23f) {
        .rot = rc_mat22f_mul(a.rot, b),
        .trans = a.trans
    };
}

// The transform that undoes m. Its linear part must have an inverse.
rc_mat23f rc_mat23f_inverse(rc_mat23f m);

#endif // RC_MATH_MAT23F_H_
