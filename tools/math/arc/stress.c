// stress.c
//
// Targeted accuracy check of the risky regions of the f64 arc functions and cbrt, against glibc's
// long double functions: asin and acos either side of 1/2 and towards 1, tiny and log-spread
// arguments, every binade of atan's table, atan2 in every octant with small, table-sized and
// subnormal ratios at any magnitude, and cbrt log-spread. Prints the worst error in ulps of each
// region and where it was.
//
//   out/stress [samples-per-region]    (or: ./run.sh stress [samples]; 10 million by default)

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "richc/math/math.h"

// The worst error seen in a region, and its arguments.
typedef struct worst_case {
    double worst;
    double at_x;
    double at_y;
} worst_case;

static uint64_t random_state = 0x1234567;


// splitmix64.
static uint64_t next_random(void)
{
    uint64_t z = (random_state += 0x9e3779b97f4a7c15ull);

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}


// Uniform in [lo, hi).
static double uniform(double lo, double hi)
{
    return lo + (hi - lo) * (double)(next_random() >> 11) * 0x1p-53;
}


// Log-uniform magnitude in [2^lo, 2^hi).
static double log_uniform(double lo, double hi)
{
    return exp2(uniform(lo, hi));
}


// |ours - ref| in ulps of ref.
static double ulps(double ours, long double ref)
{
    int exponent = 0;

    frexpl(ref, &exponent);
    int unit_exponent = (exponent - 1 < -1022 ? -1022 : exponent - 1) - 52;

    return (double)(fabsl((long double)ours - ref) / ldexpl(1.0L, unit_exponent));
}


static void note(worst_case *w, double error, double x, double y)
{
    if (error > w->worst) {
        *w = (worst_case) {.worst = error, .at_x = x, .at_y = y};
    }
}


static void report(const char *name, worst_case w)
{
    printf("%-34s worst %.4f at (%a, %a)\n", name, w.worst, w.at_x, w.at_y);
}


// asin and acos on uniform x in each region, and acos on -x too.
static void stress_arcsine_regions(long n)
{
    static const double lo[] = {0.0, 0.45, 0.5, 0.5, 0.6, 0.9, 0.999};
    static const double hi[] = {0.5, 0.5, 0.52, 0.6, 0.9, 0.999, 1.0};

    for (uint32_t r = 0; r < RC_COUNT(lo); r++) {
        worst_case asin_worst = {0};
        worst_case acos_worst = {0};
        worst_case acos_negative_worst = {0};
        char name[64] = {0};

        for (long i = 0; i < n; i++) {
            double x = uniform(lo[r], hi[r]);

            note(&asin_worst, ulps(rc_asin_f64(x), asinl(x)), x, 0);
            note(&acos_worst, ulps(rc_acos_f64(x), acosl(x)), x, 0);
            note(&acos_negative_worst, ulps(rc_acos_f64(-x), acosl(-x)), -x, 0);
        }

        snprintf(name, sizeof name, "asin [%g, %g]", lo[r], hi[r]);
        report(name, asin_worst);
        snprintf(name, sizeof name, "acos [%g, %g]", lo[r], hi[r]);
        report(name, acos_worst);
        snprintf(name, sizeof name, "acos [-%g, -%g]", hi[r], lo[r]);
        report(name, acos_negative_worst);
    }
}


// asin log-spread from the subnormals to 1, and acos just below 1.
static void stress_arcsine_spread(long n)
{
    worst_case w = {0};

    for (long i = 0; i < n; i++) {
        double x = log_uniform(-1074, 0);

        note(&w, ulps(rc_asin_f64(x), asinl(x)), x, 0);
    }

    report("asin log-spread", w);

    w = (worst_case) {0};
    for (long i = 0; i < n; i++) {
        double x = 1.0 - log_uniform(-53, -1);

        note(&w, ulps(rc_acos_f64(x), acosl(x)), x, 0);
    }

    report("acos near 1", w);
}


// atan log-spread over every magnitude, and in each binade around its table.
static void stress_atan(long n)
{
    worst_case w = {0};

    for (long i = 0; i < n; i++) {
        double x = log_uniform(-1074, 1024);

        note(&w, ulps(rc_atan_f64(x), atanl(x)), x, 0);
    }

    report("atan log-spread", w);

    for (int e = -6; e <= 6; e++) {
        char name[64] = {0};

        w = (worst_case) {0};
        for (long i = 0; i < n / 4; i++) {
            double x = ldexp(uniform(1, 2), e);

            note(&w, ulps(rc_atan_f64(x), atanl(x)), x, 0);
        }

        snprintf(name, sizeof name, "atan [2^%d, 2^%d)", e, e + 1);
        report(name, w);
    }
}


// Notes atan2's error for the larger and smaller of a pair, arranged into octant.
static void note_atan2(worst_case *w, int octant, double far, double near)
{
    double y = (octant & 1) ? far : near;
    double x = (octant & 1) ? near : far;

    if (octant & 2) {
        x = -x;
    }

    note(w, ulps(rc_atan2_f64(y, x), atan2l(y, x)), y, x);
}


// atan2 in each octant with a small ratio, a ratio the table covers, and any magnitude; then a
// ratio that is subnormal.
static void stress_atan2(long n)
{
    worst_case w = {0};

    for (int octant = 0; octant < 4; octant++) {
        worst_case small = {0};
        worst_case table = {0};
        worst_case any = {0};
        char name[64] = {0};

        for (long i = 0; i < n; i++) {
            double far = uniform(1, 2);
            double near = far * log_uniform(-40, -4);

            note_atan2(&small, octant, far, near);
            near = far * uniform(1.0 / 16, 1);
            note_atan2(&table, octant, far, near);

            double magnitude = log_uniform(-1074, 1023);

            near = magnitude * log_uniform(-60, 0);
            note_atan2(&any, octant, magnitude, near);
        }

        snprintf(name, sizeof name, "atan2 oct %d small ratio", octant);
        report(name, small);
        snprintf(name, sizeof name, "atan2 oct %d table ratio", octant);
        report(name, table);
        snprintf(name, sizeof name, "atan2 oct %d any magnitude", octant);
        report(name, any);
    }

    for (long i = 0; i < n; i++) {
        double x = uniform(1, 2);
        double y = x * log_uniform(-1100, -1000);

        note(&w, ulps(rc_atan2_f64(y, x), atan2l(y, x)), y, x);
    }

    report("atan2 subnormal ratio", w);
}


static void stress_cbrt(long n)
{
    worst_case w = {0};

    for (long i = 0; i < n; i++) {
        double x = log_uniform(-1074, 1024);

        note(&w, ulps(rc_cbrt_f64(x), cbrtl(x)), x, 0);
    }

    report("cbrt log-spread", w);
}


int main(int argc, char **argv)
{
    long n = argc > 1 ? atol(argv[1]) : 10000000;

    stress_arcsine_regions(n);
    stress_arcsine_spread(n);
    stress_atan(n);
    stress_atan2(n);
    stress_cbrt(n);
    return 0;
}
