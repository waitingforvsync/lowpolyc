// bench.c
//
// Times the f32 exp, log, log2 and pow against glibc's on the same ordinary arguments: throughput
// (independent calls, results stored) and latency (each call fed by the last), the best of 15
// rounds, alternating ours and glibc's so that load from elsewhere hits both alike. Pin it to one
// core for steadier figures.
//
//   taskset -c 3 out/bench    (or: ./run.sh bench)

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "richc/math/math.h"

enum {
    N = 4096,
    REPS = 1000,
    ROUNDS = 15
};

typedef float (*function1)(float);
typedef float (*function2)(float, float);

static float xs[N];
static float ys[N];
static float out[N];


static double now(void)
{
    struct timespec t = {0};

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}


static double fraction(double v)
{
    return v - (double)(int64_t)v;
}


static double min_f64(double a, double b)
{
    return a < b ? a : b;
}


// ns per call of g over xs, results stored. The volatile pointer keeps g from being inlined.
static double throughput1(function1 g)
{
    function1 volatile vf = g;
    function1 f = vf;
    double t0 = now();

    for (int r = 0; r < REPS; r++) {
        for (int i = 0; i < N; i++) {
            out[i] = f(xs[i]);
        }
    }

    return (now() - t0) / (N * (double)REPS) * 1e9;
}


// ns per call of g over xs, each argument made to depend on the last result.
static double latency1(function1 g)
{
    function1 volatile vf = g;
    function1 f = vf;
    float z = 0;
    double t0 = now();

    for (int r = 0; r < REPS; r++) {
        for (int i = 0; i < N; i++) {
            z = f(xs[i] + (z > 1e30f ? 1e-30f : 0.0f));
        }
    }

    out[0] = z;
    return (now() - t0) / (N * (double)REPS) * 1e9;
}


static double throughput2(function2 g)
{
    function2 volatile vf = g;
    function2 f = vf;
    double t0 = now();

    for (int r = 0; r < REPS; r++) {
        for (int i = 0; i < N; i++) {
            out[i] = f(xs[i], ys[i]);
        }
    }

    return (now() - t0) / (N * (double)REPS) * 1e9;
}


static double latency2(function2 g)
{
    function2 volatile vf = g;
    function2 f = vf;
    float z = 0;
    double t0 = now();

    for (int r = 0; r < REPS; r++) {
        for (int i = 0; i < N; i++) {
            z = f(xs[i] + (z > 1e30f ? 1e-30f : 0.0f), ys[i]);
        }
    }

    out[0] = z;
    return (now() - t0) / (N * (double)REPS) * 1e9;
}


static void print_row(const char *name, const double *throughput, const double *latency)
{
    printf("%-5s throughput %.2f vs %.2f ns (%.2fx)   latency %.2f vs %.2f ns (%.2fx)\n", name, throughput[0], throughput[1],
        throughput[0] / throughput[1], latency[0], latency[1], latency[0] / latency[1]);
}


// Compares ours and theirs on arguments spread over [lo, hi) by the golden ratio.
static void compare1(const char *name, function1 ours, function1 theirs, double lo, double hi)
{
    double throughput[2] = {1e9, 1e9};
    double latency[2] = {1e9, 1e9};

    for (int i = 0; i < N; i++) {
        xs[i] = (float)(lo + (hi - lo) * fraction(i * 0.6180339887));
    }

    for (int k = 0; k < ROUNDS; k++) {
        throughput[0] = min_f64(throughput[0], throughput1(ours));
        throughput[1] = min_f64(throughput[1], throughput1(theirs));
        latency[0] = min_f64(latency[0], latency1(ours));
        latency[1] = min_f64(latency[1], latency1(theirs));
    }

    print_row(name, throughput, latency);
}


// Compares ours and theirs on x in [0.01, 20.01) and y in [-20, 20).
static void compare2(const char *name, function2 ours, function2 theirs)
{
    double throughput[2] = {1e9, 1e9};
    double latency[2] = {1e9, 1e9};

    for (int i = 0; i < N; i++) {
        xs[i] = (float)(0.01 + 20 * fraction(i * 0.6180339887));
        ys[i] = (float)(-20 + 40 * fraction(i * 0.41421356));
    }

    for (int k = 0; k < ROUNDS; k++) {
        throughput[0] = min_f64(throughput[0], throughput2(ours));
        throughput[1] = min_f64(throughput[1], throughput2(theirs));
        latency[0] = min_f64(latency[0], latency2(ours));
        latency[1] = min_f64(latency[1], latency2(theirs));
    }

    print_row(name, throughput, latency);
}


int main(void)
{
    compare1("exp", rc_exp_f32, expf, -80.0, 80.0);
    compare1("log", rc_log_f32, logf, 1e-6, 1e6);
    compare1("log2", rc_log2_f32, log2f, 1e-6, 1e6);
    compare2("pow", rc_pow_f32, powf);
    return 0;
}
