// hash_index.c
//
// The out-of-line half of rc_hash_index: sizing, growing and removing.

#include "richc/hash_index.h"

#include "richc/mem.h"

// The slots needed to hold num_keys entries: a power of two, at least RC_HASH_INDEX_MIN_CAP_.
static uint32_t rc_hash_index_cap_for(uint32_t num_keys)
{
    uint32_t cap = RC_HASH_INDEX_MIN_CAP_;

    while (rc_hash_index_max_num_(cap) < num_keys) {
        RC_PANIC(cap < RC_HASH_INDEX_MAX_CAP_);
        cap *= 2;
    }
    return cap;
}


// Move each entry of the first old_cap slots to its place among new_cap, where the slots past
// old_cap are empty.
//
// We take each entry out of its slot and put it back from its new home. The walk starts just
// after an empty slot, so it meets each run of keys at its start. Each entry's search for a slot
// then crosses only slots already walked and slots past old_cap, never one still to walk, which
// could empty later and cut the entry off from its home. This holds for growth by any power of
// two, since the slots past old_cap outnumber the entries.
static void rc_hash_index_rehash(rc_hash_index_slot_ *slots, uint32_t old_cap, uint32_t new_cap)
{
    uint32_t start = 0;

    // At most 3/4 of the slots are used, so there is an empty one
    while (slots[start].hash != 0) {
        start++;
    }

    for (uint32_t step = 1; step < old_cap; step++) {
        uint32_t slot = (start + step) & (old_cap - 1);
        rc_hash_index_slot_ entry = slots[slot];

        if (entry.hash != 0) {
            slots[slot] = (rc_hash_index_slot_) {0};
            rc_hash_index_place_(slots, new_cap - 1, entry.hash, entry.index);
        }
    }
}


// The slot holding the entry for index under the stored hash, or RC_INDEX_NONE.
static uint32_t rc_hash_index_slot_of(rc_hash_index table, uint32_t stored, uint32_t index)
{
    uint32_t mask = table.cap - 1;
    uint32_t slot = stored & mask;

    if (table.cap == 0) {
        return RC_INDEX_NONE;
    }

    while (table.slots_[slot].hash != 0) {
        if (table.slots_[slot].hash == stored && table.slots_[slot].index == index) {
            return slot;
        }

        slot = (slot + 1) & mask;
    }
    return RC_INDEX_NONE;
}


// The table moved to new_cap slots. The new slots come zeroed, so empty, and the old ones keep
// their place at the start until the rehash.
static rc_hash_index rc_hash_index_grown(rc_hash_index table, uint32_t new_cap, rc_arena *arena)
{
    uint32_t old_size = table.cap * (uint32_t)sizeof(rc_hash_index_slot_);
    uint32_t new_size = new_cap * (uint32_t)sizeof(rc_hash_index_slot_);
    rc_hash_index_slot_ *slots = (rc_hash_index_slot_ *)rc_arena_realloc_zero(arena, table.slots_, old_size, new_size);

    if (table.num > 0) {
        rc_hash_index_rehash(slots, table.cap, new_cap);
    }

    return (rc_hash_index) {
        .slots_ = slots,
        .num = table.num,
        .cap = new_cap
    };
}


// True if gap lies between home and slot, going round the table, so an entry at slot from home
// may move back into it.
static bool rc_hash_index_is_between(uint32_t home, uint32_t gap, uint32_t slot, uint32_t mask)
{
    return ((slot - home) & mask) >= ((slot - gap) & mask);
}


// Empty the slot gap, then walk on to the end of its run, closing the gap. An entry moves back into
// the gap when the gap lies between its home and its slot, and then its old slot is the gap.
static void rc_hash_index_empty_slot(rc_hash_index table, uint32_t gap)
{
    uint32_t mask = table.cap - 1;
    uint32_t slot = (gap + 1) & mask;

    while (table.slots_[slot].hash != 0) {
        uint32_t home = table.slots_[slot].hash & mask;

        if (rc_hash_index_is_between(home, gap, slot, mask)) {
            table.slots_[gap] = table.slots_[slot];
            gap = slot;
        }

        slot = (slot + 1) & mask;
    }
    table.slots_[gap] = (rc_hash_index_slot_) {0};
}


rc_hash_index rc_hash_index_make(uint32_t num_keys, rc_arena *arena)
{
    rc_hash_index table = {0};

    rc_hash_index_reserve(&table, num_keys, arena);
    return table;
}


void rc_hash_index_reserve(rc_hash_index *table, uint32_t num_keys, rc_arena *arena)
{
    RC_ASSERT(table);
    if (num_keys <= rc_hash_index_max_num_(table->cap)) {
        return;
    }

    RC_ASSERT(arena);
    *table = rc_hash_index_grown(*table, rc_hash_index_cap_for(num_keys), arena);
}


void rc_hash_index_reset(rc_hash_index *table)
{
    RC_ASSERT(table);
    if (table->slots_) {
        rc_memset(table->slots_, 0, table->cap * (uint32_t)sizeof(rc_hash_index_slot_));
    }

    table->num = 0;
}


void rc_hash_index_release(rc_hash_index *table, rc_arena *arena)
{
    RC_ASSERT(table);
    if (table->slots_) {
        rc_arena_free(arena, table->slots_, table->cap * (uint32_t)sizeof(rc_hash_index_slot_));
    }

    *table = (rc_hash_index) {0};
}


bool rc_hash_index_remove(rc_hash_index *table, uint32_t hash, uint32_t index)
{
    uint32_t slot = rc_hash_index_slot_of(*table, rc_hash_index_stored_(hash), index);

    if (slot == RC_INDEX_NONE) {
        return false;
    }

    rc_hash_index_empty_slot(*table, slot);
    table->num--;
    return true;
}


bool rc_hash_index_move(rc_hash_index *table, uint32_t hash, uint32_t from, uint32_t to)
{
    uint32_t slot = rc_hash_index_slot_of(*table, rc_hash_index_stored_(hash), from);

    if (slot == RC_INDEX_NONE) {
        return false;
    }

    table->slots_[slot].index = to;
    return true;
}
