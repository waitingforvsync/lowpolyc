// random.h
//
// A small, fast pseudo-random generator: splitmix32. One word of state, an add and two
// multiply-mixes a draw, and good statistical quality. Unlike xorshift it has no bad seeds, 0
// included. The same seed always replays the same stream, on every target. Not for cryptography.

#ifndef RC_RANDOM_H_
#define RC_RANDOM_H_

#include "richc/macros.h"

// A generator; {0} is one seeded with 0.
typedef struct rc_random {
    uint32_t state;
} rc_random;

// A generator seeded with seed.
rc_random rc_random_make(uint32_t seed);

// The next 32-bit draw, moving the generator on.
uint32_t rc_random_next(rc_random *random);

#endif // RC_RANDOM_H_
