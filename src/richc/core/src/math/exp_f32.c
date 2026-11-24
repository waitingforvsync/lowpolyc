// math/exp_f32.c
//
// e^x for floats, as 2^(n / 32) with n = 32 x log2(e) in double, from the kernel in
// exp_log_f32.h. Rounding n moves it by at most 2^-41 below |x| = 111, which moves the result by
// about 2^-46 relative, and the kernel adds under 2^-33, so the error stays within 0.502 ulp. The
// main path takes every x whose e^x is a finite float other than +0, and the double result rounds
// to float as e^x does, subnormals included. Beyond it e^x rounds to +inf or +0, which we return
// straight away. NaN fails both comparisons and takes the slow path too.

#include "richc/math/math.h"

#include "math/exp_log_f32.h"

// True for the x of the main path: from the least x whose e^x doesn't round to +0,
// -0x1.9fe368p6, up to the greatest whose e^x doesn't round to +inf, 0x1.62e42ep6.
static bool rc_exp_f32_is_ordinary(float x)
{
    return x > -0x1.9fe36ap6f && x < 0x1.62e430p6f;
}


// e^x for an x beyond the main path, or NaN: +inf, +0 or the NaN.
static float rc_exp_f32_special(float x)
{
    if (x != x) {
        return x + x;
    }

    return x > 0.0f ? rc_bitcast_u32(0x7f800000u) : 0.0f;
}


float rc_exp_f32(float x)
{
    // The main path first, in a ternary: so gcc and clang (wasm too) both let it fall through
    return rc_exp_f32_is_ordinary(x) ? (float)rc_exp2_f32_kernel((double)x * 0x1.71547652b82fep+5, 0) : rc_exp_f32_special(x);
}
