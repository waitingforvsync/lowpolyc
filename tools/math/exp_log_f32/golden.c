// golden.c
//
// Prints the golden rows of test_math_exp_log_f32.c from the current implementation, under a
// comment naming each function, and on stderr each result's error in ulps against glibc's long
// double functions, as a sanity check that no bad value slips in.
//
//   out/golden > rows 2> errors
//   (or: ./run.sh golden, which also checks the rows against the tests)

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "richc/math/math.h"

// Inputs for exp, as bits: tiny arguments, the table's steps, and the edges of overflow and of
// the subnormals.
static const uint32_t exp_inputs[] = {
    0x3f800000, 0xbf800000, 0x3f000000, 0xbe800000, 0x33800000, 0xb3800000, 0x2edbe6ff, 0x3a83126f,
    0x3dcccccd, 0xbdcccccd, 0x3f317218, 0x3fc00000, 0x40000000, 0x40490fdb, 0x40a00000, 0x41200000,
    0xc1200000, 0x41a00000, 0xc1a00000, 0x42000000, 0x42a00000, 0x42b17217, 0xc2aeac4f, 0xc2aeac50,
    0xc2b00000, 0xc2c80000, 0xc2cff1b4, 0xc2ce8ed0, 0x3c23d70a, 0xc0e00000
};

// Inputs for log: either side of 1, the table's edges, subnormals and the extremes.
static const uint32_t log_inputs[] = {
    0x3f000000, 0x40000000, 0x40400000, 0x41200000, 0x3dcccccd, 0x3a83126f, 0x501502f9, 0x3f800001,
    0x3f7fffff, 0x3f7d70a4, 0x3f8147ae, 0x3fb33333, 0x3f333333, 0x3f320000, 0x3fb1ffff, 0x7f7fffff,
    0x00800000, 0x00000001, 0x00000003, 0x007fffff, 0x402df854, 0x40490fdb, 0x42c80000, 0x4640e6b7,
    0x3e99999a, 0x40e00000, 0x0da24260, 0x7149f2ca, 0x3f7e0000, 0x3f820000
};

// Inputs for log2, chosen the same way.
static const uint32_t log2_inputs[] = {
    0x40400000, 0x41200000, 0x3dcccccd, 0x3f800001, 0x3f7fffff, 0x501502f9, 0x7f7fffff, 0x00000003,
    0x007fffff, 0x00400001, 0x3fc00000, 0x3f400000, 0x402df854, 0x40490fdb, 0x3f320000, 0x3fb1ffff,
    0x3f7d70a4, 0x3f8147ae, 0x42c80000, 0x4640e6b7, 0x3e99999a, 0x40e00000, 0x0da24260, 0x7149f2ca,
    0x3f7e0000, 0x3f820000, 0x3f3504f3, 0x3fb504f3
};

// Pairs for pow: roots, x near 1 with y large, overflow and underflow edges, negative x, and
// subnormal x and results.
static const float pow_inputs[][2] = {
    {2.0f, 0.5f}, {2.0f, 10.0f}, {10.0f, -2.0f}, {0.5f, 100.0f}, {3.0f, 0.33333334f}, {1.0001f, 10000.0f},
    {0.9999f, -10000.0f}, {2.5f, 30.0f}, {100.0f, 19.2f}, {100.0f, -22.0f}, {2.0f, -149.0f}, {2.0f, -149.5f},
    {-2.0f, 3.0f}, {-2.0f, -3.0f}, {-1.0000001f, 16777215.0f}, {-0.5f, 24.0f}, {-1.5f, 8388609.0f},
    {0x1p-140f, 0.5f}, {1e-20f, 1.9f}, {7.0f, 1.1f}, {0.3f, 0.7f}, {1.5f, -50.0f}, {5.0f, 55.0f},
    {0.1f, 45.0f}, {-0.1f, 45.0f}, {12.5f, -35.5f}, {1.0000001f, -8388608.0f}, {0.99999994f, 2e9f},
    {9.0f, 1.5f}, {4.0f, -0.5f}, {-8.0f, 2.0f}, {3.0f, 80.5f}
};


// |ours - ref| in ulps of ref, as a float.
static double ulps(float ours, long double ref)
{
    int exponent = 0;

    frexpl(ref, &exponent);
    int unit_exponent = (exponent - 1 < -126 ? -126 : exponent - 1) - 23;

    return (double)(fabsl((long double)ours - ref) / ldexpl(1.0L, unit_exponent));
}


// Prints the rows for a function of one argument, and their errors on stderr.
static void section(const char *name, float (*function)(float), long double (*reference)(long double), const uint32_t *inputs,
    uint32_t count)
{
    printf("// %s\n", name);
    for (uint32_t i = 0; i < count; i++) {
        float x = rc_bitcast_u32(inputs[i]);
        float y = function(x);

        printf("    {0x%08xu, 0x%08xu},   // %.9g -> %.9g\n", inputs[i], rc_bitcast_f32(y), (double)x, (double)y);
        fprintf(stderr, "%s(%a) err %.4f\n", name, (double)x, isinf(y) ? 0.0 : ulps(y, reference(x)));
    }
}


// Prints the rows for pow, and their errors on stderr.
static void section_pow(void)
{
    printf("// pow\n");
    for (uint32_t i = 0; i < RC_COUNT(pow_inputs); i++) {
        float x = pow_inputs[i][0];
        float y = pow_inputs[i][1];
        float r = rc_pow_f32(x, y);
        char base[32] = {0};

        snprintf(base, sizeof base, x < 0 ? "(%.9g)" : "%.9g", (double)x);
        printf("    {0x%08xu, 0x%08xu, 0x%08xu},   // %s ^ %.9g -> %.9g\n", rc_bitcast_f32(x), rc_bitcast_f32(y),
            rc_bitcast_f32(r), base, (double)y, (double)r);
        fprintf(stderr, "pow(%a, %a) err %.4f\n", (double)x, (double)y, isinf(r) ? 0.0 : ulps(r, powl(x, y)));
    }
}


int main(void)
{
    section("exp", rc_exp_f32, expl, exp_inputs, RC_COUNT(exp_inputs));
    section("log", rc_log_f32, logl, log_inputs, RC_COUNT(log_inputs));
    section("log2", rc_log2_f32, log2l, log2_inputs, RC_COUNT(log2_inputs));
    section_pow();
    return 0;
}
