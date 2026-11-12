// bitset.h
//
// A growable array of bits, packed 32 to a word: bit i is bit i % 32 of word i / 32.
//
// Every bit at or past num is zero, and every operation keeps it that way. So whole words can be
// scanned, compared and counted with no check against num at the end.
//
// The zero value {0} is an empty bitset. To visit the set bits in order:
//
//   for (uint32_t i = rc_bitset_first_set(&bs); i != RC_INDEX_NONE;
//        i = rc_bitset_next_set(&bs, i + 1)) {
//       ...
//   }

#ifndef RC_BITSET_H_
#define RC_BITSET_H_

#include "richc/macros.h"

// We only ever take an arena by pointer, so a forward declaration will do.
typedef struct rc_arena rc_arena;

typedef struct rc_bitset {
    uint32_t *data;
    uint32_t num;  // the number of bits
    uint32_t cap;  // the capacity in bits, a whole number of words
} rc_bitset;

// ---- making and sizing ----

// A bitset of num clear bits, with exactly the capacity it needs.
rc_bitset rc_bitset_make(uint32_t num, rc_arena *arena);

// A copy of src, the same size.
rc_bitset rc_bitset_make_copy(const rc_bitset *src, rc_arena *arena);

// Make room for at least min_bits, rounded up to a whole word; never shrinks.
void rc_bitset_reserve(rc_bitset *bs, uint32_t min_bits, rc_arena *arena);

// Set the size to new_num bits. Bits added are clear; bits dropped are cleared. Shrinking, or
// growing within capacity, needs no arena.
void rc_bitset_resize(rc_bitset *bs, uint32_t new_num, rc_arena *arena);

// Append a bit, returning its index. Grows geometrically.
uint32_t rc_bitset_push(rc_bitset *bs, bool value, rc_arena *arena);

// Append n clear bits, returning the index of the first. Grows geometrically.
uint32_t rc_bitset_push_n_zero(rc_bitset *bs, uint32_t n, rc_arena *arena);

// Free the words (if they are the arena's last allocation) and zero the bitset.
void rc_bitset_release(rc_bitset *bs, rc_arena *arena);

// ---- whole sets ----

// Clear every bit, keeping the size and capacity.
void rc_bitset_clear_all(rc_bitset *bs);

// Overwrite dst with src, which must be the same size.
void rc_bitset_copy(rc_bitset *dst, const rc_bitset *src);

// dst |= src, over dst's bits only: src's bits past dst->num are ignored.
void rc_bitset_union(rc_bitset *dst, const rc_bitset *src);

// dst &= src, over dst's bits only: dst's bits past src->num are cleared.
void rc_bitset_intersection(rc_bitset *dst, const rc_bitset *src);

// True when a and b share a set bit within the smaller of their sizes.
bool rc_bitset_intersects(const rc_bitset *a, const rc_bitset *b);

// True when a and b are the same size with the same bits set.
bool rc_bitset_is_equal(const rc_bitset *a, const rc_bitset *b);

// The number of set bits, counted each time.
uint32_t rc_bitset_count_set(const rc_bitset *bs);

// The index of the first set bit at or after pos, or RC_INDEX_NONE.
uint32_t rc_bitset_next_set(const rc_bitset *bs, uint32_t pos);

// ---- single bits ----

// Set bit i.
static inline void rc_bitset_set(rc_bitset *bs, uint32_t i)
{
    RC_ASSERT(i < bs->num);
    bs->data[i >> 5] |= 1u << (i & 31u);
}

// Clear bit i.
static inline void rc_bitset_clear(rc_bitset *bs, uint32_t i)
{
    RC_ASSERT(i < bs->num);
    bs->data[i >> 5] &= ~(1u << (i & 31u));
}

// True when bit i is set.
static inline bool rc_bitset_is_set(const rc_bitset *bs, uint32_t i)
{
    RC_ASSERT(i < bs->num);
    return (bs->data[i >> 5] >> (i & 31u)) & 1u;
}

// The index of the first set bit, or RC_INDEX_NONE.
static inline uint32_t rc_bitset_first_set(const rc_bitset *bs)
{
    return rc_bitset_next_set(bs, 0);
}

#endif // RC_BITSET_H_
