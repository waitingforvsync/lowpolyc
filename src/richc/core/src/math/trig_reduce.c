// math/trig_reduce.c
//
// Reducing a huge double by the bits of 2/pi (Payne-Hanek), for sin, cos and tan: one of 2^20 or
// more. x is m 2^e for a whole m, and we want x 2/pi mod 4: the quadrant and the fraction past it.
// Bits of 2/pi worth 2^-j with e - j >= 2 only add multiples of 4, so we skip them and multiply m
// by a window of the bits after.
//
// The 53-bit m takes 224 bits, in 32-bit pieces. What that leaves out is under 2^-138, and we keep
// 126 bits of fraction, rounded to the nearest quadrant, so even the closest double to a multiple
// of pi/2, 2^-60.9 away, keeps r to a relative 2^-64; the fraction becomes a double-double and is
// multiplied by pi/2 as one. Floats, which need far fewer bits, are reduced inline (trig.h), from
// 2/pi's windows here.

#include "math/trig.h"

enum {
    // The 32-bit pieces of 2/pi multiplied in, and the pieces of the product, with one spare
    RC_TRIG_WINDOW_LIMBS = 7,
    RC_TRIG_PRODUCT_LIMBS = 10
};

// 2/pi in 32-bit pieces, most significant first: 2/pi is the sum of piece i times 2^(-32 (i + 1)).
// From mpmath at 2000 bits (tools/math/trig/trig_constants.py); 37 pieces reach the largest
// double's window.
static const uint32_t rc_trig_two_over_pi[] = {
    0xa2f9836eu, 0x4e441529u, 0xfc2757d1u, 0xf534ddc0u, 0xdb629599u, 0x3c439041u,
    0xfe5163abu, 0xdebbc561u, 0xb7246e3au, 0x424dd2e0u, 0x06492eeau, 0x09d1921cu,
    0xfe1deb1cu, 0xb129a73eu, 0xe88235f5u, 0x2ebb4484u, 0xe99c7026u, 0xb45f7e41u,
    0x3991d639u, 0x835339f4u, 0x9c845f8bu, 0xbdf9283bu, 0x1ff897ffu, 0xde05980fu,
    0xef2f118bu, 0x5a0a6d1fu, 0x6d367ecfu, 0x27cb09b7u, 0x4f463f66u, 0x9e5fea2du,
    0x7527bac7u, 0xebe5f17bu, 0x3d0739f7u, 0x8a5292eau, 0x6bfb5fb1u, 0x1f8d5d08u,
    0x56033046u
};

// The windows of 2/pi for floats (see trig.h). From mpmath (tools/math/trig/trig_constants.py); 44
// reach the largest float's.
const uint64_t rc_trig_two_over_pi_f32[RC_TRIG_TWO_OVER_PI_F32_COUNT] = {
    0x00a2f9836e4e4415ull, 0x0a2f9836e4e44152ull, 0xa2f9836e4e441529ull, 0x2f9836e4e441529full,
    0xf9836e4e441529fcull, 0x9836e4e441529fc2ull, 0x836e4e441529fc27ull, 0x36e4e441529fc275ull,
    0x6e4e441529fc2757ull, 0xe4e441529fc2757dull, 0x4e441529fc2757d1ull, 0xe441529fc2757d1full,
    0x441529fc2757d1f5ull, 0x41529fc2757d1f53ull, 0x1529fc2757d1f534ull, 0x529fc2757d1f534dull,
    0x29fc2757d1f534ddull, 0x9fc2757d1f534ddcull, 0xfc2757d1f534ddc0ull, 0xc2757d1f534ddc0dull,
    0x2757d1f534ddc0dbull, 0x757d1f534ddc0db6ull, 0x57d1f534ddc0db62ull, 0x7d1f534ddc0db629ull,
    0xd1f534ddc0db6295ull, 0x1f534ddc0db62959ull, 0xf534ddc0db629599ull, 0x534ddc0db6295993ull,
    0x34ddc0db6295993cull, 0x4ddc0db6295993c4ull, 0xddc0db6295993c43ull, 0xdc0db6295993c439ull,
    0xc0db6295993c4390ull, 0x0db6295993c43904ull, 0xdb6295993c439041ull, 0xb6295993c439041full,
    0x6295993c439041feull, 0x295993c439041fe5ull, 0x95993c439041fe51ull, 0x5993c439041fe516ull,
    0x993c439041fe5163ull, 0x93c439041fe5163aull, 0x3c439041fe5163abull, 0xc439041fe5163abdull
};

// A double's m times a window of 2/pi, as 32-bit pieces, least significant first.
typedef struct rc_trig_product {
    uint32_t limbs[RC_TRIG_PRODUCT_LIMBS];
} rc_trig_product;


// The mantissa times the window of 2/pi starting at piece first. Each step multiplies both halves
// of the mantissa by one piece and carries what passes 32 bits, which stays under 2^54.
static rc_trig_product rc_trig_multiply(uint64_t mantissa, uint32_t first)
{
    rc_trig_product product = {0};
    uint64_t mantissa_low = mantissa & 0xffffffffu;
    uint64_t mantissa_high = mantissa >> 32;
    uint64_t carry = 0;

    RC_ASSERT(first + RC_TRIG_WINDOW_LIMBS <= RC_COUNT(rc_trig_two_over_pi));
    for (uint32_t i = 0; i < RC_TRIG_WINDOW_LIMBS; i++) {
        uint64_t piece = rc_trig_two_over_pi[first + RC_TRIG_WINDOW_LIMBS - 1 - i];
        uint64_t low = mantissa_low * piece;
        uint64_t column = (low & 0xffffffffu) + (carry & 0xffffffffu);

        product.limbs[i] = (uint32_t)column;
        carry = (low >> 32) + mantissa_high * piece + (carry >> 32) + (column >> 32);
    }

    product.limbs[RC_TRIG_WINDOW_LIMBS] = (uint32_t)carry;
    product.limbs[RC_TRIG_WINDOW_LIMBS + 1] = (uint32_t)(carry >> 32);
    return product;
}


// The 64 bits of the product from bit position upwards.
static uint64_t rc_trig_bits_at(rc_trig_product product, uint32_t position)
{
    uint32_t index = position >> 5;
    uint32_t shift = position & 31u;
    uint64_t low = product.limbs[index] | ((uint64_t)product.limbs[index + 1] << 32);
    uint64_t high = product.limbs[index + 2];

    RC_ASSERT(index + 2 < RC_TRIG_PRODUCT_LIMBS);
    return shift == 0 ? low : (low >> shift) | (high << (64 - shift));
}


// 2^exponent, for exponents well inside the normal doubles.
static double rc_trig_power_of_2(int32_t exponent)
{
    return rc_bitcast_u64((uint64_t)(1023 + exponent) << 52);
}


// The 128-bit fraction high:low, in units of 2^-128, as a double-double. It is never below 2^-62,
// so high is never 0: its top 53 bits after the leading zeros make hi, and the next 53 make lo.
static rc_trig_dd rc_trig_fraction(uint64_t high, uint64_t low)
{
    uint32_t zeros = rc_clz_u64(high);
    uint64_t top = (high << zeros) | (zeros == 0 ? 0 : low >> (64 - zeros));
    uint64_t rest = low << zeros;
    uint64_t lead = top >> 11;
    uint64_t trail = ((top & 0x7ffu) << 42) | (rest >> 22);
    int32_t scale = (int32_t)zeros;

    RC_ASSERT(high != 0);
    return (rc_trig_dd) {
        .hi = (double)lead * rc_trig_power_of_2(-53 - scale),
        .lo = (double)trail * rc_trig_power_of_2(-106 - scale)
    };
}


// The signed fraction f, in turns of pi/2, times pi/2.
static rc_trig_dd rc_trig_times_half_pi(rc_trig_dd f)
{
    rc_trig_dd product = rc_trig_two_prod(f.hi, RC_TRIG_HALF_PI_HI);
    double lo = product.lo + (f.hi * RC_TRIG_HALF_PI_LO + f.lo * RC_TRIG_HALF_PI_HI);

    return rc_trig_fast_two_sum(product.hi, lo);
}


// x 2/pi mod 4 from the product, whose bit shift is worth 1. The two bits above shift are the
// quadrant and the 126 below the fraction. Past a half we round up, and the fraction left is
// negative: its magnitude is the two's complement, made here by flipping with a mask and adding 1.
static rc_trig_reduced rc_trig_round_quadrant(rc_trig_product product, uint32_t shift)
{
    uint64_t top = rc_trig_bits_at(product, shift - 62);
    uint64_t next = rc_trig_bits_at(product, shift - 126);
    uint64_t fraction_high = (top << 2) | (next >> 62);
    uint64_t fraction_low = next << 2;
    uint64_t round_up = fraction_high >> 63;
    uint64_t flip = 0 - round_up;
    uint64_t magnitude_low = (fraction_low ^ flip) + round_up;
    uint64_t magnitude_high = (fraction_high ^ flip) + (magnitude_low < round_up ? 1u : 0u);
    rc_trig_dd r = rc_trig_times_half_pi(rc_trig_fraction(magnitude_high, magnitude_low));
    double sign = round_up != 0 ? -1.0 : 1.0;

    return (rc_trig_reduced) {
        .r = {.hi = sign * r.hi, .lo = sign * r.lo},
        .quadrant = (uint32_t)((top >> 62) + round_up) & 3u
    };
}


// x of at least 2^20 (exponent -32 or more, as m 2^e), reduced.
rc_trig_reduced rc_trig_reduce_huge(double x)
{
    uint64_t bits = rc_bitcast_f64(x);
    int32_t exponent = (int32_t)(bits >> 52) - 1075;
    uint64_t mantissa = (bits & 0xfffffffffffffull) | 0x10000000000000ull;
    uint32_t first = exponent >= 34 ? (uint32_t)(exponent - 2) >> 5 : 0u;
    uint32_t shift = (uint32_t)((int32_t)(32 * (first + RC_TRIG_WINDOW_LIMBS)) - exponent);

    RC_ASSERT(exponent >= -32);
    return rc_trig_round_quadrant(rc_trig_multiply(mantissa, first), shift);
}
