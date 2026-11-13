// hash_index.h
//
// A hash index: a table that finds things in your own arrays by key. It holds only each key's
// hash and its index into your data, never the key itself, so the keys and anything kept with
// them stay in your arrays, laid out however suits you: one array of {key, value} structs, or a
// keys array with values in arrays of their own.
//
// The table is one allocation of 8-byte slots, of which at most 3/4 are used. Growing reallocates
// it, in place when it is its arena's last allocation, and rehashes in place. An add that need not
// grow takes a NULL arena.
//
// Sizing: growth costs little, and a growing table stays small, which the cache likes. A table
// that stays small (a few MiB at most) can be made generously big up front, since it fits in the
// cache anyway and a sparser table finds keys sooner. A big one does better left to grow. If other
// allocations would follow it, give it an arena of its own, made with a chunk as big as the table
// can get, so it always grows in place. Natively, pages never written cost no memory.
//
// This is the untyped core, dealing in hashes and indices. A lookup is a probe, which yields the
// indices whose hash matches for the caller to compare keys:
//
//   rc_hash_index_probe probe = rc_hash_index_probe_make(table, hash);
//   uint32_t i = rc_hash_index_probe_next(table, &probe);
//   while (i != RC_INDEX_NONE && !vec3_is_equal(rc_view_vec3_get(positions, i), key)) {
//       i = rc_hash_index_probe_next(table, &probe);
//   }
//
// hash_index.template.h builds typed lookups on top, reading the keys from your data. Nothing
// stops two entries sharing a key, so a table can also serve as a multimap.
//
// How it works
//
// A slot holds a hash and an index, and hash 0 marks it empty: a hash of 0 is stored as 1, and
// zeroed memory is an empty table. A key's home is the slot the low bits of its hash pick, and it
// takes the first empty slot from there on, wrapping at the end (linear probing). Lookups compare
// the stored hashes, so the caller rarely compares a key that does not match.
//
// Removing a key empties its slot, then moves back each later key in the same run whose home
// allows it, so no key is ever cut off from its home by a gap, and removals leave no markers to
// slow lookups down.

#ifndef RC_HASH_INDEX_H_
#define RC_HASH_INDEX_H_

#include "richc/arena.h"

// The smallest table, a cache line of slots, and the largest, whose size in bytes is 2 GiB
#define RC_HASH_INDEX_MIN_CAP_ 8u
#define RC_HASH_INDEX_MAX_CAP_ 0x10000000u

// A slot: the stored hash, 0 when empty, and the index into your data.
typedef struct rc_hash_index_slot_ {
    uint32_t hash;
    uint32_t index;
} rc_hash_index_slot_;

// {0} is an empty table. Lookups take it by value, but a copy goes stale once the table changes.
typedef struct rc_hash_index {
    rc_hash_index_slot_ *slots_;
    uint32_t num;  // entries
    uint32_t cap;  // slots: 0, or a power of two from RC_HASH_INDEX_MIN_CAP_
} rc_hash_index;

// A lookup in progress. Anything that changes the table ends it.
typedef struct rc_hash_index_probe {
    uint32_t hash_;  // the stored hash sought; 0 once the probe has reached an empty slot
    uint32_t slot_;  // the next slot to look at
} rc_hash_index_probe;

// ---- internal ----

// The hash as stored: 0 means empty, so it becomes 1.
static inline uint32_t rc_hash_index_stored_(uint32_t hash)
{
    return hash != 0 ? hash : 1u;
}

// The most entries cap slots may hold: 3/4 of them.
static inline uint32_t rc_hash_index_max_num_(uint32_t cap)
{
    return cap - cap / 4;
}

// Put an entry in the first empty slot from its home. There must be one.
static inline void rc_hash_index_place_(rc_hash_index_slot_ *slots, uint32_t mask, uint32_t stored, uint32_t index)
{
    uint32_t slot = stored & mask;

    while (slots[slot].hash != 0) {
        slot = (slot + 1) & mask;
    }
    slots[slot] = (rc_hash_index_slot_) {.hash = stored, .index = index};
}

// ---- the table ----

// An empty table with room for num_keys entries before it grows. 0 allocates nothing, so the
// arena may then be NULL.
rc_hash_index rc_hash_index_make(uint32_t num_keys, rc_arena *arena);

// Make room for num_keys entries in all; never shrinks. Panics (in release too) if that would take
// more than RC_HASH_INDEX_MAX_CAP_ slots.
void rc_hash_index_reserve(rc_hash_index *table, uint32_t num_keys, rc_arena *arena);

// Remove every entry, keeping the slots.
void rc_hash_index_reset(rc_hash_index *table);

// Free the slots (if they are the arena's last allocation) and zero the table.
void rc_hash_index_release(rc_hash_index *table, rc_arena *arena);

// ---- entries ----

// Add an entry for index under hash, growing first if it would make the table over 3/4 full. It
// does not look for an entry already there.
static inline void rc_hash_index_add(rc_hash_index *table, uint32_t hash, uint32_t index, rc_arena *arena)
{
    RC_ASSERT(table);
    if (table->num >= rc_hash_index_max_num_(table->cap)) {
        rc_hash_index_reserve(table, table->num + 1, arena);
    }
    rc_hash_index_place_(table->slots_, table->cap - 1, rc_hash_index_stored_(hash), index);
    table->num++;
}

// Start a lookup of hash.
static inline rc_hash_index_probe rc_hash_index_probe_make(rc_hash_index table, uint32_t hash)
{
    uint32_t stored = rc_hash_index_stored_(hash);

    if (table.cap == 0) {
        return (rc_hash_index_probe) {0};
    }
    return (rc_hash_index_probe) {
        .hash_ = stored,
        .slot_ = stored & (table.cap - 1)
    };
}

// The next index whose entry has the probe's hash, or RC_INDEX_NONE once there are no more. Each
// comes up once. Since 0 is stored as 1, hashes 0 and 1 match each other's entries.
static inline uint32_t rc_hash_index_probe_next(rc_hash_index table, rc_hash_index_probe *probe)
{
    RC_ASSERT(probe);
    while (probe->hash_ != 0) {
        rc_hash_index_slot_ slot = table.slots_[probe->slot_];

        probe->slot_ = (probe->slot_ + 1) & (table.cap - 1);
        if (slot.hash == 0) {
            probe->hash_ = 0;
        }
        else if (slot.hash == probe->hash_) {
            return slot.index;
        }
    }
    return RC_INDEX_NONE;
}

// Remove the entry for index under hash: true if it was there.
bool rc_hash_index_remove(rc_hash_index *table, uint32_t hash, uint32_t index);

// Point the entry for from under hash at to instead: true if it was there. For when data moves,
// as when you swap-remove from an array.
bool rc_hash_index_move(rc_hash_index *table, uint32_t hash, uint32_t from, uint32_t to);

#endif // RC_HASH_INDEX_H_
