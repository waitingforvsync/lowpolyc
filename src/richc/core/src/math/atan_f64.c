// math/atan_f64.c
//
// The arctangent of a double. Ported from musl 1.2.5 (MIT), src/math/atan.c, which is FreeBSD's
// s_atan.c. Its notice:
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunPro, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.

#include "richc/math/math.h"

// atan(0.5), atan(1), atan(1.5) and atan(infinity), each split in two, hi + lo.
static const double rc_atan_hi_f64[] = {
    4.63647609000806093515e-01, // 0x3fddac67 0561bb4f
    7.85398163397448278999e-01, // 0x3fe921fb 54442d18
    9.82793723247329054082e-01, // 0x3fef730b d281f69b
    1.57079632679489655800e+00, // 0x3ff921fb 54442d18
};
static const double rc_atan_lo_f64[] = {
    2.26987774529616870924e-17, // 0x3c7a2b7f 222f65e2
    3.06161699786838301793e-17, // 0x3c81a626 33145c07
    1.39033110312309984516e-17, // 0x3c700788 7af0cbbd
    6.12323399573676603587e-17, // 0x3c91a626 33145c07
};

// The odd series for atan on [-7/16, 7/16]: atan(t) ~= t - t^3 (a0 + a1 t^2 + ... + a10 t^20).
static const double rc_atan_series_f64[] = {
    3.33333333333329318027e-01,  // 0x3fd55555 5555550d
    -1.99999999998764832476e-01, // 0xbfc99999 9998ebc4
    1.42857142725034663711e-01,  // 0x3fc24924 920083ff
    -1.11111104054623557880e-01, // 0xbfbc71c6 fe231671
    9.09088713343650656196e-02,  // 0x3fb745cd c54c206e
    -7.69187620504482999495e-02, // 0xbfb3b0f2 af749a6d
    6.66107313738753120669e-02,  // 0x3fb10d66 a0d03d51
    -5.83357013379057348645e-02, // 0xbfadde2d 52defd9a
    4.97687799461593236017e-02,  // 0x3fa97b4b 24760deb
    -3.65315727442169155270e-02, // 0xbfa2b444 2c6a6c2f
    1.62858201153657823623e-02,  // 0x3f90ad3a e322da11
};


// t - atan(t), from the series, split into its odd and even halves for speed.
static double rc_atan_remainder_f64(double t)
{
    const double *a = rc_atan_series_f64;
    double z = t * t;
    double w = z * z;
    double odd = z * (a[0] + w * (a[2] + w * (a[4] + w * (a[6] + w * (a[8] + w * a[10])))));
    double even = w * (a[1] + w * (a[3] + w * (a[5] + w * (a[7] + w * a[9]))));

    return t * (odd + even);
}


// atan(t) plus the tabled angle it was reduced by.
static double rc_atan_offset_f64(uint32_t angle, double t)
{
    return rc_atan_hi_f64[angle] - (rc_atan_remainder_f64(t) - rc_atan_lo_f64[angle] - t);
}


// atan(x) for 7/16 <= x < 2^66, from a tabled angle and the arctangent of what is left:
//   [7/16, 11/16)   atan(1/2) + atan((2x - 1) / (2 + x))
//   [11/16, 19/16)  atan(1) + atan((x - 1) / (x + 1))
//   [19/16, 39/16)  atan(3/2) + atan((x - 1.5) / (1 + 1.5x))
//   [39/16, 2^66)   atan(infinity) + atan(-1 / x)
static double rc_atan_reduced_f64(double x, uint32_t top)
{
    if (top < 0x3fe60000u) {
        return rc_atan_offset_f64(0, (2.0 * x - 1.0) / (2.0 + x));
    }

    if (top < 0x3ff30000u) {
        return rc_atan_offset_f64(1, (x - 1.0) / (x + 1.0));
    }

    if (top < 0x40038000u) {
        return rc_atan_offset_f64(2, (x - 1.5) / (1.0 + 1.5 * x));
    }

    return rc_atan_offset_f64(3, -1.0 / x);
}


double rc_atan_f64(double x)
{
    uint32_t top = (uint32_t)(rc_bitcast_f64(x) >> 32) & 0x7fffffffu;

    // NaN stays NaN, and from 2^66 up atan(x) is +-pi/2 to double precision
    if (x != x) {
        return x;
    }

    if (top >= 0x44100000u) {
        return x < 0.0 ? -rc_atan_hi_f64[3] : rc_atan_hi_f64[3];
    }

    // Below 2^-27, atan(x) is x to double precision
    if (top < 0x3e400000u) {
        return x;
    }

    // Below 7/16, the series alone will do
    if (top < 0x3fdc0000u) {
        return x - rc_atan_remainder_f64(x);
    }

    if (x < 0.0) {
        return -rc_atan_reduced_f64(-x, top);
    }

    return rc_atan_reduced_f64(x, top);
}
