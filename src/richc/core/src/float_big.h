// float_big.h
//
// The little bignum behind turning floating point into decimal text and back: an unsigned integer
// of up to RC_FLOAT_BIG_WORDS 32-bit words, with the operations both directions share. Private to
// core: mstr_float.c formats with it and str_parse_float.c parses with it, each adding the
// operations only it needs.

#ifndef RC_FLOAT_BIG_H_
#define RC_FLOAT_BIG_H_

#include "richc/macros.h"

enum {
    // Enough words for the biggest number either side makes. Parsing is the larger: 800 decimal
    // digits less 10^1124, shifted up by 66 bits for the division, is under 3830 bits
    RC_FLOAT_BIG_WORDS = 124
};

// An unsigned integer, least significant word first. num counts the words in use, so zero has none.
typedef struct rc_float_big {
    uint32_t words[RC_FLOAT_BIG_WORDS];
    uint32_t num;
} rc_float_big;

// Drop zero words from the top.
static inline void rc_float_big_trim(rc_float_big *big)
{
    while (big->num > 0 && big->words[big->num - 1] == 0) {
        big->num--;
    }
}

// Multiply by factor and add addend, in place.
static inline void rc_float_big_multiply_add(rc_float_big *big, uint32_t factor, uint32_t addend)
{
    uint64_t carry = addend;

    for (uint32_t i = 0; i < big->num; i++) {
        uint64_t part = (uint64_t)big->words[i] * factor + carry;

        big->words[i] = (uint32_t)part;
        carry = part >> 32;
    }

    if (carry != 0) {
        RC_ASSERT(big->num < RC_FLOAT_BIG_WORDS);
        big->words[big->num] = (uint32_t)carry;
        big->num++;
    }
}

// Divide by divisor in place, returning the remainder.
static inline uint32_t rc_float_big_divide(rc_float_big *big, uint32_t divisor)
{
    uint64_t remainder = 0;

    for (uint32_t i = big->num; i-- > 0;) {
        uint64_t part = (remainder << 32) | big->words[i];

        big->words[i] = (uint32_t)(part / divisor);
        remainder = part % divisor;
    }
    rc_float_big_trim(big);
    return (uint32_t)remainder;
}

#endif // RC_FLOAT_BIG_H_
