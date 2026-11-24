// check_reduce.c
//
// Checks the log reduction of src/math/exp_log_f64.h gives r = m c - 1 exactly, with the tables of
// 256 that log and log2 each have, for the first, second, last and next to last significands of
// every interval and 100 million random ones between. Prints how many were inexact and the range
// r covers for each table, which should be within [-2^-8.45, 2^-8].
//
//   out/check_reduce    (or: ./run.sh check)

#include <quadmath.h>
#include <stdio.h>

#include "math/exp_log_f64.h"

enum {
    EDGES_PER_INTERVAL = 4,
    RANDOM_SAMPLES = 100000000
};


// The significand bits of the n-th sample for a table of 2^table_bits: interval edges first,
// then random.
static uint64_t sample_mantissa(uint64_t n, uint32_t table_bits, uint64_t *state)
{
    uint64_t width = 1ull << (52 - table_bits);
    uint64_t edge_offsets[EDGES_PER_INTERVAL] = {0, width - 1, 1, width - 2};

    if (n < ((uint64_t)EDGES_PER_INTERVAL << table_bits)) {
        return (n / EDGES_PER_INTERVAL) * width + edge_offsets[n % EDGES_PER_INTERVAL];
    }

    *state = *state * 6364136223846793005ull + 1442695040888963407ull;
    return *state >> 12;
}


// Checks one table, printing its line.
static void check_table(const char *name, uint32_t table_bits, const uint16_t *inverse, double r_scale)
{
    uint64_t state = 1;
    uint64_t inexact = 0;
    double r_min = 0;
    double r_max = 0;
    uint64_t count = ((uint64_t)EDGES_PER_INTERVAL << table_bits) + RANDOM_SAMPLES;

    for (uint64_t n = 0; n < count; n++) {
        uint64_t bits = 0x3ff0000000000000ull | sample_mantissa(n, table_bits, &state);
        rc_log_f64_reduced reduced = rc_log_f64_reduce(bits, table_bits, inverse, 0x1p52 + 1023.0);
        double r = rc_log_f64_r(reduced.scaled, table_bits, r_scale);
        double m = rc_bitcast_u64(bits);
        __float128 c = (__float128)inverse[reduced.index] / (__float128)(2u << table_bits);

        if ((__float128)r != (__float128)m * c - 1 || reduced.k != 0.0) {
            inexact++;
        }

        r_min = r < r_min ? r : r_min;
        r_max = r > r_max ? r : r_max;
    }

    printf("%s: inexact r: %llu, r in [%a, %a]\n", name, (unsigned long long)inexact, r_min, r_max);
}


int main(void)
{
    check_table("log", 8, rc_log_f64_inverse, rc_log_f64_data.r_scale);
    check_table("log2", 8, rc_log2_f64_inverse, rc_log_f64_data.r_scale);
    return 0;
}
