// cbrt_bias.c
//
// Finds the biases of cbrt's first guesses (RC_CBRT_BIAS in cbrt_f32.c and cbrt_f64.c): the bias
// that, added to a third of |x|'s bits (a float's 32, or a double's top 32), makes the guess at
// cbrt(x) closest relatively, at worst over every input. The guess cycles with the exponent mod 3,
// so [1, 8) covers every case: every float there, and both ends of the range each top 32 bits of
// a double cover. The worst error falls as the bias rises toward the best and climbs after, so a
// ternary search finds it. Prints each bias and its worst relative error.
//
//   ./run.sh constants

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// The worst relative error of the float guess with the given bias, over every float in [1, 8).
static double worst_f32(uint32_t bias)
{
    double worst = 0.0;

    for (uint32_t bits = 0x3f800000u; bits < 0x41000000u; bits++) {
        uint32_t guess_bits = bits / 3 + bias;
        float x = 0.0f;
        float guess = 0.0f;

        memcpy(&x, &bits, sizeof x);
        memcpy(&guess, &guess_bits, sizeof guess);
        worst = fmax(worst, fabs((double)guess / cbrt((double)x) - 1.0));
    }

    return worst;
}


// The worst relative error of the double guess with the given bias, over every top 32 bits of a
// double in [1, 8), each at both ends of the doubles it covers.
static double worst_f64(uint32_t bias)
{
    double worst = 0.0;

    for (uint64_t top = 0x3ff00000u; top < 0x40200000u; top++) {
        uint64_t guess_bits = (top / 3 + bias) << 32;
        uint64_t low_bits = top << 32;
        uint64_t high_bits = low_bits | 0xffffffffu;
        double guess = 0.0;
        double low = 0.0;
        double high = 0.0;

        memcpy(&guess, &guess_bits, sizeof guess);
        memcpy(&low, &low_bits, sizeof low);
        memcpy(&high, &high_bits, sizeof high);
        worst = fmax(worst, fabs(guess / cbrt(low) - 1.0));
        worst = fmax(worst, fabs(guess / cbrt(high) - 1.0));
    }

    return worst;
}


// The bias in [low, high] with the least worst error, by ternary search, then a look either side.
static uint32_t best_bias(double (*worst)(uint32_t), uint32_t low, uint32_t high)
{
    uint32_t best = 0;

    while (high - low > 2) {
        uint32_t third = (high - low) / 3;

        if (worst(low + third) < worst(high - third)) {
            high = high - third;
        }
        else {
            low = low + third;
        }
    }

    best = low;
    for (uint32_t bias = low - 2; bias <= high + 2; bias++) {
        if (worst(bias) < worst(best)) {
            best = bias;
        }
    }

    return best;
}


int main(void)
{
    uint32_t bias_f32 = best_bias(worst_f32, 700000000u, 720000000u);
    uint32_t bias_f64 = best_bias(worst_f64, 700000000u, 730000000u);

    printf("cbrt_f32 bias %u  worst relative error 2^%.2f\n", bias_f32, log2(worst_f32(bias_f32)));
    printf("cbrt_f64 bias %u  worst relative error 2^%.2f\n", bias_f64, log2(worst_f64(bias_f64)));
    return 0;
}
