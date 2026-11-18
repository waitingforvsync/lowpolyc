// math/trig_kernel_f64.c
//
// The double kernels of sin, cos and tan on [-pi/4, pi/4], each taking the reduced argument as a
// head x and a tail below its precision. Ported from musl 1.2.5 (MIT), src/math/__sin.c, __cos.c
// and __tan.c, which are FreeBSD's k_sin.c, k_cos.c and k_tan.c. The notice of the first two:
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunSoft, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.
//
// And of k_tan.c:
//
// Copyright 2004 Sun Microsystems, Inc.  All Rights Reserved.
//
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.

#include "trig.h"

// sin(x) ~= x + s1 x^3 + ... + s6 x^13 on [0, pi/4], to within 2^-58 in sin(x) / x.
static const double s1 = -1.66666666666666324348e-01;
static const double s2 = 8.33333333332248946124e-03;
static const double s3 = -1.98412698298579493134e-04;
static const double s4 = 2.75573137070700676789e-06;
static const double s5 = -2.50507602534068634195e-08;
static const double s6 = 1.58969099521155010221e-10;

// cos(x) ~= 1 - x^2 / 2 + c1 x^4 + ... + c6 x^14 on [0, pi/4], with an error of at most 2^-58.
static const double c1 = 4.16666666666666019037e-02;
static const double c2 = -1.38888888888741095749e-03;
static const double c3 = 2.48015872894767294178e-05;
static const double c4 = -2.75573143513906633035e-07;
static const double c5 = 2.08757232129817482790e-09;
static const double c6 = -1.13596475577881948265e-11;

// tan(x) ~= x + t0 x^3 + ... + t12 x^27 on [0, 0.67434], to within 2^-59.2 in tan(x) / x.
static const double t[] = {
    3.33333333333334091986e-01,
    1.33333333333201242699e-01,
    5.39682539762260521377e-02,
    2.18694882948595424599e-02,
    8.86323982359930005737e-03,
    3.59207910759131235356e-03,
    1.45620945432529025516e-03,
    5.88041240820264096874e-04,
    2.46463134818469906812e-04,
    7.81794442939557092300e-05,
    7.14072491382608190305e-05,
    -1.85586374855275456654e-05,
    2.59073051863633712884e-05,
};

// pi/4 as a head and tail.
static const double pio4 = 7.85398163397448278999e-01;
static const double pio4_lo = 3.06161699786838301793e-17;

// sin(x + tail) ~= sin(x) + (1 - x^2 / 2) tail. With r = s2 + s3 x^2 + ... + s6 x^10, that is
// x + (s1 x^3 + (x^2 (x^3 r - tail / 2) + tail)), and without a tail just x + x^3 (s1 + x^2 r).
double rc_sin_kernel_f64(double x, double tail, bool has_tail)
{
    double x2 = x * x;
    double x4 = x2 * x2;
    double high_terms = s2 + x2 * (s3 + x2 * s4) + x2 * x4 * (s5 + x2 * s6);
    double x3 = x2 * x;

    if (!has_tail) {
        return x + x3 * (s1 + x2 * high_terms);
    }

    return x - ((x2 * (0.5 * tail - x3 * high_terms) - tail) - x3 * s1);
}


// cos(x + tail) ~= cos(x) - x tail = 1 - (x^2 / 2 - (r - x tail)), with r = c1 x^4 + ... + c6 x^14.
// We work out 1 - x^2 / 2 as a head and the tiny correction that makes it exact, and add the
// small terms to the correction first.
double rc_cos_kernel_f64(double x, double tail)
{
    double x2 = x * x;
    double x4 = x2 * x2;
    double high_terms = x2 * (c1 + x2 * (c2 + x2 * c3)) + x4 * x4 * (c4 + x2 * (c5 + x2 * c6));
    double half_x2 = 0.5 * x2;
    double head = 1.0 - half_x2;

    return head + (((1.0 - head) - half_x2) + (x2 * high_terms - x * tail));
}


// The r with tan(x + tail) ~= x + r, for |x| < 0.6744. As tan(x + tail) ~= tan(x) + tail (1 + x^2),
// r = t0 x^3 + (x^2 (x^3 (odd + even) + tail) + tail), where odd = t1 + t3 x^4 + ... + t11 x^20 and
// even = t2 x^2 + t4 x^6 + ... + t12 x^22 split the polynomial into halves to evaluate in parallel.
static double rc_tan_correction_f64(double x, double tail)
{
    double x2 = x * x;
    double x4 = x2 * x2;
    double odd_terms = t[1] + x4 * (t[3] + x4 * (t[5] + x4 * (t[7] + x4 * (t[9] + x4 * t[11]))));
    double even_terms = x2 * (t[2] + x4 * (t[4] + x4 * (t[6] + x4 * (t[8] + x4 * (t[10] + x4 * t[12])))));
    double x3 = x2 * x;

    return tail + x2 * (x3 * (odd_terms + even_terms) + tail) + x3 * t[0];
}


// x with the low 32 bits of its mantissa cleared.
static double rc_tan_clear_low_word(double x)
{
    return rc_bitcast_u64(rc_bitcast_f64(x) & 0xffffffff00000000ull);
}


// -1 / (x + correction). A plain division would have up to 2 ulp of error, so we split the sum
// and its reciprocal each into a short head and the rest, and refine the head of the reciprocal.
static double rc_tan_negative_reciprocal(double x, double correction)
{
    double sum = x + correction;
    double sum_head = rc_tan_clear_low_word(sum);
    double sum_tail = correction - (sum_head - x);
    double reciprocal = -1.0 / sum;
    double reciprocal_head = rc_tan_clear_low_word(reciprocal);

    return reciprocal_head + reciprocal * (1.0 + reciprocal_head * sum_head + reciprocal_head * sum_tail);
}


// For 0.6744 <= |x| <= pi/4, with y = pi/4 - |x|, tan(|x|) = (1 - tan(y)) / (1 + tan(y)) =
// 1 - 2 (tan(y) - tan(y)^2 / (1 + tan(y))), and -1 / tan(|x|) the same with the signs turned.
static double rc_tan_near_pio4_f64(double x, double tail, bool odd)
{
    bool negative = (rc_bitcast_f64(x) >> 63) != 0;
    double abs_x = negative ? -x : x;
    double abs_tail = negative ? -tail : tail;
    double y = (pio4 - abs_x) + (pio4_lo - abs_tail);
    double correction = rc_tan_correction_f64(y, 0.0);
    double tan_y = y + correction;
    double one = odd ? -1.0 : 1.0;
    double tan_abs_x = one - 2.0 * (y + (correction - tan_y * tan_y / (tan_y + one)));

    return negative ? -tan_abs_x : tan_abs_x;
}


// tan(x) for |x| < 0.6744.
static double rc_tan_small_f64(double x, double tail, bool odd)
{
    double correction = rc_tan_correction_f64(x, tail);

    return odd ? rc_tan_negative_reciprocal(x, correction) : x + correction;
}


double rc_tan_kernel_f64(double x, double tail, bool odd)
{
    // |x| >= 0.6744
    if (rc_trig_abs_high_word_f64(x) >= 0x3fe59428u) {
        return rc_tan_near_pio4_f64(x, tail, odd);
    }

    return rc_tan_small_f64(x, tail, odd);
}
