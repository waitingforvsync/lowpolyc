// bench.c
//
// Nanoseconds per call of rc_exp_f64, rc_log_f64, rc_log2_f64 and rc_pow_f64 against glibc's, on
// the same ordinary arguments: throughput over an array, the best of 200 runs of 40 passes over
// 4096 arguments, and ours / glibc.
//
//   out/bench    (or: ./run.sh bench)

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "richc/math/math.h"

enum {
    N = 4096,
    REPS = 40,
    TRIALS = 200
};

static double xs[N];
static double ys[N];
static volatile double sink;

// Times call over the arrays, prints the best time per call and leaves it in result.
#define BENCH(name, call) \
    do { \
        double best = 1e9; \
        for (int k = 0; k < TRIALS; k++) { \
            double t0 = now(); \
            double acc = 0; \
            for (int r = 0; r < REPS; r++) { \
                for (int i = 0; i < N; i++) { \
                    acc += call; \
                } \
            } \
            double t = (now() - t0) / ((double)REPS * N) * 1e9; \
            sink = acc; \
            if (t < best) { \
                best = t; \
            } \
        } \
        printf("%-14s %6.2f ns\n", name, best); \
        result = best; \
    } while (0)

// Times ours and glibc's and prints their ratio.
#define COMPARE(ours_name, ours_call, glibc_name, glibc_call) \
    do { \
        BENCH(ours_name, ours_call); \
        double ours = result; \
        BENCH(glibc_name, glibc_call); \
        printf("  ratio %.2f\n", ours / result); \
    } while (0)


static double now(void)
{
    struct timespec t = {0};

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}


static double random_unit(void)
{
    return rand() / (double)RAND_MAX;
}


int main(void)
{
    double result = 0;

    srand(1);
    for (int i = 0; i < N; i++) {
        xs[i] = random_unit() * 20 - 10;
    }

    COMPARE("rc_exp", rc_exp_f64(xs[i]), "glibc exp", exp(xs[i]));

    for (int i = 0; i < N; i++) {
        xs[i] = random_unit() * 1000 + 1e-3;
    }

    COMPARE("rc_log", rc_log_f64(xs[i]), "glibc log", log(xs[i]));
    COMPARE("rc_log2", rc_log2_f64(xs[i]), "glibc log2", log2(xs[i]));

    for (int i = 0; i < N; i++) {
        xs[i] = random_unit() * 100 + 1e-3;
        ys[i] = random_unit() * 20 - 10;
    }

    COMPARE("rc_pow", rc_pow_f64(xs[i], ys[i]), "glibc pow", pow(xs[i], ys[i]));
    return 0;
}
