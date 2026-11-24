// check.c
//
// Quick checks of the arc functions and cbrt against glibc bit for bit: every pairing of special
// and ordinary values for atan2, the same values for the functions of one argument, atan2's odd
// symmetry in y, and exact cubes, scaled into the subnormals, for cbrt. Prints each failure and
// the count. A few failures are expected, since our functions need not round correctly and
// glibc's cbrt does not always: today asin(0.5) (ours 0.517 ulp out) and five of glibc's cbrt
// results (cbrt(2) is 1.12 ulp out, and cbrt(2^-1074) is not the exact 2^-358).
//
//   out/check    (or: ./run.sh check)

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "richc/math/math.h"

#define EQ_F64(ours, theirs) check_f64(#ours, (ours), (theirs))
#define EQ_F32(ours, theirs) check_f32(#ours, (ours), (theirs))

static int failures = 0;


// Whether a and b have the same bits, or are both NaN.
static bool same_f64(double a, double b)
{
    if (a != a && b != b) {
        return true;
    }

    return rc_bitcast_f64(a) == rc_bitcast_f64(b);
}


static bool same_f32(float a, float b)
{
    if (a != a && b != b) {
        return true;
    }

    return rc_bitcast_f32(a) == rc_bitcast_f32(b);
}


static void check_f64(const char *what, double ours, double theirs)
{
    if (!same_f64(ours, theirs)) {
        failures++;
        printf("FAIL %s: %a vs %a\n", what, ours, theirs);
    }
}


static void check_f32(const char *what, float ours, float theirs)
{
    if (!same_f32(ours, theirs)) {
        failures++;
        printf("FAIL %s: %a vs %a\n", what, (double)ours, (double)theirs);
    }
}


// atan2 on every pair of values, and its symmetry in y.
static void check_atan2(const double *values, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = 0; j < count; j++) {
            double y = values[i];
            double x = values[j];

            EQ_F64(rc_atan2_f64(y, x), atan2(y, x));
            EQ_F32(rc_atan2_f32((float)y, (float)x), atan2f((float)y, (float)x));
            EQ_F64(rc_atan2_f64(-y, x), -rc_atan2_f64(y, x));
            EQ_F32(rc_atan2_f32(-(float)y, (float)x), -rc_atan2_f32((float)y, (float)x));
        }
    }
}


// The functions of one argument on every value.
static void check_one_argument(const double *values, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        double x = values[i];
        float f = (float)x;

        EQ_F64(rc_asin_f64(x), asin(x));
        EQ_F64(rc_acos_f64(x), acos(x));
        EQ_F64(rc_atan_f64(x), atan(x));
        EQ_F64(rc_cbrt_f64(x), cbrt(x));
        EQ_F32(rc_asin_f32(f), asinf(f));
        EQ_F32(rc_acos_f32(f), acosf(f));
        EQ_F32(rc_atan_f32(f), atanf(f));
        EQ_F32(rc_cbrt_f32(f), cbrtf(f));
    }
}


// Exact cubes give exact roots, at ordinary sizes, scaled down and scaled into the subnormals.
static void check_cubes(void)
{
    for (int64_t k = 1; k < 300000; k += 7) {
        double y = (double)k;
        double c = y * y * y;

        if (c >= 0x1p53) {
            continue;
        }

        EQ_F64(rc_cbrt_f64(c), y);
        EQ_F64(rc_cbrt_f64(c * 0x1p-900), y * 0x1p-300);
        EQ_F64(rc_cbrt_f64(-c * 0x1p-1074), -y * 0x1p-358);
    }

    for (int k = 1; k < 256; k++) {
        float y = (float)k;
        float c = y * y * y;

        EQ_F32(rc_cbrt_f32(c), y);
        EQ_F32(rc_cbrt_f32(c * 0x1p-147f), y * 0x1p-49f);
        EQ_F32(rc_cbrt_f32(-c * 0x1p-60f), -y * 0x1p-20f);
    }
}


int main(void)
{
    static const double values[] = {
        0.0, -0.0, 1.0, -1.0, 2.0, -2.0, 0.5, 1e-310, -1e-310, 1e300, -1e300, INFINITY, -INFINITY, NAN, 0x1p-1074, 3.0, 1e-20,
    };

    check_atan2(values, RC_COUNT(values));
    check_one_argument(values, RC_COUNT(values));
    check_cubes();
    printf("%d failures\n", failures);
    return 0;
}
