// math/trig_kernel_f32.c
//
// The float kernels of sin, cos and tan on [-pi/4, pi/4]: polynomials evaluated in double, which
// has precision to spare. Ported from musl 1.2.5 (MIT), src/math/__sindf.c, __cosdf.c and
// __tandf.c, which are FreeBSD's k_sinf.c, k_cosf.c and k_tanf.c. The notice of the first two:
//
// Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
// Optimized by Bruce D. Evans (k_cosf.c: debugged and optimized).
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunPro, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.
//
// And of k_tanf.c:
//
// Copyright 2004 Sun Microsystems, Inc.  All Rights Reserved.
//
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.

#include "trig.h"

// |sin(x) / x - s(x)| < 2^-37.5 (~[-4.89e-12, 4.824e-12]).
static const double s1 = -0x15555554cbac77.0p-55;
static const double s2 = 0x111110896efbb2.0p-59;
static const double s3 = -0x1a00f9e2cae774.0p-65;
static const double s4 = 0x16cd878c3b46a7.0p-71;

// |cos(x) - c(x)| < 2^-34.1 (~[-5.37e-11, 5.295e-11]).
static const double c0 = -0x1ffffffd0c5e81.0p-54;
static const double c1 = 0x155553e1053a42.0p-57;
static const double c2 = -0x16c087e80f1e27.0p-62;
static const double c3 = 0x199342e0ee5069.0p-68;

// |tan(x) / x - t(x)| < 2^-25.5 (~[-2e-08, 2e-08]).
static const double t[] = {
    0x15554d3418c99f.0p-54,
    0x1112fd38999f72.0p-55,
    0x1b54c91d865afe.0p-57,
    0x191df3908c33ce.0p-58,
    0x185dadfcecf44e.0p-61,
    0x1362b9bf971bcd.0p-59,
};

// The terms are grouped to be evaluated in parallel, as in the tan kernel.
float rc_sin_kernel_f32(double x)
{
    double x2 = x * x;
    double x4 = x2 * x2;
    double high_terms = s3 + x2 * s4;
    double x3 = x2 * x;

    return (float)((x + x3 * (s1 + x2 * s2)) + x3 * x4 * high_terms);
}


float rc_cos_kernel_f32(double x)
{
    double x2 = x * x;
    double x4 = x2 * x2;
    double high_terms = c2 + x2 * c3;

    return (float)(((1.0 + x2 * c0) + x4 * c1) + (x4 * x2) * high_terms);
}


// The polynomial is split into small independent terms, added from the lowest degree up, so they
// can be evaluated in parallel. That costs two multiplications over Horner's method, and loses
// nothing in accuracy, as we have precision to spare.
float rc_tan_kernel_f32(double x, bool odd)
{
    double x2 = x * x;
    double terms_45 = t[4] + x2 * t[5];
    double terms_23 = t[2] + x2 * t[3];
    double x4 = x2 * x2;
    double x3 = x2 * x;
    double terms_01 = t[0] + x2 * t[1];
    double tan_x = (x + x3 * terms_01) + (x3 * x4) * (terms_23 + x4 * terms_45);

    return (float)(odd ? -1.0 / tan_x : tan_x);
}
