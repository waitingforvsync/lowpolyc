// math/log_f32.c
//
// The natural logarithm of a float. Ported from musl 1.2.5 (MIT), src/math/logf.c, which is Arm's
// optimized-routines. Its notice:
//
// Copyright (c) 2017-2018, Arm Limited.
// SPDX-License-Identifier: MIT
//
// The error is at most 0.818 ulp. Before rounding, the relative error is 1.957 x 2^-26.

#include "exp_log_f32.h"

// log(x) for the bits of a normal x, or a normalised subnormal one. With x = 2^k z and c the
// centre nearest z, log(x) = log1p(z/c - 1) + log(c) + k ln2, and the polynomial gives log1p.
static float rc_log_of_normal_f32(uint32_t x_bits)
{
    rc_log_parts_f32 parts = rc_log_parts_of_f32(x_bits);
    rc_log_centre_f32 centre = rc_log_data_f32.centres[parts.index];
    const double *poly = rc_log_data_f32.poly;
    double r = parts.z * centre.inv_c - 1.0;
    double y0 = centre.log_c + (double)parts.exponent * rc_log_data_f32.ln2;
    double r2 = r * r;
    double low = poly[1] * r + poly[2];
    double terms = poly[0] * r2 + low;

    return (float)(terms * r2 + (y0 + r));
}


float rc_log_f32(float x)
{
    uint32_t bits = rc_bitcast_f32(x);

    if (!rc_log_is_special_f32(bits)) {
        return rc_log_of_normal_f32(bits);
    }

    if ((bits << 1) == 0) {
        return rc_bitcast_u32(0xff800000u);
    }

    if (bits == 0x7f800000u) {
        return x;
    }

    // Below zero, or NaN
    if ((bits & 0x80000000u) != 0) {
        return (x - x) / (x - x);
    }

    if (bits > 0x7f800000u) {
        return (x - x) / (x - x);
    }

    return rc_log_of_normal_f32(rc_log_normalise_subnormal_f32(x));
}
