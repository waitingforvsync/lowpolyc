// stress.c
//
// Targeted accuracy check of rc_sin_f64, rc_cos_f64 or rc_tan_f64 against libquadmath, on random
// doubles in [lo, hi) spread over 16 threads. Prints the worst error in ulps and where it was.
// richc_math_accuracy covers every function broadly; this digs into the hardest range, just
// below pi/4, where the kernels' rounding errors are largest relative to the result.
//
//   out/stress sin|cos|tan lo hi samples    (or: ./run.sh stress [samples])

#include <math.h>
#include <pthread.h>
#include <quadmath.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "richc/math/math.h"

enum {
    THREADS = 16
};

// A function under test and its libquadmath reference.
typedef struct stress_function {
    const char *name;
    double (*ours)(double);
    __float128 (*reference)(__float128);
} stress_function;

// One thread's share of the samples, and the worst it found.
typedef struct stress_job {
    const stress_function *function;
    double lo;
    double hi;
    uint64_t samples;
    uint64_t seed;
    double worst;
    double worst_at;
} stress_job;

static const stress_function stress_functions[] = {
    {.name = "sin", .ours = rc_sin_f64, .reference = sinq},
    {.name = "cos", .ours = rc_cos_f64, .reference = cosq},
    {.name = "tan", .ours = rc_tan_f64, .reference = tanq},
};


// splitmix64.
static uint64_t next_random(uint64_t *state)
{
    uint64_t z = (*state += 0x9e3779b97f4a7c15ull);

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}


// |ours - ref| in ulps of ref.
static double ulps(double ours, __float128 ref)
{
    int exponent = 0;

    frexpq(ref, &exponent);
    if (exponent - 1 < -1022) {
        exponent = -1021;
    }

    return (double)(fabsq((__float128)ours - ref) / ldexpq(1.0Q, exponent - 1 - 52));
}


static void *run_job(void *arg)
{
    stress_job *job = arg;

    for (uint64_t i = 0; i < job->samples; i++) {
        double u = (double)(next_random(&job->seed) >> 11) * 0x1p-53;
        double x = job->lo + (job->hi - job->lo) * u;
        double error = ulps(job->function->ours(x), job->function->reference(x));

        if (error > job->worst) {
            job->worst = error;
            job->worst_at = x;
        }
    }

    return NULL;
}


// The function called name, or NULL.
static const stress_function *function_named(const char *name)
{
    for (uint32_t i = 0; i < RC_COUNT(stress_functions); i++) {
        if (strcmp(stress_functions[i].name, name) == 0) {
            return &stress_functions[i];
        }
    }

    return NULL;
}


int main(int argc, char **argv)
{
    const stress_function *function = argc == 5 ? function_named(argv[1]) : NULL;

    if (function == NULL) {
        fprintf(stderr, "usage: stress sin|cos|tan lo hi samples\n");
        return 2;
    }

    double lo = strtod(argv[2], NULL);
    double hi = strtod(argv[3], NULL);
    uint64_t per_thread = strtoull(argv[4], NULL, 10) / THREADS;
    pthread_t threads[THREADS] = {0};
    stress_job jobs[THREADS] = {0};
    double worst = 0;
    double worst_at = 0;

    for (int i = 0; i < THREADS; i++) {
        jobs[i] = (stress_job) {
            .function = function,
            .lo = lo,
            .hi = hi,
            .samples = per_thread,
            .seed = 1234567 + (uint64_t)i * 7919 + (uint64_t)(lo * 1000)
        };
        pthread_create(&threads[i], NULL, run_job, &jobs[i]);
    }

    for (int i = 0; i < THREADS; i++) {
        pthread_join(threads[i], NULL);
        if (jobs[i].worst > worst) {
            worst = jobs[i].worst;
            worst_at = jobs[i].worst_at;
        }
    }

    printf("%s [%g, %g): worst %.4f ulp at %a\n", argv[1], lo, hi, worst, worst_at);
    return 0;
}
