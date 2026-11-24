// golden.c
//
// Prints the golden rows of test_math_exp_log_f64.c (input bits, output bits) under a line naming
// each function, with each output's error in ulps against libquadmath in its comment, so that a
// bad golden value can't slip in.
//
//   out/golden    (or: ./run.sh golden, which also checks the rows against the tests)

#include <math.h>
#include <quadmath.h>
#include <stdint.h>
#include <stdio.h>

#include "richc/math/math.h"

// Inputs for exp: tiny, around the table's steps, ordinary, either side of the fast path's edge,
// near overflow, and down through the subnormals.
static const double exp_inputs[] = {
    0x1p-1074, 1e-300, -1e-20, 0x1p-30, 1e-5, -0.001, 0.0027, 0.5, -0.5, 1.0, -1.0, 0.6931471805599453, 2.5, -3.7, 10, -10, 37.5,
    100, -100, 300.25, -500.75, 699.75, -699.75, 700, -700, 704.5, -704.5, 708.5, 709.0, 709.78, 709.782712893384, -708.0, -708.5,
    -720, -740, -745.0, -745.1
};

// Inputs for log and log2: subnormal, either side of 1, at the edges of the tables' intervals and
// of log2's careful range [1/2, 2), ordinary and huge.
static const double log_inputs[] = {
    0x1p-1074, 0x0.8p-1022, 0x1p-1022, 1e-300, 1e-10, 0.1, 0x1.fffffffffffffp-2, 0.5, 0.70710678118654757, 0.9, 0.99, 0x1.fffffffffffffp-1,
    1 - 0x1p-20, 1 + 0x1p-52, 1 + 0x1p-20, 1.00390625, 1.0039, 1.0078125, 1.0077, 1.5, 1.9921875, 1.9990234375, 0x1.fffffffffffffp+0, 2,
    2.718281828459045, 3, 10, 1000, 123456.789, 1e100, 1e300, 0x1.fffffffffffffp+1023
};

// Pairs for pow: ordinary, near 1 with a large power, near overflow, subnormal results, a negative
// x (its result also overflowing or subnormal), a subnormal x, negative too, and a subnormal y.
static const double pow_inputs[][2] = {
    {2, 0.5}, {2, -0.5}, {10, 0.3}, {0.5, 3.7}, {1.5, 100.25}, {3, 0.333}, {100, -1.5}, {1 + 0x1p-40, 0x1p40}, {1 - 0x1p-30, -0x1p31},
    {0x1.fffffffffffffp-1, 1e18}, {1.0000001, -7e9}, {7, 300}, {2, 1023.99}, {10, -307.5}, {10, -310.3}, {10, -323.5}, {0.5, 1074.5},
    {-3, 5}, {-1.1, 101}, {-0.9, -201}, {-2, 1025}, {-1.5, 2000}, {-0.5, 1073}, {-0.5, 1074}, {0x0.0000000000123p-1022, 0.25},
    {1e-310, -0.5}, {1e300, 1.02}, {1e-300, 1.02}, {3, 1e-310}, {0x1.8p-3, -350.5}, {12345.678, 0.0001}, {0.001, 50.5}, {1.25, -3000},
    {99.9, 99.9}, {-5e-310, 1}, {-0x1.8p-1070, 3}, {1e-310, -1}
};


// |ours - ref| in ulps of ref: 0 for matching NaNs or infinities, and infinite for any other
// NaN or infinity.
static double ulps(double ours, __float128 ref)
{
    double rounded = (double)ref;

    if (isnanq(ref) || isnan(ours)) {
        return (isnanq(ref) && isnan(ours)) ? 0 : INFINITY;
    }

    if (isinf(rounded) || isinf(ours)) {
        return rounded == ours ? 0 : INFINITY;
    }

    int exponent = 0;

    frexpq(ref, &exponent);
    int unit_exponent = (exponent - 1 < -1022 ? -1022 : exponent - 1) - 52;

    return (double)(fabsq((__float128)ours - ref) / ldexpq(1.0Q, unit_exponent));
}


// Prints the rows for a function of one argument.
static void section(const char *name, double (*function)(double), __float128 (*reference)(__float128), const double *inputs,
    uint32_t count)
{
    printf("%s:\n", name);
    for (uint32_t i = 0; i < count; i++) {
        double x = inputs[i];
        double r = function(x);

        printf("    {0x%016llxull, 0x%016llxull},  // %.9g -> %.9g (%.3f ulp)\n", (unsigned long long)rc_bitcast_f64(x),
            (unsigned long long)rc_bitcast_f64(r), x, r, ulps(r, reference(x)));
    }
}


static void section_pow(void)
{
    printf("pow:\n");
    for (uint32_t i = 0; i < RC_COUNT(pow_inputs); i++) {
        double x = pow_inputs[i][0];
        double y = pow_inputs[i][1];
        double r = rc_pow_f64(x, y);

        printf("    {0x%016llxull, 0x%016llxull, 0x%016llxull},  // %.6g ^ %.6g (%.3f ulp)\n", (unsigned long long)rc_bitcast_f64(x),
            (unsigned long long)rc_bitcast_f64(y), (unsigned long long)rc_bitcast_f64(r), x, y, ulps(r, powq(x, y)));
    }
}


int main(void)
{
    section("exp", rc_exp_f64, expq, exp_inputs, RC_COUNT(exp_inputs));
    section("log", rc_log_f64, logq, log_inputs, RC_COUNT(log_inputs));
    section("log2", rc_log2_f64, log2q, log_inputs, RC_COUNT(log_inputs));
    section_pow();
    return 0;
}
