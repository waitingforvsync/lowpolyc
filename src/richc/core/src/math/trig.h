// math/trig.h
//
// What sin, cos and tan share: the kernels that approximate them on [-pi/4, pi/4], and the
// reductions that bring any argument into that range by subtracting a multiple of pi/2. Private
// to core. The float functions work in double throughout, so their kernels take a double.

#ifndef RC_MATH_TRIG_H_
#define RC_MATH_TRIG_H_

#include "richc/math/math.h"

// A float reduced by a multiple of pi/2: x = quadrant * pi/2 + angle, give or take whole turns,
// with |angle| ~<= pi/4.
typedef struct rc_trig_reduced_f32 {
    int32_t quadrant;
    double angle;
} rc_trig_reduced_f32;

// A double reduced by a multiple of pi/2: x = quadrant * pi/2 + hi + lo, give or take whole turns,
// with |hi| ~<= pi/4 and lo the part of the remainder below hi's precision.
typedef struct rc_trig_reduced_f64 {
    int32_t quadrant;
    double hi;
    double lo;
} rc_trig_reduced_f64;

// The top 32 bits of |x|: its exponent and the top 20 bits of its mantissa.
static inline uint32_t rc_trig_abs_high_word_f64(double x)
{
    return (uint32_t)(rc_bitcast_f64(x) >> 32) & 0x7fffffffu;
}

// sin(x) for |x| ~<= pi/4, to float precision.
float rc_sin_kernel_f32(double x);

// cos(x) for |x| ~<= pi/4, to float precision.
float rc_cos_kernel_f32(double x);

// tan(x) for |x| ~<= pi/4, to float precision, or -1 / tan(x) if odd.
float rc_tan_kernel_f32(double x, bool odd);

// sin(x + tail) for |x| ~<= pi/4, where tail is below x's precision and only used if has_tail.
// Never gives -0, so the callers handle tiny x themselves.
double rc_sin_kernel_f64(double x, double tail, bool has_tail);

// cos(x + tail) for |x| ~<= pi/4, where tail is below x's precision.
double rc_cos_kernel_f64(double x, double tail);

// tan(x + tail) for |x| ~<= pi/4, where tail is below x's precision, or -1 / tan(x + tail) if odd.
// Never gives -0, so the callers handle tiny x themselves.
double rc_tan_kernel_f64(double x, double tail, bool odd);

// x less the nearest multiple of pi/2, for finite |x| > pi/4.
rc_trig_reduced_f32 rc_trig_reduce_f32(float x);

// x less the nearest multiple of pi/2, for finite |x| > pi/4.
rc_trig_reduced_f64 rc_trig_reduce_f64(double x);

// The reduction of a large positive x given as num_parts (1 to 3) whole 24-bit parts, the first
// scaled by 2^exponent and each later one by 2^-24 more. The quadrant comes back modulo 8. With
// is_double false, only hi is meant to be used, and is good to a float's precision.
rc_trig_reduced_f64 rc_trig_reduce_large(const double *parts, int32_t num_parts, int32_t exponent, bool is_double);

#endif // RC_MATH_TRIG_H_
