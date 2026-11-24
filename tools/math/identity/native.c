// native.c
//
// Prints driver.c's 24 hashes from a native build, one per line, in the same form as run.mjs does
// for the wasm build, so that the outputs can be compared as text.
//
//   out/native    (or: ./run.sh)

#include <stdint.h>
#include <stdio.h>

enum {
    F1_STRIDE = 17,
    SAMPLES = 20000000
};

uint64_t hash_f1(uint32_t which, uint32_t stride);
uint64_t hash_d1(uint32_t which, uint32_t count);
uint64_t hash_2(uint32_t which, uint32_t count);


int main(void)
{
    for (uint32_t which = 0; which < 10; which++) {
        printf("f1 %u %016llx\n", which, (unsigned long long)hash_f1(which, F1_STRIDE));
    }

    for (uint32_t which = 0; which < 10; which++) {
        printf("d1 %u %016llx\n", which, (unsigned long long)hash_d1(which, SAMPLES));
    }

    for (uint32_t which = 0; which < 4; which++) {
        printf("t2 %u %016llx\n", which, (unsigned long long)hash_2(which, SAMPLES));
    }

    return 0;
}
