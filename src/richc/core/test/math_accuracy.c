// math_accuracy.c
//
// Measures how far richc's transcendentals stray from the true values, in ulps, and fails if any
// goes past its stated limit. The f32 functions of one argument are tried on every one of the 2^32
// inputs, against glibc's double function; the f64 ones, and those of two arguments, on random
// samples and the special values, against glibc's long double functions. That takes minutes, so
// it is a tool to run after changing a function, not one of the tests that run after every build:
//
//   richc_math_accuracy [name-prefix] [samples]
//
// Native and POSIX only: it leans on libm for the references and pthreads to use every core.

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "richc/math/math.h"

enum {
    THREAD_COUNT = 16,
    DEFAULT_SAMPLES = 10000000
};

// The worst error a run found, and the input that gave it.
typedef struct accuracy_worst {
    double ulps;
    double x;
    double y;
} accuracy_worst;

typedef float (*accuracy_f32_fn)(float);
typedef double (*accuracy_f64_fn)(double);
typedef long double (*accuracy_f80_fn)(long double);
typedef float (*accuracy_f32_fn2)(float, float);
typedef double (*accuracy_f64_fn2)(double, double);
typedef long double (*accuracy_f80_fn2)(long double, long double);

// A function to measure: ours, the reference, the largest error we promise, and the range where
// random samples are most telling.
typedef struct accuracy_case {
    const char *name;
    accuracy_f32_fn ours_f32;
    accuracy_f64_fn reference_f32;
    accuracy_f64_fn ours_f64;
    accuracy_f80_fn reference_f64;
    accuracy_f32_fn2 ours2_f32;
    accuracy_f64_fn2 reference2_f32;
    accuracy_f64_fn2 ours2_f64;
    accuracy_f80_fn2 reference2_f64;
    double limit_ulps;
    double low;
    double high;
} accuracy_case;

// One thread's share of a run.
typedef struct accuracy_job {
    const accuracy_case *test;
    uint64_t first;
    uint64_t count;
    uint64_t seed;
    accuracy_worst worst;
} accuracy_job;

// The limits are the worst errors measured, or the bound a function's banner derives where sampling
// could miss the worst, with a little room, so a change that makes a function worse shows up.
static const accuracy_case accuracy_cases[] = {
    {.name = "sin_f32", .ours_f32 = rc_sin_f32, .reference_f32 = sin, .limit_ulps = 0.502},
    {.name = "cos_f32", .ours_f32 = rc_cos_f32, .reference_f32 = cos, .limit_ulps = 0.502},
    {.name = "tan_f32", .ours_f32 = rc_tan_f32, .reference_f32 = tan, .limit_ulps = 0.501},
    {.name = "asin_f32", .ours_f32 = rc_asin_f32, .reference_f32 = asin, .limit_ulps = 0.505},
    {.name = "acos_f32", .ours_f32 = rc_acos_f32, .reference_f32 = acos, .limit_ulps = 0.505},
    {.name = "atan_f32", .ours_f32 = rc_atan_f32, .reference_f32 = atan, .limit_ulps = 0.51},
    {.name = "exp_f32", .ours_f32 = rc_exp_f32, .reference_f32 = exp, .limit_ulps = 0.502},
    {.name = "log_f32", .ours_f32 = rc_log_f32, .reference_f32 = log, .limit_ulps = 0.501},
    {.name = "log2_f32", .ours_f32 = rc_log2_f32, .reference_f32 = log2, .limit_ulps = 0.501},
    {.name = "cbrt_f32", .ours_f32 = rc_cbrt_f32, .reference_f32 = cbrt, .limit_ulps = 0.501},
    {.name = "atan2_f32", .ours2_f32 = rc_atan2_f32, .reference2_f32 = atan2, .limit_ulps = 0.52, .low = -1e3, .high = 1e3},
    {.name = "pow_f32", .ours2_f32 = rc_pow_f32, .reference2_f32 = pow, .limit_ulps = 0.52, .low = 0.0, .high = 100.0},
    {.name = "sin_f64", .ours_f64 = rc_sin_f64, .reference_f64 = sinl, .limit_ulps = 0.65, .low = -1e6, .high = 1e6},
    {.name = "cos_f64", .ours_f64 = rc_cos_f64, .reference_f64 = cosl, .limit_ulps = 0.65, .low = -1e6, .high = 1e6},
    {.name = "tan_f64", .ours_f64 = rc_tan_f64, .reference_f64 = tanl, .limit_ulps = 0.80, .low = -1e6, .high = 1e6},
    {.name = "asin_f64", .ours_f64 = rc_asin_f64, .reference_f64 = asinl, .limit_ulps = 0.80, .low = -1.0, .high = 1.0},
    {.name = "acos_f64", .ours_f64 = rc_acos_f64, .reference_f64 = acosl, .limit_ulps = 0.75, .low = -1.0, .high = 1.0},
    {.name = "atan_f64", .ours_f64 = rc_atan_f64, .reference_f64 = atanl, .limit_ulps = 0.75, .low = -1e3, .high = 1e3},
    {.name = "exp_f64", .ours_f64 = rc_exp_f64, .reference_f64 = expl, .limit_ulps = 0.51, .low = -750.0, .high = 750.0},
    {.name = "log_f64", .ours_f64 = rc_log_f64, .reference_f64 = logl, .limit_ulps = 0.505, .low = 0.0, .high = 1e10},
    {.name = "log2_f64", .ours_f64 = rc_log2_f64, .reference_f64 = log2l, .limit_ulps = 0.505, .low = 0.0, .high = 1e10},
    {.name = "cbrt_f64", .ours_f64 = rc_cbrt_f64, .reference_f64 = cbrtl, .limit_ulps = 0.501, .low = -1e10, .high = 1e10},
    {.name = "atan2_f64", .ours2_f64 = rc_atan2_f64, .reference2_f64 = atan2l, .limit_ulps = 0.70, .low = -1e3, .high = 1e3},
    {.name = "pow_f64", .ours2_f64 = rc_pow_f64, .reference2_f64 = powl, .limit_ulps = 0.515, .low = 0.0, .high = 100.0},
};

// The special values every sampled run tries, in every combination for two arguments.
static const double accuracy_specials[] = {
    0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 3.0, 10.0, 0.1, 1e-300, -1e-300, 1e300, -1e300, 4.9e-324, -4.9e-324,
    2.2250738585072014e-308, DBL_MAX, -DBL_MAX, (double)FLT_MAX, (double)FLT_MIN, 1.5707963267948966, 3.141592653589793, 6.283185307179586,
    -1.5707963267948966, 709.78, -745.1, 88.72, -103.9, 1e22, 0.7853981633974483, (double)INFINITY, -(double)INFINITY, (double)NAN
};


// The next of a splitmix64 sequence.
static uint64_t accuracy_random(uint64_t *state)
{
    uint64_t z = (*state += 0x9e3779b97f4a7c15ull);

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}


// A random double in [low, high).
static double accuracy_random_between(uint64_t *state, double low, double high)
{
    double unit = (double)(accuracy_random(state) >> 11) * 0x1p-53;

    return low + (high - low) * unit;
}


// A random double from its bits, NaNs and all.
static double accuracy_random_bits(uint64_t *state)
{
    uint64_t bits = accuracy_random(state);
    double x = 0.0;

    memcpy(&x, &bits, sizeof x);
    return x;
}


// A random float from its bits.
static float accuracy_random_bits_f32(uint64_t *state)
{
    uint32_t bits = (uint32_t)(accuracy_random(state) >> 32);
    float x = 0.0f;

    memcpy(&x, &bits, sizeof x);
    return x;
}


// The power of 2 of one ulp at a value that frexp gave exponent, for a type with the given smallest
// normal exponent and mantissa bits. The subnormals all share the smallest normal's ulp.
static int accuracy_ulp_exponent(int exponent, int min_exponent, int mantissa_bits)
{
    int normal_exponent = exponent - 1 < min_exponent ? min_exponent : exponent - 1;

    return normal_exponent - mantissa_bits;
}


// The error of ours in units of the last place of a float at the reference value. NaN must meet
// NaN, and a reference beyond the floats must have rounded to the same infinity.
static double accuracy_ulps_f32(float ours, double reference)
{
    float rounded = (float)reference;
    int exponent = 0;

    if (isnan(reference) || isnan(ours)) {
        return isnan(reference) && isnan(ours) ? 0.0 : (double)INFINITY;
    }

    if (isinf(rounded) || isinf(ours)) {
        return rounded == ours ? 0.0 : (double)INFINITY;
    }

    frexp(reference, &exponent);
    return fabs((double)ours - reference) / ldexp(1.0, accuracy_ulp_exponent(exponent, -126, 23));
}


// The error of ours in units of the last place of a double at the reference value.
static double accuracy_ulps_f64(double ours, long double reference)
{
    double rounded = (double)reference;
    int exponent = 0;

    if (isnan(reference) || isnan(ours)) {
        return isnan(reference) && isnan(ours) ? 0.0 : (double)INFINITY;
    }

    if (isinf(rounded) || isinf(ours)) {
        return rounded == ours ? 0.0 : (double)INFINITY;
    }

    frexpl(reference, &exponent);
    return (double)(fabsl((long double)ours - reference) / ldexpl(1.0L, accuracy_ulp_exponent(exponent, -1022, 52)));
}


// Keep the error at x and y if it is the worst so far.
static void accuracy_note(accuracy_worst *worst, double ulps, double x, double y)
{
    if (ulps > worst->ulps) {
        *worst = (accuracy_worst) {.ulps = ulps, .x = x, .y = y};
    }
}


// One argument, single precision: every input in the job's share of the 2^32.
static void accuracy_run_f32(accuracy_job *job)
{
    for (uint64_t i = job->first; i < job->first + job->count; i++) {
        uint32_t bits = (uint32_t)i;
        float x = 0.0f;

        memcpy(&x, &bits, sizeof x);
        accuracy_note(&job->worst, accuracy_ulps_f32(job->test->ours_f32(x), job->test->reference_f32((double)x)), (double)x, 0.0);
    }
}


// One argument, double precision: random bits, then random values in the telling range.
static void accuracy_run_f64(accuracy_job *job)
{
    uint64_t state = job->seed;

    for (uint64_t i = 0; i < job->count; i++) {
        double x = (i & 1) ? accuracy_random_bits(&state) : accuracy_random_between(&state, job->test->low, job->test->high);

        accuracy_note(&job->worst, accuracy_ulps_f64(job->test->ours_f64(x), job->test->reference_f64((long double)x)), x, 0.0);
    }
}


// Two arguments, single precision: random bits, then random values in the telling range.
static void accuracy_run2_f32(accuracy_job *job)
{
    uint64_t state = job->seed;

    for (uint64_t i = 0; i < job->count; i++) {
        bool random_bits = (i & 1) != 0;
        float x = random_bits ? accuracy_random_bits_f32(&state) : (float)accuracy_random_between(&state, job->test->low, job->test->high);
        float y = random_bits ? accuracy_random_bits_f32(&state) : (float)accuracy_random_between(&state, job->test->low, job->test->high);
        double reference = job->test->reference2_f32((double)x, (double)y);

        accuracy_note(&job->worst, accuracy_ulps_f32(job->test->ours2_f32(x, y), reference), (double)x, (double)y);
    }
}


// Two arguments, double precision: random bits, then random values in the telling range.
static void accuracy_run2_f64(accuracy_job *job)
{
    uint64_t state = job->seed;

    for (uint64_t i = 0; i < job->count; i++) {
        bool random_bits = (i & 1) != 0;
        double x = random_bits ? accuracy_random_bits(&state) : accuracy_random_between(&state, job->test->low, job->test->high);
        double y = random_bits ? accuracy_random_bits(&state) : accuracy_random_between(&state, job->test->low, job->test->high);
        long double reference = job->test->reference2_f64((long double)x, (long double)y);

        accuracy_note(&job->worst, accuracy_ulps_f64(job->test->ours2_f64(x, y), reference), x, y);
    }
}


// A thread's entry: whichever run its function needs.
static void *accuracy_thread(void *context)
{
    accuracy_job *job = context;

    if (job->test->ours_f32) {
        accuracy_run_f32(job);
    }
    else if (job->test->ours_f64) {
        accuracy_run_f64(job);
    }
    else if (job->test->ours2_f32) {
        accuracy_run2_f32(job);
    }
    else {
        accuracy_run2_f64(job);
    }

    return NULL;
}


// The special values, alone or in pairs, on this thread.
static accuracy_worst accuracy_specials_worst(const accuracy_case *test)
{
    accuracy_worst worst = {0};
    uint32_t count = RC_COUNT(accuracy_specials);

    for (uint32_t i = 0; i < count; i++) {
        double x = accuracy_specials[i];

        if (test->ours_f64) {
            accuracy_note(&worst, accuracy_ulps_f64(test->ours_f64(x), test->reference_f64((long double)x)), x, 0.0);
        }

        for (uint32_t j = 0; j < count; j++) {
            double y = accuracy_specials[j];

            if (test->ours2_f32) {
                float x_f32 = (float)x;
                float y_f32 = (float)y;
                double reference = test->reference2_f32((double)x_f32, (double)y_f32);

                accuracy_note(&worst, accuracy_ulps_f32(test->ours2_f32(x_f32, y_f32), reference), x, y);
            }

            if (test->ours2_f64) {
                accuracy_note(&worst, accuracy_ulps_f64(test->ours2_f64(x, y), test->reference2_f64((long double)x, (long double)y)), x, y);
            }
        }
    }

    return worst;
}


// Measure one function across every thread, print its line, and return whether it is within
// its limit.
static bool accuracy_measure(const accuracy_case *test, uint64_t samples)
{
    bool exhaustive = test->ours_f32 != NULL;
    uint64_t total = exhaustive ? (UINT64_C(1) << 32) : samples;
    accuracy_job jobs[THREAD_COUNT] = {0};
    pthread_t threads[THREAD_COUNT] = {0};
    accuracy_worst worst = accuracy_specials_worst(test);

    for (uint32_t i = 0; i < THREAD_COUNT; i++) {
        uint64_t first = total / THREAD_COUNT * i;
        uint64_t end = i + 1 == THREAD_COUNT ? total : total / THREAD_COUNT * (i + 1);

        jobs[i] = (accuracy_job) {.test = test, .first = first, .count = end - first, .seed = 0x5eed0000ull + i};
        pthread_create(&threads[i], NULL, accuracy_thread, &jobs[i]);
    }

    for (uint32_t i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
        accuracy_note(&worst, jobs[i].worst.ulps, jobs[i].worst.x, jobs[i].worst.y);
    }

    printf("%-10s %12" PRIu64 " inputs  worst %.3f ulp at (%a, %a)  limit %.3f  %s\n", test->name, total, worst.ulps, worst.x, worst.y,
        test->limit_ulps, worst.ulps <= test->limit_ulps ? "ok" : "FAIL");
    return worst.ulps <= test->limit_ulps;
}


int main(int argc, char **argv)
{
    const char *prefix = argc > 1 ? argv[1] : "";
    uint64_t samples = argc > 2 ? strtoull(argv[2], NULL, 10) : DEFAULT_SAMPLES;
    uint32_t count = RC_COUNT(accuracy_cases);
    int failures = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (strncmp(accuracy_cases[i].name, prefix, strlen(prefix)) != 0) {
            continue;
        }

        failures += accuracy_measure(&accuracy_cases[i], samples) ? 0 : 1;
    }

    return failures != 0;
}
