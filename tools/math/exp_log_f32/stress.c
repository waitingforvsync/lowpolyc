// stress.c
//
// A finer accuracy check than richc_math_accuracy, against glibc's long double functions: the
// worst error to 6 places and the count of results that differ from the correctly rounded float,
// over all 2^32 inputs of exp, log and log2, and for pow over seven telling distributions of
// pairs (any bits, ordinary values, x near 1 with y large, results near overflow and underflow,
// negative x with integer y, small integers, and subnormal x). Runs on 16 threads.
//
//   out/stress [pow-samples-per-distribution [exp|log|log2|pow]]
//   (or: ./run.sh stress [samples [function]]; 100 million pow samples by default)

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "richc/math/math.h"

enum {
    THREADS = 16,
    POW_DISTRIBUTIONS = 7
};

// A function under test and its reference; pow has neither, and is sampled instead.
typedef struct stress_function {
    const char *name;
    float (*ours)(float);
    long double (*reference)(long double);
} stress_function;

// One thread's share of the inputs, and the worst it found.
typedef struct stress_job {
    const stress_function *function;
    uint64_t first;
    uint64_t count;
    uint64_t seed;
    int distribution;
    double worst;
    float worst_x;
    float worst_y;
    uint64_t wrong;
} stress_job;

static const stress_function stress_functions[] = {
    {.name = "exp", .ours = rc_exp_f32, .reference = expl},
    {.name = "log", .ours = rc_log_f32, .reference = logl},
    {.name = "log2", .ours = rc_log2_f32, .reference = log2l},
    {.name = "pow", .ours = NULL, .reference = NULL},
};


// splitmix64.
static uint64_t next_random(uint64_t *state)
{
    uint64_t z = (*state += 0x9e3779b97f4a7c15ull);

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}


// Uniform in [0, 1).
static double unit(uint64_t *state)
{
    return (double)(next_random(state) >> 11) * 0x1p-53;
}


// |ours - ref| in ulps of ref, as a float: 0 for matching NaNs or infinities, infinite for any
// other NaN or infinity.
static double ulps(float ours, long double ref)
{
    float rounded = (float)ref;

    if (isnan(ref) || isnan(ours)) {
        return isnan(ref) && isnan(ours) ? 0 : INFINITY;
    }

    if (isinf(rounded) || isinf(ours)) {
        return rounded == ours ? 0 : INFINITY;
    }

    int exponent = 0;

    frexpl(ref, &exponent);
    int unit_exponent = (exponent - 1 < -126 ? -126 : exponent - 1) - 23;

    return (double)(fabsl((long double)ours - ref) / ldexpl(1.0L, unit_exponent));
}


// Whether ours is the correctly rounded float (both NaN counts).
static bool is_correctly_rounded(float ours, long double ref)
{
    float rounded = (float)ref;

    if (rounded == ours) {
        return true;
    }

    return rounded != rounded && ours != ours;
}


static void note(stress_job *job, float ours, long double ref, float x, float y)
{
    double error = ulps(ours, ref);

    if (!is_correctly_rounded(ours, ref)) {
        job->wrong++;
    }

    if (error > job->worst) {
        job->worst = error;
        job->worst_x = x;
        job->worst_y = y;
    }
}


// x near 1 with y such that y log2 x is in [-150, 150).
static void sample_near_one(uint64_t *s, float *x, float *y)
{
    *x = rc_bitcast_u32(0x3f800000u + (uint32_t)(next_random(s) % 4096) - 2048u);
    double l = log2((double)*x);

    *y = l == 0 ? 3 : (float)((unit(s) * 300 - 150) / l);
}


// Any positive x, with y such that the result is near overflow, or underflow and the subnormals.
static void sample_extreme_result(uint64_t *s, float *x, float *y)
{
    *x = rc_bitcast_u32((uint32_t)(next_random(s) % 0x7f800000u));
    double l = log2((double)*x);

    if (l == 0) {
        *y = 5;
        return;
    }

    double t = (next_random(s) & 1) ? 120 + unit(s) * 10 : -155 + unit(s) * 32;

    *y = (float)(t / l);
}


// A pair for pow from the job's distribution.
static void sample_pow(stress_job *job, float *x, float *y)
{
    uint64_t *s = &job->seed;

    switch (job->distribution) {
    case 0:
        *x = rc_bitcast_u32((uint32_t)next_random(s));
        *y = rc_bitcast_u32((uint32_t)next_random(s));
        break;
    case 1:
        *x = (float)(unit(s) * 100);
        *y = (float)(unit(s) * 200 - 100);
        break;
    case 2:
        sample_near_one(s, x, y);
        break;
    case 3:
        sample_extreme_result(s, x, y);
        break;
    case 4:
        *x = -(float)(unit(s) * 4);
        *y = (float)((int32_t)(next_random(s) % 400) - 200);
        break;
    case 5:
        *x = (float)(int)(next_random(s) % 64);
        *y = (float)((int)(next_random(s) % 40) - 20);
        if (next_random(s) & 1) {
            *x = -*x;
        }
        break;
    default:
        *x = rc_bitcast_u32((uint32_t)(next_random(s) % 0x800000u));
        *y = (float)(unit(s) * 2 - 1);
        break;
    }
}


static void *run_job(void *arg)
{
    stress_job *job = arg;
    const stress_function *function = job->function;

    for (uint64_t i = job->first; i < job->first + job->count; i++) {
        float x = rc_bitcast_u32((uint32_t)i);
        float y = 0;

        if (function->ours != NULL) {
            note(job, function->ours(x), function->reference(x), x, 0);
            continue;
        }

        sample_pow(job, &x, &y);
        note(job, rc_pow_f32(x, y), powl(x, y), x, y);
    }

    return NULL;
}


// Runs total inputs (the first total bit patterns, or pow samples) over the threads and prints
// the worst.
static void measure(const stress_function *function, uint64_t total, int distribution)
{
    stress_job jobs[THREADS] = {0};
    pthread_t threads[THREADS] = {0};
    stress_job worst = {0};
    uint64_t wrong = 0;

    for (int i = 0; i < THREADS; i++) {
        uint64_t first = total / THREADS * (uint64_t)i;
        uint64_t end = i == THREADS - 1 ? total : total / THREADS * (uint64_t)(i + 1);

        jobs[i] = (stress_job) {
            .function = function,
            .first = first,
            .count = end - first,
            .seed = 1234567ull * (uint64_t)(i + 1) + (uint64_t)distribution * 7919,
            .distribution = distribution
        };
        pthread_create(&threads[i], NULL, run_job, &jobs[i]);
    }

    for (int i = 0; i < THREADS; i++) {
        pthread_join(threads[i], NULL);
        wrong += jobs[i].wrong;
        if (jobs[i].worst > worst.worst) {
            worst = jobs[i];
        }
    }

    printf("%-6s dist %d  %12llu inputs  worst %.6f ulp at (%a, %a)  not correctly rounded %llu\n", function->name, distribution,
        (unsigned long long)total, worst.worst, (double)worst.worst_x, (double)worst.worst_y, (unsigned long long)wrong);
}


int main(int argc, char **argv)
{
    uint64_t pow_samples = argc > 1 ? strtoull(argv[1], NULL, 10) : 100000000ull;
    const char *only = argc > 2 ? argv[2] : "";

    for (uint32_t i = 0; i < RC_COUNT(stress_functions); i++) {
        const stress_function *function = &stress_functions[i];

        if (*only && strcmp(only, function->name) != 0) {
            continue;
        }

        if (function->ours != NULL) {
            measure(function, 1ull << 32, 0);
            continue;
        }

        for (int d = 0; d < POW_DISTRIBUTIONS; d++) {
            measure(function, pow_samples, d);
        }
    }

    return 0;
}
