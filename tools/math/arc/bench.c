// bench.c
//
// Nanoseconds per call of the arc functions and cbrt in both widths against glibc's, on the same
// random arguments: the best of 7 runs of 500 passes over 4096 arguments, and ours / glibc.
//
//   out/bench [name-filter]    (or: ./run.sh bench [name-filter]; only the rows whose name holds
//                              the filter run, so "f64" or "atan2" pick a few)

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "richc/math/math.h"

enum {
    N = 4096,
    REPS = 500,
    TRIES = 7
};

static double xs[N];
static double ys[N];
static float xf[N];
static float yf[N];
static uint64_t random_state = 12345;

// Leaves the best time per call of the expression call, in ns, in result.
#define TIME(T, call) \
    do { \
        result = 1e9; \
        for (int k = 0; k < TRIES; k++) { \
            volatile T sink = 0; \
            T acc = 0; \
            double t0 = now(); \
            for (int r = 0; r < REPS; r++) { \
                for (int i = 0; i < N; i++) { \
                    acc += call; \
                } \
            } \
            double t1 = now(); \
            sink = acc; \
            (void)sink; \
            double v = (t1 - t0) / ((double)N * REPS) * 1e9; \
            result = v < result ? v : result; \
        } \
    } while (0)

// Prints the row timing ours and glibc's on arguments in [lo, hi), if its name holds only.
#define ROW(name, lo, hi, T, ours, theirs, call_args) \
    do { \
        if (strstr(name, only)) { \
            double result = 0; \
            fill(lo, hi); \
            TIME(T, ours call_args); \
            double a = result; \
            TIME(T, theirs call_args); \
            double b = result; \
            printf("%-10s ours %6.2f ns  glibc %6.2f ns  ratio %.2f\n", name, a, b, a / b); \
        } \
    } while (0)


static double now(void)
{
    struct timespec t = {0};

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}


// A 64-bit LCG, uniform in [lo, hi).
static double next_random(double lo, double hi)
{
    random_state = random_state * 6364136223846793005ull + 1442695040888963407ull;
    return lo + (hi - lo) * (double)(random_state >> 11) * 0x1p-53;
}


static void fill(double lo, double hi)
{
    for (int i = 0; i < N; i++) {
        xs[i] = next_random(lo, hi);
        ys[i] = next_random(lo, hi);
        xf[i] = (float)xs[i];
        yf[i] = (float)ys[i];
    }
}


int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : "";

    ROW("asin_f32", -1, 1, float, rc_asin_f32, asinf, (xf[i]));
    ROW("acos_f32", -1, 1, float, rc_acos_f32, acosf, (xf[i]));
    ROW("atan_f32", -10, 10, float, rc_atan_f32, atanf, (xf[i]));
    ROW("atan2_f32", -10, 10, float, rc_atan2_f32, atan2f, (yf[i], xf[i]));
    ROW("cbrt_f32", -1000, 1000, float, rc_cbrt_f32, cbrtf, (xf[i]));
    ROW("asin_f64", -1, 1, double, rc_asin_f64, asin, (xs[i]));
    ROW("acos_f64", -1, 1, double, rc_acos_f64, acos, (xs[i]));
    ROW("atan_f64", -10, 10, double, rc_atan_f64, atan, (xs[i]));
    ROW("atan2_f64", -10, 10, double, rc_atan2_f64, atan2, (ys[i], xs[i]));
    ROW("cbrt_f64", -1000, 1000, double, rc_cbrt_f64, cbrt, (xs[i]));
    return 0;
}
