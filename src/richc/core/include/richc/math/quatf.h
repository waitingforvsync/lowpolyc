// math/quatf.h
//
// A quaternion of floats, w + x i + y j + z k (Hamilton's convention), held as its vector part xyz
// and its scalar part w. A unit quaternion is a rotation. Treating it as a 4-vector (add, scale,
// lerp) doesn't keep it unit length, so normalise when a rotation is wanted again.

#ifndef RC_MATH_QUATF_H_
#define RC_MATH_QUATF_H_

#include "richc/math/mat33f.h"

typedef struct rc_quatf {
    rc_vec3f xyz;
    float w;
} rc_quatf;

// ---- construction ----

// The quaternion w + x i + y j + z k.
static inline rc_quatf rc_quatf_make(float x, float y, float z, float w)
{
    return (rc_quatf) {.xyz = rc_vec3f_make(x, y, z), .w = w};
}

// The rotation that leaves everything where it is.
static inline rc_quatf rc_quatf_make_identity(void)
{
    return (rc_quatf) {.xyz = rc_vec3f_make_zero(), .w = 1.0f};
}

// The rotation by angle about axis, which needn't be unit length. For a unit axis it is
// (sin(angle / 2) axis, cos(angle / 2)).
rc_quatf rc_quatf_make_angle_axis(float angle, rc_vec3f axis);

// The quaternion from four consecutive floats: x, y, z and w.
static inline rc_quatf rc_quatf_from_floats(const float *f)
{
    return (rc_quatf) {.xyz = rc_vec3f_from_floats(f), .w = f[3]};
}

// The quaternion with vector part xyz and scalar part w.
static inline rc_quatf rc_quatf_from_vec3f(rc_vec3f xyz, float w)
{
    return (rc_quatf) {.xyz = xyz, .w = w};
}

// The rotation m makes, which must be orthonormal.
rc_quatf rc_quatf_from_mat33f(rc_mat33f m);

// ---- conversion ----

// The components as a float[4]: x, y, z and w.
static inline const float *rc_quatf_as_floats(const rc_quatf *q)
{
    return &q->xyz.x;
}

// The rotation matrix of the unit quaternion q.
rc_mat33f rc_mat33f_from_quatf(rc_quatf q);

// ---- arithmetic as a 4-vector ----

// a + b.
static inline rc_quatf rc_quatf_add(rc_quatf a, rc_quatf b)
{
    return (rc_quatf) {.xyz = rc_vec3f_add(a.xyz, b.xyz), .w = a.w + b.w};
}

// a - b.
static inline rc_quatf rc_quatf_sub(rc_quatf a, rc_quatf b)
{
    return (rc_quatf) {.xyz = rc_vec3f_sub(a.xyz, b.xyz), .w = a.w - b.w};
}

// q with every component scaled by s.
static inline rc_quatf rc_quatf_scalar_mul(rc_quatf q, float s)
{
    return (rc_quatf) {.xyz = rc_vec3f_scalar_mul(q.xyz, s), .w = q.w * s};
}

// -q, which is the same rotation as q.
static inline rc_quatf rc_quatf_negate(rc_quatf q)
{
    return (rc_quatf) {.xyz = rc_vec3f_negate(q.xyz), .w = -q.w};
}

// The dot product of a and b as 4-vectors.
static inline float rc_quatf_dot(rc_quatf a, rc_quatf b)
{
    return rc_vec3f_dot(a.xyz, b.xyz) + a.w * b.w;
}

// The squared length of q.
static inline float rc_quatf_lengthsqr(rc_quatf q)
{
    return rc_quatf_dot(q, q);
}

// The length of q.
static inline float rc_quatf_length(rc_quatf q)
{
    return rc_sqrt_f32(rc_quatf_lengthsqr(q));
}

// q scaled to length 1. q must not be zero.
static inline rc_quatf rc_quatf_normalize(rc_quatf q)
{
    float length = rc_quatf_length(q);

    RC_ASSERT(length != 0.0f);
    return rc_quatf_scalar_mul(q, 1.0f / length);
}

// ---- rotation ----

// q with its vector part negated, which for a unit quaternion is its inverse.
static inline rc_quatf rc_quatf_conjugate(rc_quatf q)
{
    return (rc_quatf) {.xyz = rc_vec3f_negate(q.xyz), .w = q.w};
}

// The inverse of q, its conjugate over its squared length. q must not be zero.
static inline rc_quatf rc_quatf_inverse(rc_quatf q)
{
    float lengthsqr = rc_quatf_lengthsqr(q);

    RC_ASSERT(lengthsqr != 0.0f);
    return rc_quatf_scalar_mul(rc_quatf_conjugate(q), 1.0f / lengthsqr);
}

// a * b: the rotation b, then a. Its vector part is a.w b.xyz + b.w a.xyz + a.xyz x b.xyz, and
// its scalar part a.w b.w - a.xyz . b.xyz.
static inline rc_quatf rc_quatf_mul(rc_quatf a, rc_quatf b)
{
    rc_vec3f xyz = rc_vec3f_add3(rc_vec3f_scalar_mul(b.xyz, a.w), rc_vec3f_scalar_mul(a.xyz, b.w), rc_vec3f_cross(a.xyz, b.xyz));

    return (rc_quatf) {.xyz = xyz, .w = a.w * b.w - rc_vec3f_dot(a.xyz, b.xyz)};
}

// v rotated by the unit quaternion q. With t = 2 (q.xyz x v), that is v + q.w t + q.xyz x t: two
// cross products, and no matrix.
static inline rc_vec3f rc_quatf_vec3f_transform(rc_quatf q, rc_vec3f v)
{
    rc_vec3f t = rc_vec3f_scalar_mul(rc_vec3f_cross(q.xyz, v), 2.0f);

    return rc_vec3f_add3(v, rc_vec3f_scalar_mul(t, q.w), rc_vec3f_cross(q.xyz, t));
}

// The angle the unit quaternion q rotates by, in [0, 2 pi]. atan2 keeps it accurate over the
// whole range, where acos(w) would lose it near no rotation.
static inline float rc_quatf_angle(rc_quatf q)
{
    return 2.0f * rc_atan2_f32(rc_vec3f_length(q.xyz), q.w);
}

// The axis q rotates about, of length 1; the x axis when q hardly rotates at all.
static inline rc_vec3f rc_quatf_axis(rc_quatf q)
{
    float length = rc_vec3f_length(q.xyz);

    if (length < 1e-6f) {
        return rc_vec3f_make_unitx();
    }

    return rc_vec3f_scalar_mul(q.xyz, 1.0f / length);
}

// ---- interpolation ----

// The 4-vector t of the way from a to b, which is generally not unit length.
static inline rc_quatf rc_quatf_lerp(rc_quatf a, rc_quatf b, float t)
{
    return rc_quatf_add(a, rc_quatf_scalar_mul(rc_quatf_sub(b, a), t));
}

// The lerp normalised: cheap, but it doesn't turn at a steady rate.
static inline rc_quatf rc_quatf_nlerp(rc_quatf a, rc_quatf b, float t)
{
    return rc_quatf_normalize(rc_quatf_lerp(a, b, t));
}

// The rotation t of the way from a to b, turning at a steady rate the shorter way round.
rc_quatf rc_quatf_slerp(rc_quatf a, rc_quatf b, float t);

// ---- exponential and logarithm ----

// e to the power q. A quaternion with no scalar part exponentiates to a unit one: its rotation.
rc_quatf rc_quatf_exp(rc_quatf q);

// The natural logarithm of q, which undoes rc_quatf_exp. q must not be zero.
rc_quatf rc_quatf_log(rc_quatf q);

// q to the power t, exp(t log(q)): for a unit q, the rotation t of the way round.
rc_quatf rc_quatf_pow(rc_quatf q, float t);

// ---- comparison ----

// True if a and b are the same quaternion. -q is the same rotation as q, but not equal to it.
static inline bool rc_quatf_is_equal(rc_quatf a, rc_quatf b)
{
    return rc_vec3f_is_equal(a.xyz, b.xyz) && a.w == b.w;
}

// True if a and b are less than tolerance apart as 4-vectors.
static inline bool rc_quatf_is_nearly_equal(rc_quatf a, rc_quatf b, float tolerance)
{
    return rc_quatf_lengthsqr(rc_quatf_sub(a, b)) < tolerance * tolerance;
}

#endif // RC_MATH_QUATF_H_
