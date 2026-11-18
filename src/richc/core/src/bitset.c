// bitset.c
//
// The out-of-line half of rc_bitset: sizing, whole-set operations and searching.

#include "richc/bitset.h"

#include "richc/arena.h"
#include "richc/math/math.h"
#include "richc/mem.h"

enum {
    // The bytes in a word, as a uint32_t for size arithmetic
    RC_BITSET_WORD_BYTES = sizeof(uint32_t),

    // Growth never leaves less room than this
    RC_BITSET_MIN_GROWTH = 64
};

// The number of words that hold bits bits.
static uint32_t rc_bitset_words(uint32_t bits)
{
    return (bits >> 5) + ((bits & 31u) != 0);
}


// The capacity of words words in bits. The most words a uint32_t of bits can need hold one bit
// more than it can count, so that saturates.
static uint32_t rc_bitset_cap_of(uint32_t words)
{
    return words > (UINT32_MAX >> 5) ? UINT32_MAX : words << 5;
}


// Clear the bits from num up to the end of its word.
static void rc_bitset_clear_tail(rc_bitset *bs, uint32_t num)
{
    uint32_t bit = num & 31u;

    if (bit != 0) {
        bs->data[num >> 5] &= (1u << bit) - 1u;
    }
}


// The capacity in bits for need bits, growing from cap: the larger of double cap, the need, or 64.
static uint32_t rc_bitset_grown_cap(uint32_t cap, uint32_t need)
{
    uint32_t doubled = cap > UINT32_MAX / 2 ? UINT32_MAX : cap * 2;
    uint32_t new_cap = need > doubled ? need : doubled;

    return new_cap < RC_BITSET_MIN_GROWTH ? RC_BITSET_MIN_GROWTH : new_cap;
}


// Make room for n more bits, growing by grown_cap when they do not fit. Panics (in release too)
// if the size would wrap.
static void rc_bitset_grow(rc_bitset *bs, uint32_t n, rc_arena *arena)
{
    RC_PANIC(n <= UINT32_MAX - bs->num);
    if (bs->num + n <= bs->cap) {
        return;
    }

    rc_bitset_reserve(bs, rc_bitset_grown_cap(bs->cap, bs->num + n), arena);
}


rc_bitset rc_bitset_make(uint32_t num, rc_arena *arena)
{
    rc_bitset bs = {0};

    rc_bitset_reserve(&bs, num, arena);
    bs.num = num;
    return bs;
}


rc_bitset rc_bitset_make_copy(const rc_bitset *src, rc_arena *arena)
{
    rc_bitset bs = rc_bitset_make(src->num, arena);

    if (src->num > 0) {
        rc_memcpy(bs.data, src->data, rc_bitset_words(src->num) * RC_BITSET_WORD_BYTES);
    }

    return bs;
}


void rc_bitset_reserve(rc_bitset *bs, uint32_t min_bits, rc_arena *arena)
{
    uint32_t old_words = bs->cap >> 5;
    uint32_t new_words = rc_bitset_words(min_bits);

    if (min_bits <= bs->cap) {
        return;
    }

    RC_ASSERT(arena);

    // realloc_zero clears the new words, so the bits past num stay clear
    bs->data = rc_arena_realloc_zero(arena, bs->data, old_words * RC_BITSET_WORD_BYTES, new_words * RC_BITSET_WORD_BYTES);
    bs->cap = rc_bitset_cap_of(new_words);
}


void rc_bitset_resize(rc_bitset *bs, uint32_t new_num, rc_arena *arena)
{
    if (new_num > bs->num) {
        rc_bitset_reserve(bs, new_num, arena);
    }
    else if (new_num < bs->num) {
        uint32_t new_words = rc_bitset_words(new_num);
        uint32_t old_words = rc_bitset_words(bs->num);

        rc_bitset_clear_tail(bs, new_num);
        rc_memset(bs->data + new_words, 0, (old_words - new_words) * RC_BITSET_WORD_BYTES);
    }

    bs->num = new_num;
}


uint32_t rc_bitset_push(rc_bitset *bs, bool value, rc_arena *arena)
{
    uint32_t i = bs->num;

    // The new bit is already clear, being past the old num
    rc_bitset_grow(bs, 1, arena);
    bs->num = i + 1;
    if (value) {
        rc_bitset_set(bs, i);
    }

    return i;
}


uint32_t rc_bitset_push_n_zero(rc_bitset *bs, uint32_t n, rc_arena *arena)
{
    uint32_t start = bs->num;

    rc_bitset_grow(bs, n, arena);
    bs->num = start + n;
    return start;
}


void rc_bitset_release(rc_bitset *bs, rc_arena *arena)
{
    if (bs->data) {
        rc_arena_free(arena, bs->data, rc_bitset_words(bs->cap) * RC_BITSET_WORD_BYTES);
    }

    *bs = (rc_bitset) {0};
}


void rc_bitset_clear_all(rc_bitset *bs)
{
    if (bs->num > 0) {
        rc_memset(bs->data, 0, rc_bitset_words(bs->num) * RC_BITSET_WORD_BYTES);
    }
}


void rc_bitset_copy(rc_bitset *dst, const rc_bitset *src)
{
    RC_ASSERT(dst->num == src->num);
    if (src->num > 0) {
        rc_memcpy(dst->data, src->data, rc_bitset_words(src->num) * RC_BITSET_WORD_BYTES);
    }
}


void rc_bitset_union(rc_bitset *dst, const rc_bitset *src)
{
    uint32_t dst_words = rc_bitset_words(dst->num);
    uint32_t src_words = rc_bitset_words(src->num);
    uint32_t shared = dst_words < src_words ? dst_words : src_words;

    for (uint32_t i = 0; i < shared; i++) {
        dst->data[i] |= src->data[i];
    }

    // A wider src can set bits past dst->num in the last shared word
    rc_bitset_clear_tail(dst, dst->num);
}


void rc_bitset_intersection(rc_bitset *dst, const rc_bitset *src)
{
    uint32_t dst_words = rc_bitset_words(dst->num);
    uint32_t src_words = rc_bitset_words(src->num);

    // Words src does not have count as clear; and-ing never sets a bit, so the tail stays clear
    for (uint32_t i = 0; i < dst_words; i++) {
        dst->data[i] &= i < src_words ? src->data[i] : 0;
    }
}


bool rc_bitset_intersects(const rc_bitset *a, const rc_bitset *b)
{
    uint32_t a_words = rc_bitset_words(a->num);
    uint32_t b_words = rc_bitset_words(b->num);
    uint32_t shared = a_words < b_words ? a_words : b_words;

    for (uint32_t i = 0; i < shared; i++) {
        if ((a->data[i] & b->data[i]) != 0) {
            return true;
        }
    }
    return false;
}


bool rc_bitset_is_equal(const rc_bitset *a, const rc_bitset *b)
{
    if (a->num != b->num) {
        return false;
    }

    if (a->num == 0) {
        return true;
    }

    return rc_memcmp(a->data, b->data, rc_bitset_words(a->num) * RC_BITSET_WORD_BYTES) == 0;
}


uint32_t rc_bitset_count_set(const rc_bitset *bs)
{
    uint32_t words = rc_bitset_words(bs->num);
    uint32_t count = 0;

    for (uint32_t i = 0; i < words; i++) {
        count += rc_popcount_u32(bs->data[i]);
    }
    return count;
}


// The index of the first set bit at or after pos, which is below num, or RC_INDEX_NONE. A set bit
// found in a later word is below num too, since the bits past it are clear.
static uint32_t rc_bitset_scan(const rc_bitset *bs, uint32_t pos)
{
    // The first word, without the bits before pos
    uint32_t first_word = bs->data[pos >> 5] >> (pos & 31u);
    uint32_t words = rc_bitset_words(bs->num);

    if (first_word != 0) {
        return pos + rc_ctz_u32(first_word);
    }

    for (uint32_t i = (pos >> 5) + 1; i < words; i++) {
        if (bs->data[i] != 0) {
            return (i << 5) + rc_ctz_u32(bs->data[i]);
        }
    }
    return RC_INDEX_NONE;
}


uint32_t rc_bitset_next_set(const rc_bitset *bs, uint32_t pos)
{
    if (pos >= bs->num) {
        return RC_INDEX_NONE;
    }

    return rc_bitset_scan(bs, pos);
}
