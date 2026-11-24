// stress.c
//
// Targeted accuracy runs of rc_exp_f64, rc_log_f64, rc_log2_f64 and rc_pow_f64 against
// libquadmath. Each test draws its arguments from one telling region (see stress_tests below:
// near overflow and the subnormals for exp, near 1 for log, a large power of x near 1 for pow, and
// so on), and prints the worst error in ulps and where it was.
//
//   out/stress TEST SAMPLES [THREADS]    (THREADS is 4 by default)
//   (or: ./run.sh stress [samples], which runs the targeted set)

#include <math.h>
#include <pthread.h>
#include <quadmath.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "richc/math/math.h"

enum {
    MAX_THREADS = 64
};

typedef void (*stress_sampler)(uint64_t *state, double *x, double *y);

// A test: where its arguments come from, and the function under test and its reference (both
// NULL for pow).
typedef struct stress_test {
    const char *name;
    stress_sampler sample;
    double (*ours)(double);
    __float128 (*reference)(__float128);
} stress_test;

// One thread's share of the samples, and the worst it found.
typedef struct stress_job {
    const stress_test *test;
    uint64_t samples;
    uint64_t seed;
    double worst;
    double worst_x;
    double worst_y;
} stress_job;


// splitmix64.
static uint64_t rnd(uint64_t *s)
{
    uint64_t z = (*s += 0x9e3779b97f4a7c15ull);

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}


// Uniform in [0, 1).
static double unit(uint64_t *s)
{
    return (double)(rnd(s) >> 11) * 0x1p-53;
}


static double bits(uint64_t b)
{
    return rc_bitcast_u64(b);
}


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


// exp over its whole finite range.
static void sample_exp(uint64_t *s, double *x, double *y)
{
    *y = 0;
    *x = -745.2 + unit(s) * (709.8 + 745.2);
}


// exp near overflow.
static void sample_exp_hi(uint64_t *s, double *x, double *y)
{
    *y = 0;
    *x = 700 + unit(s) * 9.79;
}


// exp with subnormal results.
static void sample_exp_sub(uint64_t *s, double *x, double *y)
{
    *y = 0;
    *x = -745.2 + unit(s) * (745.2 - 700);
}


// exp of small arguments, in every binade down to 2^-60.
static void sample_exp_small(uint64_t *s, double *x, double *y)
{
    *y = 0;
    *x = (unit(s) * 2 - 1) * ldexp(1, -(int)(rnd(s) % 60));
}


static void sample_any_bits(uint64_t *s, double *x, double *y)
{
    *y = 0;
    *x = bits(rnd(s));
}


// log and log2 within 2^-7 of 1, in every binade down to 2^-46.
static void sample_log_near1(uint64_t *s, double *x, double *y)
{
    *y = 0;
    *x = 1 + (unit(s) * 2 - 1) * ldexp(1, -7 - (int)(rnd(s) % 40));
}


// log and log2 of any positive bits.
static void sample_log_bits(uint64_t *s, double *x, double *y)
{
    *y = 0;
    *x = bits(rnd(s) >> 1);
}


// log and log2 of x in [1/4, 1/2) and [2, 4), just outside log2's careful range.
static void sample_log_far(uint64_t *s, double *x, double *y)
{
    *y = 0;
    *x = bits((rnd(s) & 0x800fffffffffffffull) | 0x3fd0000000000000ull);
    if (*x < 0) {
        *x = -*x * 8;
    }
}


// log and log2 of subnormals.
static void sample_log_sub(uint64_t *s, double *x, double *y)
{
    *y = 0;
    *x = bits(rnd(s) >> 12);
}


static void sample_pow(uint64_t *s, double *x, double *y)
{
    *x = unit(s) * 100;
    *y = unit(s) * 200 - 100;
}


// x near 1, y so that |y log x| is up to ~745.
static void sample_pow_near1(uint64_t *s, double *x, double *y)
{
    double d = (unit(s) * 2 - 1) * ldexp(1, -7 - (int)(rnd(s) % 45));

    *x = 1 + d;
    double l = log(*x);

    if (l == 0) {
        l = 1e-300;
    }

    *y = (unit(s) * 2 - 1) * 745 / fabs(l);
}


// Any x above 1 (or its reciprocal), y so that the result is near overflow or underflow.
static void sample_pow_hi(uint64_t *s, double *x, double *y)
{
    *x = bits(0x3ff0000000000000ull + (rnd(s) >> 1) % 0x40f0000000000000ull);
    if (*x <= 1) {
        *x = 1.5;
    }

    double t = 700 + unit(s) * 9.79;

    if (rnd(s) & 1) {
        t = -708 - unit(s) * 37.2;
    }

    *y = t / log(*x);
    if (rnd(s) & 1) {
        *x = 1 / *x;
        *y = -*y;
    }
}


// |log x| up to 2^-7, so r runs over the whole table interval, and |y log x| from 600 to 745.
static void sample_pow_edge(uint64_t *s, double *x, double *y)
{
    *x = 1 + (unit(s) * 2 - 1) * 0x1p-7;
    double l = log(*x);

    if (l == 0) {
        l = 1e-300;
    }

    double t = 600 + unit(s) * 145;

    if (rnd(s) & 1) {
        t = 600 + unit(s) * 109.78;
    }
    else {
        t = -t;
    }

    *y = t / l;
}


// x in [0.5, 2), y over the whole range of results.
static void sample_pow_mid(uint64_t *s, double *x, double *y)
{
    *x = 0.5 + unit(s) * 1.5;
    double l = log(*x);

    if (l == 0) {
        l = 1e-300;
    }

    *y = (-745 + unit(s) * (745 + 709.78)) / l;
}


static void sample_pow_bits(uint64_t *s, double *x, double *y)
{
    *x = bits(rnd(s));
    *y = bits(rnd(s));
}


// Subnormal x.
static void sample_pow_xsub(uint64_t *s, double *x, double *y)
{
    *x = bits(rnd(s) >> 12);
    *y = unit(s) * 2 - 0.5;
}


// Negative x, integer y.
static void sample_pow_neg(uint64_t *s, double *x, double *y)
{
    *x = -unit(s) * 30;
    *y = (double)(int)(unit(s) * 400 - 200);
}


// Small integers, whose powers are often exact.
static void sample_pow_int(uint64_t *s, double *x, double *y)
{
    *x = (double)(rnd(s) % 1000);
    *y = (double)(rnd(s) % 12);
}


static const stress_test stress_tests[] = {
    {.name = "exp", .sample = sample_exp, .ours = rc_exp_f64, .reference = expq},
    {.name = "exp_hi", .sample = sample_exp_hi, .ours = rc_exp_f64, .reference = expq},
    {.name = "exp_sub", .sample = sample_exp_sub, .ours = rc_exp_f64, .reference = expq},
    {.name = "exp_small", .sample = sample_exp_small, .ours = rc_exp_f64, .reference = expq},
    {.name = "exp_bits", .sample = sample_any_bits, .ours = rc_exp_f64, .reference = expq},
    {.name = "log_near1", .sample = sample_log_near1, .ours = rc_log_f64, .reference = logq},
    {.name = "log_bits", .sample = sample_log_bits, .ours = rc_log_f64, .reference = logq},
    {.name = "log_sub", .sample = sample_log_sub, .ours = rc_log_f64, .reference = logq},
    {.name = "log2_near1", .sample = sample_log_near1, .ours = rc_log2_f64, .reference = log2q},
    {.name = "log2_bits", .sample = sample_log_bits, .ours = rc_log2_f64, .reference = log2q},
    {.name = "log2_far", .sample = sample_log_far, .ours = rc_log2_f64, .reference = log2q},
    {.name = "log2_sub", .sample = sample_log_sub, .ours = rc_log2_f64, .reference = log2q},
    {.name = "pow", .sample = sample_pow},
    {.name = "pow_near1", .sample = sample_pow_near1},
    {.name = "pow_hi", .sample = sample_pow_hi},
    {.name = "pow_edge", .sample = sample_pow_edge},
    {.name = "pow_mid", .sample = sample_pow_mid},
    {.name = "pow_bits", .sample = sample_pow_bits},
    {.name = "pow_xsub", .sample = sample_pow_xsub},
    {.name = "pow_neg", .sample = sample_pow_neg},
    {.name = "pow_int", .sample = sample_pow_int},
};


static void *run_job(void *arg)
{
    stress_job *job = arg;
    const stress_test *test = job->test;
    uint64_t s = job->seed;

    for (uint64_t i = 0; i < job->samples; i++) {
        double x = 0;
        double y = 0;

        test->sample(&s, &x, &y);

        double ours = test->ours != NULL ? test->ours(x) : rc_pow_f64(x, y);
        __float128 ref = test->ours != NULL ? test->reference(x) : powq(x, y);
        double error = ulps(ours, ref);

        if (error > job->worst) {
            job->worst = error;
            job->worst_x = x;
            job->worst_y = y;
        }
    }

    return NULL;
}


// The test called name, or NULL.
static const stress_test *test_named(const char *name)
{
    for (uint32_t i = 0; i < RC_COUNT(stress_tests); i++) {
        if (strcmp(stress_tests[i].name, name) == 0) {
            return &stress_tests[i];
        }
    }

    return NULL;
}


int main(int argc, char **argv)
{
    const stress_test *test = argc >= 3 ? test_named(argv[1]) : NULL;

    if (test == NULL) {
        fprintf(stderr, "usage: stress TEST SAMPLES [THREADS]; tests:");
        for (uint32_t i = 0; i < RC_COUNT(stress_tests); i++) {
            fprintf(stderr, " %s", stress_tests[i].name);
        }
        fprintf(stderr, "\n");
        return 2;
    }

    uint64_t n = strtoull(argv[2], NULL, 10);
    int thread_count = argc > 3 ? atoi(argv[3]) : 4;
    pthread_t threads[MAX_THREADS] = {0};
    stress_job jobs[MAX_THREADS] = {0};
    double worst = 0;
    double worst_x = 0;
    double worst_y = 0;

    if (thread_count < 1 || thread_count > MAX_THREADS) {
        fprintf(stderr, "stress: THREADS must be 1 to %d\n", MAX_THREADS);
        return 2;
    }

    for (int i = 0; i < thread_count; i++) {
        jobs[i] = (stress_job) {
            .test = test,
            .samples = n / (uint64_t)thread_count,
            .seed = 0x1234567ull * (uint64_t)(i + 1) + n
        };
        pthread_create(&threads[i], NULL, run_job, &jobs[i]);
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
        if (jobs[i].worst > worst) {
            worst = jobs[i].worst;
            worst_x = jobs[i].worst_x;
            worst_y = jobs[i].worst_y;
        }
    }

    printf("%-12s %12llu  worst %.5f ulp at (%a, %a)\n", test->name, (unsigned long long)n, worst, worst_x, worst_y);
    return 0;
}
