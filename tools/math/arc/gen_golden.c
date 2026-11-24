// gen_golden.c
//
// Prints the golden rows of test_math_arc.c and test_math_cbrt.c: each input, as a hex float
// literal, with the bits our function gives for it, under a line naming the function and width.
//
//   out/gen_golden    (or: ./run.sh golden, which also checks the rows against the tests)

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "richc/math/math.h"


// x as a C hex float literal, without the + of a positive exponent: 0x1.8p+1 -> 0x1.8p1.
static void literal_f64(char *out, size_t size, double x)
{
    char *plus = NULL;

    snprintf(out, size, "%a", x);
    plus = strstr(out, "p+");
    if (plus != NULL) {
        memmove(plus + 1, plus + 2, strlen(plus + 2) + 1);
    }
}


// x as a C float literal, as literal_f64 with an f.
static void literal_f32(char *out, size_t size, float x)
{
    literal_f64(out, size, (double)x);
    strncat(out, "f", size - strlen(out) - 1);
}


static void row_f64(double (*function)(double), double x)
{
    char a[64] = {0};

    literal_f64(a, sizeof a, x);
    printf("    {%s, 0x%016llxull},\n", a, (unsigned long long)rc_bitcast_f64(function(x)));
}


static void row_f32(float (*function)(float), float x)
{
    char a[64] = {0};

    literal_f32(a, sizeof a, x);
    printf("    {%s, 0x%08xu},\n", a, rc_bitcast_f32(function(x)));
}


static void row2_f64(double y, double x)
{
    char a[64] = {0};
    char b[64] = {0};

    literal_f64(a, sizeof a, y);
    literal_f64(b, sizeof b, x);
    printf("    {%s, %s, 0x%016llxull},\n", a, b, (unsigned long long)rc_bitcast_f64(rc_atan2_f64(y, x)));
}


static void row2_f32(float y, float x)
{
    char a[64] = {0};
    char b[64] = {0};

    literal_f32(a, sizeof a, y);
    literal_f32(b, sizeof b, x);
    printf("    {%s, %s, 0x%08xu},\n", a, b, rc_bitcast_f32(rc_atan2_f32(y, x)));
}


// Every branch of asin and acos: tiny, the ratio up to 1/2, the boundary, the square-root side
// with its root split up to 0.975 and whole beyond, and negatives.
static const double arcsine_inputs[] = {
    0x1p-1070, 1e-300, 1e-9, 0x1p-27, 0.001, 0.1, 0.2, 0.25, 0.3, 0.4, 0.45, 0.4999999999999999, 0.5, 0.5000000000000001,
    0.55, 0.6, 0.7, 0.75, 0.8, 0.866, 0.9, 0.95, 0.975, 0.9750000000000001, 0.99, 0.999, 0.999999, 0x1.fffffffffffffp-1,
    -0.1, -0.35, -0.5, -0.65, -0.9, -0.975, -0.99999, -0x1.fffffffffffffp-1,
};

// Every branch of atan: tiny, below the table, each binade of it, its edges, beyond, and huge.
static const double atan_inputs[] = {
    0x1p-1070, 1e-300, 1e-9, 0x1.fffffffffffffp-28, 0x1p-27, 0.01, 0x1.fffffffffffffp-5, 0x1p-4, 0.07, 0.1, 0.2, 0.3, 0.45,
    0.6, 0.85, 1.0, 1.3, 2.5, 3.7, 6.0, 11.0, 0x1.fffffffffffffp3, 16.0, 17.5, 100.0, 12345.0, 1e10, 0x1.fffffffffffffp53,
    0x1p54, 1e20, 1e300, -0.25, -5.0, -1e6, -1e30,
};

// Every octant, the table and the small ratio (with and without its correction, either side of
// 2^-30), y negative, and pairs whose sizes need scaling or underflow.
static const double atan2_inputs[][2] = {
    {1.0, 2.0}, {2.0, 1.0}, {2.0, -1.0}, {1.0, -2.0}, {-1.0, 2.0}, {-2.0, -1.0}, {1.0, 1.0}, {1.0, -1.0}, {0.6, 0.8},
    {3.0, 7.0}, {7.0, -3.0}, {0.01, 1.0}, {1e-5, 3.0}, {3.0, 1e-5}, {-1e-5, -3.0}, {1e-12, 1.0}, {1.0, -1e-12},
    {0x1p-4, 1.0}, {1.0, 0x1.0000000000001p4}, {1e-310, 3e-310}, {5e-324, 1e-320}, {1e300, 2e300}, {-1.7e308, 1e308},
    {1e-300, 1e300}, {5e-324, 1.0}, {1e-310, 2.0}, {123.456, -654.321}, {-0.001, 1000.0}, {0x1p-30, 1.0},
    {0x1.fffffffffffffp-31, 1.0}, {-0x1p-31, -1.0}, {-3.0, -1e-20}, {-0.6, -0.8}, {-7.0, 3.0},
};

// Exact cubes, the three exponent residues, subnormals, the edges of the scaled ranges, and the
// extremes.
static const double cbrt_inputs[] = {
    1.0, 2.0, 3.0, 4.0, 7.999999, 8.0, 10.0, 27.0, 0.1, 0.001, 0.5, 1.5, 12345.678, 1e10, -1e10, 1e300, 0x1.fffffffffffffp1023,
    0x1p-1022, 1e-310, 5e-324, 0x1.fffffffffffffp-1, -0.3, -64.0, -1e-200, 1e-20, 2251799813685248.0, 1953125.0, 0x1p600,
    0x1.0000000000001p600, 0x1p-600, 0x1.fffffffffffffp-601, -0x1.8p700, 0x1.ffffffffffffep-1023, -0x1.8p-1070,
    0x1p-1060,
};

static const float arcsine_inputs_f32[] = {
    0x1p-149f, 1e-30f, 1e-5f, 0.001f, 0.1f, 0.25f, 0.3f, 0.45f, 0x1.fffffep-2f, 0.5f, 0x1.000002p-1f, 0.6f, 0.7f, 0.8f,
    0.9f, 0.95f, 0.99f, 0.9999f, 0x1.fffffep-1f, -0.2f, -0.55f, -0.75f, -0.999f, -0x1.fffffep-1f,
};

static const float atan_inputs_f32[] = {
    0x1p-149f, 1e-30f, 1e-5f, 0.01f, 0.1f, 0.3f, 0x1.a82798p-2f, 0x1.a8279ap-2f, 0.5f, 0.75f, 0x1.fffffep-1f, 1.0f,
    0x1.000002p0f, 1.25f, 2.0f, 0x1.3504f2p1f, 0x1.3504f4p1f, 3.3f, 10.0f, 100.0f, 1e5f, 1e20f, 0x1.fffffep127f, -0.4f,
    -0x1.a8279ap-2f, -7.0f, -1e10f, 0x1.fffffep25f, 0x1p26f, -0x1p26f,
};

static const float atan2_inputs_f32[][2] = {
    {1.0f, 2.0f}, {2.0f, 1.0f}, {2.0f, -1.0f}, {1.0f, -2.0f}, {-1.0f, 2.0f}, {-2.0f, -1.0f}, {1.0f, 1.0f},
    {1.0f, -1.0f}, {0.6f, 0.8f}, {3.0f, 7.0f}, {7.0f, -3.0f}, {1e-5f, 3.0f}, {3.0f, 1e-5f}, {1e-30f, 1e30f},
    {0x1p-149f, 1.0f}, {0x1p-149f, 0x1p-148f}, {1e38f, 3e38f}, {-1e38f, -1e-38f}, {123.456f, -654.321f},
    {-0.001f, 1000.0f}, {0x1p-126f, 0x1.8p-120f}, {0x1.a82798p-2f, 1.0f}, {0x1.a8279ap-2f, -1.0f}, {-3.0f, 0x1.3504f4p1f},
    {0x1p-15f, 1.0f}, {0x1.fffffep-16f, -1.0f}, {-1e-6f, -2.0f}, {-2.0f, 1e-6f},
};

static const float cbrt_inputs_f32[] = {
    1.0f, 2.0f, 3.0f, 4.0f, 8.0f, 10.0f, 27.0f, 0.1f, 0.001f, 0.5f, 1.5f, 12345.678f, 1e10f, -1e10f, 0x1.fffffep127f,
    0x1p-126f, 1e-40f, 0x1p-149f, 0x1.fffffep-1f, -0.3f, -64.0f, -1e-20f, 3375.0f, 1953125.0f, 0x1.fffffcp-127f,
    -0x1.8p-140f, 0x1p-147f,
};


// Prints a heading and the rows for a function of one double over count inputs.
static void section_f64(const char *name, double (*function)(double), const double *inputs, uint32_t count)
{
    puts(name);
    for (uint32_t i = 0; i < count; i++) {
        row_f64(function, inputs[i]);
    }
}


// Prints a heading and the rows for a function of one float over count inputs.
static void section_f32(const char *name, float (*function)(float), const float *inputs, uint32_t count)
{
    puts(name);
    for (uint32_t i = 0; i < count; i++) {
        row_f32(function, inputs[i]);
    }
}


int main(void)
{
    section_f64("asin_f64", rc_asin_f64, arcsine_inputs, RC_COUNT(arcsine_inputs));
    section_f64("acos_f64", rc_acos_f64, arcsine_inputs, RC_COUNT(arcsine_inputs));
    section_f64("atan_f64", rc_atan_f64, atan_inputs, RC_COUNT(atan_inputs));
    puts("atan2_f64");
    for (uint32_t i = 0; i < RC_COUNT(atan2_inputs); i++) {
        row2_f64(atan2_inputs[i][0], atan2_inputs[i][1]);
    }

    section_f64("cbrt_f64", rc_cbrt_f64, cbrt_inputs, RC_COUNT(cbrt_inputs));
    section_f32("asin_f32", rc_asin_f32, arcsine_inputs_f32, RC_COUNT(arcsine_inputs_f32));
    section_f32("acos_f32", rc_acos_f32, arcsine_inputs_f32, RC_COUNT(arcsine_inputs_f32));
    section_f32("atan_f32", rc_atan_f32, atan_inputs_f32, RC_COUNT(atan_inputs_f32));
    puts("atan2_f32");
    for (uint32_t i = 0; i < RC_COUNT(atan2_inputs_f32); i++) {
        row2_f32(atan2_inputs_f32[i][0], atan2_inputs_f32[i][1]);
    }

    section_f32("cbrt_f32", rc_cbrt_f32, cbrt_inputs_f32, RC_COUNT(cbrt_inputs_f32));
    return 0;
}
