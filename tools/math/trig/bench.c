// bench.c
//
// Nanoseconds per call of rc_sin, rc_cos and rc_tan in both widths against glibc's, on the same
// random arguments, for |x| below a series of bounds from 0.7 to 1e30. Each figure is the best of
// 60 passes over 65536 arguments, and each pair ends with ours / glibc.
//
//   out/bench    (or: ./run.sh bench)

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "richc/math/math.h"

enum {
    N = 65536,
    REPEATS = 1,
    TRIALS = 60
};

static double xs[N];
static float xf[N];

// Times fn over xs (or xf, as T says), prints the best time per call and records it in times.
#define BENCH(T, args, name, fn) \
    do { \
        T sum = 0; \
        double best = 1e30; \
        for (int trial = 0; trial < TRIALS; trial++) { \
            double t0 = now(); \
            for (int r = 0; r < REPEATS; r++) { \
                for (int i = 0; i < N; i++) { \
                    sum += fn(args[i]); \
                } \
            } \
            double d = (now() - t0) / ((double)N * REPEATS); \
            if (d < best) { \
                best = d; \
            } \
        } \
        printf("  %-12s %6.2f ns  (%g)\n", name, best, (double)sum); \
        times[count++] = best; \
    } while (0)


static double now(void)
{
    struct timespec t = {0};

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}


static double glibc_sin(double x)
{
    return sin(x);
}


static double glibc_cos(double x)
{
    return cos(x);
}


static double glibc_tan(double x)
{
    return tan(x);
}


static float glibc_sinf(float x)
{
    return sinf(x);
}


static float glibc_cosf(float x)
{
    return cosf(x);
}


static float glibc_tanf(float x)
{
    return tanf(x);
}


// Fills xs and xf with random arguments in (-range, range), carrying on from state.
static void fill(double range, uint64_t *state)
{
    for (int i = 0; i < N; i++) {
        *state ^= *state << 13;
        *state ^= *state >> 7;
        *state ^= *state << 17;
        xs[i] = ((double)(*state >> 11) * 0x1p-53 * 2 - 1) * range;
        xf[i] = (float)xs[i];
    }
}


int main(void)
{
    static const double ranges[] = {0.7, 0.7, 3.2, 100.0, 1e5, 1e9, 1e30};
    uint64_t state = 88172645463325252ull;

    for (uint32_t ri = 0; ri < RC_COUNT(ranges); ri++) {
        double times[16] = {0};
        int count = 0;

        fill(ranges[ri], &state);
        printf("|x| < %g\n", ranges[ri]);
        BENCH(double, xs, "rc_sin_f64", rc_sin_f64);
        BENCH(double, xs, "sin", glibc_sin);
        BENCH(double, xs, "rc_cos_f64", rc_cos_f64);
        BENCH(double, xs, "cos", glibc_cos);
        BENCH(double, xs, "rc_tan_f64", rc_tan_f64);
        BENCH(double, xs, "tan", glibc_tan);
        BENCH(float, xf, "rc_sin_f32", rc_sin_f32);
        BENCH(float, xf, "sinf", glibc_sinf);
        BENCH(float, xf, "rc_cos_f32", rc_cos_f32);
        BENCH(float, xf, "cosf", glibc_cosf);
        BENCH(float, xf, "rc_tan_f32", rc_tan_f32);
        BENCH(float, xf, "tanf", glibc_tanf);
        printf("  ratios: sin %.2f cos %.2f tan %.2f sinf %.2f cosf %.2f tanf %.2f\n", times[0] / times[1], times[2] / times[3],
            times[4] / times[5], times[6] / times[7], times[8] / times[9], times[10] / times[11]);
    }

    return 0;
}
