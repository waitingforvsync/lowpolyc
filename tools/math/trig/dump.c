// dump.c
//
// Our sin, cos and tan of each input, as bits, for golden.py to check against mpmath and turn into
// the golden rows of test_math_trig.c. Reads one hex bit pattern per line and prints the input and
// the three results.
//
//   out/dump f64 < inputs
//   out/dump f32 < inputs

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "richc/math/math.h"


// Prints the row for the float with the given bits.
static void dump_f32(uint32_t bits)
{
    float x = rc_bitcast_u32(bits);

    printf("0x%08" PRIx32 " 0x%08" PRIx32 " 0x%08" PRIx32 " 0x%08" PRIx32 "\n", bits, rc_bitcast_f32(rc_sin_f32(x)),
        rc_bitcast_f32(rc_cos_f32(x)), rc_bitcast_f32(rc_tan_f32(x)));
}


// Prints the row for the double with the given bits.
static void dump_f64(uint64_t bits)
{
    double x = rc_bitcast_u64(bits);

    printf("0x%016" PRIx64 " 0x%016" PRIx64 " 0x%016" PRIx64 " 0x%016" PRIx64 "\n", bits, rc_bitcast_f64(rc_sin_f64(x)),
        rc_bitcast_f64(rc_cos_f64(x)), rc_bitcast_f64(rc_tan_f64(x)));
}


int main(int argc, char **argv)
{
    char line[128] = {0};
    bool is_f32 = argc > 1 && strcmp(argv[1], "f32") == 0;

    while (fgets(line, sizeof line, stdin)) {
        uint64_t bits = 0;

        if (sscanf(line, "%" SCNx64, &bits) != 1) {
            continue;
        }

        if (is_f32) {
            dump_f32((uint32_t)bits);
        }
        else {
            dump_f64(bits);
        }
    }

    return 0;
}
