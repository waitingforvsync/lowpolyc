// math/trig_reduce_large.c
//
// Reducing a large argument by the nearest multiple of pi/2, with as many bits of 2/pi as it
// takes. Ported from musl 1.2.5 (MIT), src/math/__rem_pio2_large.c, which is FreeBSD's
// k_rem_pio2.c. Its notice:
//
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunSoft, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.
//
// We multiply x by 2/pi in 24-bit chunks, held as whole numbers in doubles, and keep the whole
// part modulo 8 as the quadrant and the fraction as the remainder. Eight quarter turns are a whole
// turn, so we skip the leading chunks of 2/pi whose products with x are multiples of 8, and the
// work doesn't grow with x. If the fraction cancels to zero in the chunks we have, we bring in
// more and go again.

#include "trig.h"

// The most chunks the product of a double and 2/pi can need.
enum { RC_TRIG_MAX_CHUNKS = 20 };

// The state of one reduction. product holds x * 2/pi in chunks 0 to last, from the most
// significant, where chunk 0 is in units of 2^scale, each a whole number that may run past 24 bits
// until carried. chunks holds them carried into exactly 24 bits, from the least significant, and
// terms the remainder times pi/2, from the most significant. We skip the first skip chunks of
// 2/pi, and line the rest up in two_over_pi so that parts[j] times entry last_part + i - j lands
// in chunk i. initial_last is the last chunk we start with.
typedef struct rc_trig_large {
    const double *parts;
    int32_t last_part;
    int32_t skip;
    int32_t initial_last;
    int32_t last;
    int32_t scale;
    double two_over_pi[RC_TRIG_MAX_CHUNKS];
    double product[RC_TRIG_MAX_CHUNKS];
    uint32_t chunks[RC_TRIG_MAX_CHUNKS];
    double terms[RC_TRIG_MAX_CHUNKS];
} rc_trig_large;

// The top of the product: the quadrant, before taking it modulo 8, and the fraction of a quadrant
// below it. round_up is nonzero when the fraction is at least a half, so that the nearest multiple
// of pi/2 is the next one up: 1 when the chunks showed it, and 2 when the fraction did.
typedef struct rc_trig_split {
    int32_t quadrant;
    int32_t round_up;
    double fraction;
} rc_trig_split;

// 2/pi in 24-bit chunks: chunk i is bits 24i to 24i + 23 after the binary point. Doubles use up to
// chunk 45, and a few beyond it when the fraction cancels.
static const uint32_t two_over_pi_bits[] = {
    0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62,
    0x95993C, 0x439041, 0xFE5163, 0xABDEBB, 0xC561B7, 0x246E3A,
    0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129,
    0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C, 0x7026B4, 0x5F7E41,
    0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8,
    0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D, 0x367ECF,
    0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5,
    0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08,
    0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3,
    0x91615E, 0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880,
    0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B,
};

// pi/2 in 24-bit chunks.
static const double pio2_chunks[] = {
    1.57079625129699707031e+00,
    7.54978941586159635335e-08,
    5.39030252995776476554e-15,
    3.28200341580791294123e-22,
    1.27065575308067607349e-29,
    1.22933308981111328932e-36,
    2.73370053816464559624e-44,
    2.16741683877804819444e-51,
};

// x * 2^exponent, for an exponent that keeps 2^exponent a normal double, as ours always do.
static double rc_trig_scalbn(double x, int32_t exponent)
{
    RC_ASSERT(exponent >= -1022);
    RC_ASSERT(exponent <= 1023);
    return x * rc_bitcast_u64((uint64_t)(0x3ff + exponent) << 52);
}


// Chunks first to last of 2/pi, after the skipped ones, as doubles. Those before the first chunk
// are zero.
static void rc_trig_load_two_over_pi(rc_trig_large *work, int32_t first, int32_t last)
{
    for (int32_t i = first; i <= last; i++) {
        int32_t index = work->skip + i;

        RC_ASSERT(index < (int32_t)RC_COUNT(two_over_pi_bits));
        RC_ASSERT(work->last_part + i < RC_TRIG_MAX_CHUNKS);
        work->two_over_pi[work->last_part + i] = index < 0 ? 0.0 : (double)two_over_pi_bits[index];
    }
}


// Chunk index of the product: the sum of the parts of x times the chunks of 2/pi that land there.
static double rc_trig_product_chunk(const rc_trig_large *work, int32_t index)
{
    double sum = 0.0;

    for (int32_t j = 0; j <= work->last_part; j++) {
        sum += work->parts[j] * work->two_over_pi[work->last_part + index - j];
    }

    return sum;
}


// Work out chunks first to last of the product, with the chunks of 2/pi they need.
static void rc_trig_extend(rc_trig_large *work, int32_t first, int32_t last)
{
    rc_trig_load_two_over_pi(work, first, last);

    for (int32_t i = first; i <= last; i++) {
        work->product[i] = rc_trig_product_chunk(work, i);
    }
}


// We start with chunks 0 to initial_last, two more than a float or a double strictly needs, as up
// to 24 bits of the whole part and 23 of the fraction can cancel before we test for it.
static rc_trig_large rc_trig_large_make(const double *parts, int32_t num_parts, int32_t exponent, bool is_double)
{
    int32_t skip = rc_max_i32((exponent - 3) / 24, 0);
    int32_t initial_last = is_double ? 4 : 3;
    rc_trig_large work = {
        .parts = parts,
        .last_part = num_parts - 1,
        .skip = skip,
        .initial_last = initial_last,
        .last = initial_last,
        .scale = exponent - 24 * (skip + 1)
    };

    rc_trig_load_two_over_pi(&work, -work.last_part, -1);
    rc_trig_extend(&work, 0, initial_last);
    return work;
}


// Carry the product into exact 24-bit chunks, from the least significant, all but the top one,
// which comes back with the carries added in.
static double rc_trig_carry(rc_trig_large *work)
{
    double sum = work->product[work->last];

    for (int32_t i = 0; i < work->last; i++) {
        double carry = (double)(int32_t)(0x1p-24 * sum);

        work->chunks[i] = (uint32_t)(sum - 0x1p24 * carry);
        sum = work->product[work->last - 1 - i] + carry;
    }

    return sum;
}


// With a scale of 1 or 2, the top bits of the top chunk belong to the quadrant, and the bit below
// them says whether to round up.
static rc_trig_split rc_trig_split_top_chunk(rc_trig_large *work, int32_t whole, double fraction)
{
    int32_t top = work->last - 1;
    uint32_t shift = (uint32_t)(24 - work->scale);
    uint32_t quadrant_bits = work->chunks[top] >> shift;

    work->chunks[top] -= quadrant_bits << shift;
    return (rc_trig_split) {
        .quadrant = whole + (int32_t)quadrant_bits,
        .round_up = (int32_t)(work->chunks[top] >> (shift - 1u)),
        .fraction = fraction
    };
}


// Carry the product and split its top into the quadrant and the fraction below it.
static rc_trig_split rc_trig_distill(rc_trig_large *work)
{
    double top = rc_trig_scalbn(rc_trig_carry(work), work->scale);
    double below_eight = top - 8.0 * rc_floor_f64(top * 0.125);
    int32_t whole = (int32_t)below_eight;
    double fraction = below_eight - (double)whole;

    if (work->scale > 0) {
        return rc_trig_split_top_chunk(work, whole, fraction);
    }

    if (work->scale == 0) {
        return (rc_trig_split) {.quadrant = whole, .round_up = (int32_t)(work->chunks[work->last - 1] >> 23), .fraction = fraction};
    }

    return (rc_trig_split) {.quadrant = whole, .round_up = fraction >= 0.5 ? 2 : 0, .fraction = fraction};
}


// Replace the chunks q with 1 - q, returning whether any was nonzero, and so borrowed from above.
static bool rc_trig_complement(rc_trig_large *work)
{
    bool borrowing = false;

    for (int32_t i = 0; i < work->last; i++) {
        uint32_t chunk = work->chunks[i];

        if (borrowing) {
            work->chunks[i] = 0xffffffu - chunk;
        }
        else if (chunk != 0) {
            borrowing = true;
            work->chunks[i] = 0x1000000u - chunk;
        }
    }

    return borrowing;
}


// 1 - fraction, less the 2^scale the chunks below it borrowed, if they did.
static double rc_trig_one_less(double fraction, bool borrowed, int32_t scale)
{
    double complement = 1.0 - fraction;

    return borrowed ? complement - rc_trig_scalbn(1.0, scale) : complement;
}


// Round up to the next quadrant, which leaves 1 less the old remainder in the chunks and fraction.
static rc_trig_split rc_trig_round_up(rc_trig_large *work, rc_trig_split split)
{
    bool borrowed = rc_trig_complement(work);
    double fraction = split.round_up == 2 ? rc_trig_one_less(split.fraction, borrowed, work->scale) : split.fraction;

    // The complement set the top bits of the top chunk, which belong to the quadrant
    if (work->scale > 0) {
        work->chunks[work->last - 1] &= 0xffffffu >> work->scale;
    }

    return (rc_trig_split) {.quadrant = split.quadrant + 1, .round_up = split.round_up, .fraction = fraction};
}


// Split the product into the quadrant and the remainder from the nearest multiple of pi/2.
static rc_trig_split rc_trig_split_product(rc_trig_large *work)
{
    rc_trig_split split = rc_trig_distill(work);

    return split.round_up > 0 ? rc_trig_round_up(work, split) : split;
}


// True if the chunks past the ones we started with are all zero.
static bool rc_trig_are_new_chunks_zero(const rc_trig_large *work)
{
    uint32_t bits = 0;

    for (int32_t i = work->initial_last; i < work->last; i++) {
        bits |= work->chunks[i];
    }

    return bits == 0;
}


// How many more chunks of 2/pi we need: none, unless the remainder has cancelled to zero in all
// the chunks we have, and then one more than the run of zero chunks below the starting ones.
static int32_t rc_trig_chunks_needed(const rc_trig_large *work, double fraction)
{
    int32_t needed = 1;

    if (fraction != 0.0) {
        return 0;
    }

    if (!rc_trig_are_new_chunks_zero(work)) {
        return 0;
    }

    while (work->chunks[work->initial_last - needed] == 0) {
        needed++;
    }

    return needed;
}


// Split the product, bringing in more chunks of 2/pi for as long as the remainder cancels to zero.
static rc_trig_split rc_trig_settle(rc_trig_large *work)
{
    rc_trig_split split = rc_trig_split_product(work);
    int32_t needed = rc_trig_chunks_needed(work, split.fraction);

    while (needed > 0) {
        rc_trig_extend(work, work->last + 1, work->last + needed);
        work->last += needed;
        split = rc_trig_split_product(work);
        needed = rc_trig_chunks_needed(work, split.fraction);
    }

    return split;
}


// A zero fraction leaves the remainder all in the chunks, so we drop the zero ones from the top.
static void rc_trig_drop_zero_chunks(rc_trig_large *work)
{
    do {
        work->last -= 1;
        work->scale -= 24;
    } while (work->chunks[work->last] == 0);
}


// Store the fraction as the top chunk, or two if it reaches 2^24 once scaled.
static void rc_trig_store_fraction(rc_trig_large *work, double fraction)
{
    double scaled = rc_trig_scalbn(fraction, -work->scale);

    if (scaled >= 0x1p24) {
        double high = (double)(int32_t)(0x1p-24 * scaled);

        work->chunks[work->last] = (uint32_t)(scaled - 0x1p24 * high);
        work->last += 1;
        work->scale += 24;
        work->chunks[work->last] = (uint32_t)high;
    }
    else {
        work->chunks[work->last] = (uint32_t)scaled;
    }
}


// Turn the chunks back into doubles, each scaled to its place, into the product. It now runs from
// the least significant.
static void rc_trig_unchunk(rc_trig_large *work)
{
    double weight = rc_trig_scalbn(1.0, work->scale);

    for (int32_t i = work->last; i >= 0; i--) {
        work->product[i] = weight * (double)work->chunks[i];
        weight *= 0x1p-24;
    }
}


// The products of chunk first + k of the remainder and chunk k of pi/2, which all have the same
// weight, summed for k up to initial_last.
static double rc_trig_pio2_term(const rc_trig_large *work, int32_t first)
{
    int32_t last_k = rc_min_i32(work->initial_last, work->last - first);
    double sum = 0.0;

    for (int32_t k = 0; k <= last_k; k++) {
        sum += pio2_chunks[k] * work->product[first + k];
    }

    return sum;
}


// The remainder times pi/2, as terms from the most significant.
static void rc_trig_multiply_by_pio2(rc_trig_large *work)
{
    for (int32_t i = work->last; i >= 0; i--) {
        work->terms[work->last - i] = rc_trig_pio2_term(work, i);
    }
}


// The terms summed from the smallest up.
static double rc_trig_sum_terms(const rc_trig_large *work)
{
    double sum = 0.0;

    for (int32_t i = work->last; i >= 0; i--) {
        sum += work->terms[i];
    }

    return sum;
}


// What rounding the terms to head lost.
static double rc_trig_sum_tail(const rc_trig_large *work, double head)
{
    double tail = work->terms[0] - head;

    for (int32_t i = 1; i <= work->last; i++) {
        tail += work->terms[i];
    }

    return tail;
}


// The reduction, from the remainder's terms and the split, which rounding up turns negative.
static rc_trig_reduced_f64 rc_trig_sum(const rc_trig_large *work, rc_trig_split split)
{
    double head = rc_trig_sum_terms(work);
    double tail = rc_trig_sum_tail(work, head);
    bool negative = split.round_up != 0;

    return (rc_trig_reduced_f64) {
        .quadrant = (int32_t)((uint32_t)split.quadrant & 7u),
        .hi = negative ? -head : head,
        .lo = negative ? -tail : tail
    };
}


rc_trig_reduced_f64 rc_trig_reduce_large(const double *parts, int32_t num_parts, int32_t exponent, bool is_double)
{
    rc_trig_large work = rc_trig_large_make(parts, num_parts, exponent, is_double);
    rc_trig_split split = rc_trig_settle(&work);

    if (split.fraction == 0.0) {
        rc_trig_drop_zero_chunks(&work);
    }
    else {
        rc_trig_store_fraction(&work, split.fraction);
    }

    rc_trig_unchunk(&work);
    rc_trig_multiply_by_pio2(&work);
    return rc_trig_sum(&work, split);
}
