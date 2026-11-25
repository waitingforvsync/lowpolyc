// random.c
//
// splitmix32: the state counts up by the golden ratio's odd constant, and each draw runs the count
// through two xor-shift-multiplies. The multipliers are a well-tested pair from the hash-prospector
// search, with near-ideal avalanche, so a one-bit change in the state scatters across the draw.

#include "richc/random.h"


// Scatter the bits of count across the result.
static uint32_t rc_random_mix(uint32_t count)
{
    uint32_t first = (count ^ (count >> 16)) * 0x21F0AAADu;
    uint32_t second = (first ^ (first >> 15)) * 0x735A2D97u;

    return second ^ (second >> 15);
}


rc_random rc_random_make(uint32_t seed)
{
    return (rc_random) {.state = seed};
}


uint32_t rc_random_next(rc_random *random)
{
    RC_ASSERT(random);
    random->state += 0x9E3779B9u;
    return rc_random_mix(random->state);
}
