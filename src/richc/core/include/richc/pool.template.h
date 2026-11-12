// pool.template.h
//
// Template for a pool: elements at stable indices, with removed elements' slots reused before the
// pool grows.
//
//   RC_POOL_TYPE  the element type (required)
//   RC_POOL_NAME  the name suffix; defaults to RC_POOL_TYPE, which must then be one identifier
//
// The pool is rc_pool_<name>, and {0} is an empty pool. add returns the new element's index, which
// stays valid until the element is removed, however much the pool grows. Nothing marks a removed
// element's slot, so using a stale index goes unnoticed; a genpool catches that.
//
// The free slots form a list threaded through the slots themselves. Each link is the next slot's
// index plus one, so 0 can end the list and a zeroed pool's list is empty. add and remove are
// exact inverses: add then remove leaves the pool as it was.
// The slots never shrink until reset or release.
//
// Only the free list knows which slots are live. To visit the live elements, ask free_bitset for
// the free slots, and take the clear bits.

// ---- shared by every instantiation ----

#ifndef RC_POOL_TEMPLATE_H_
#define RC_POOL_TEMPLATE_H_

#include "richc/arena.h"
#include "richc/bitset.h"

#endif // RC_POOL_TEMPLATE_H_

// ---- per-type names ----

// The fallback type keeps IntelliSense happy when it parses this file on its own
#ifndef RC_POOL_TYPE
#  define RC_POOL_TYPE int
#  error "RC_POOL_TYPE must be defined before including richc/pool.template.h"
#endif

#ifndef RC_POOL_NAME
#  define RC_POOL_NAME RC_POOL_TYPE
#endif

#define RC_POOL_ RC_CONCAT(rc_pool_, RC_POOL_NAME)
#define RC_POOL_SLOT_ RC_CONCAT(RC_POOL_, _slot_)
#define RC_POOL_SLOTS_ RC_CONCAT(rc_array_pool_, RC_CONCAT(RC_POOL_NAME, _slot_))

#define RC_POOL_MAKE_ RC_CONCAT(RC_POOL_, _make)
#define RC_POOL_RESERVE_ RC_CONCAT(RC_POOL_, _reserve)
#define RC_POOL_RESET_ RC_CONCAT(RC_POOL_, _reset)
#define RC_POOL_RELEASE_ RC_CONCAT(RC_POOL_, _release)
#define RC_POOL_NUM_SLOTS_ RC_CONCAT(RC_POOL_, _num_slots)
#define RC_POOL_ADD_ RC_CONCAT(RC_POOL_, _add)
#define RC_POOL_REMOVE_ RC_CONCAT(RC_POOL_, _remove)
#define RC_POOL_GET_ RC_CONCAT(RC_POOL_, _get)
#define RC_POOL_SET_ RC_CONCAT(RC_POOL_, _set)
#define RC_POOL_AT_ RC_CONCAT(RC_POOL_, _at)
#define RC_POOL_AT_CONST_ RC_CONCAT(RC_POOL_, _at_const)
#define RC_POOL_FREE_BITSET_ RC_CONCAT(RC_POOL_, _free_bitset)

#define RC_POOL_SLOTS_MAKE_ RC_CONCAT(RC_POOL_SLOTS_, _make)
#define RC_POOL_SLOTS_RESERVE_ RC_CONCAT(RC_POOL_SLOTS_, _reserve)
#define RC_POOL_SLOTS_RESET_ RC_CONCAT(RC_POOL_SLOTS_, _reset)
#define RC_POOL_SLOTS_RELEASE_ RC_CONCAT(RC_POOL_SLOTS_, _release)
#define RC_POOL_SLOTS_PUSH_ RC_CONCAT(RC_POOL_SLOTS_, _push)

// ---- types ----

// An element, or while the slot is free, the link to the next free slot.
typedef union RC_POOL_SLOT_ {
    uint32_t next_free_;
    RC_POOL_TYPE value;
} RC_POOL_SLOT_;

#define RC_ARRAY_TYPE RC_POOL_SLOT_
#define RC_ARRAY_NAME RC_CONCAT(pool_, RC_CONCAT(RC_POOL_NAME, _slot_))
#include "richc/array.template.h"

typedef struct RC_POOL_ {
    RC_POOL_SLOTS_ slots_;
    uint32_t first_free_;  // the first free slot's index plus one; 0 if none is free
} RC_POOL_;

// ---- lifecycle ----

// An empty pool with room for capacity elements. Only needed to reserve room up front.
static inline RC_POOL_ RC_POOL_MAKE_(uint32_t capacity, rc_arena *arena)
{
    return (RC_POOL_) {.slots_ = RC_POOL_SLOTS_MAKE_(capacity, arena)};
}

// Make room for at least capacity elements, exactly; never shrinks.
static inline void RC_POOL_RESERVE_(RC_POOL_ *pool, uint32_t capacity, rc_arena *arena)
{
    RC_POOL_SLOTS_RESERVE_(&pool->slots_, capacity, arena);
}

// Remove every element, keeping the slots' memory. Every index becomes invalid.
static inline void RC_POOL_RESET_(RC_POOL_ *pool)
{
    RC_POOL_SLOTS_RESET_(&pool->slots_);
    pool->first_free_ = 0;
}

// Free the slots (if they are the arena's last allocation) and zero the pool.
static inline void RC_POOL_RELEASE_(RC_POOL_ *pool, rc_arena *arena)
{
    RC_POOL_SLOTS_RELEASE_(&pool->slots_, arena);
    pool->first_free_ = 0;
}

// ---- add and remove ----

// Add value, returning its index: the most recently freed slot, or else a new one at the end.
static inline uint32_t RC_POOL_ADD_(RC_POOL_ *pool, RC_POOL_TYPE value, rc_arena *arena)
{
    if (pool->first_free_ == 0) {
        return RC_POOL_SLOTS_PUSH_(&pool->slots_, (RC_POOL_SLOT_) {.value = value}, arena);
    }

    uint32_t index = pool->first_free_ - 1;
    RC_POOL_SLOT_ *slot = &pool->slots_.data[index];
    pool->first_free_ = slot->next_free_;
    slot->value = value;
    return index;
}

// Remove the element at index, freeing its slot. Removing from a free slot corrupts the pool, and
// is not detected.
static inline void RC_POOL_REMOVE_(RC_POOL_ *pool, uint32_t index)
{
    RC_ASSERT(index < pool->slots_.num);
    pool->slots_.data[index].next_free_ = pool->first_free_;
    pool->first_free_ = index + 1;
}

// ---- access ----

// The element at index, by value.
static inline RC_POOL_TYPE RC_POOL_GET_(const RC_POOL_ *pool, uint32_t index)
{
    RC_ASSERT(index < pool->slots_.num);
    return pool->slots_.data[index].value;
}

// Overwrite the element at index.
static inline void RC_POOL_SET_(RC_POOL_ *pool, uint32_t index, RC_POOL_TYPE value)
{
    RC_ASSERT(index < pool->slots_.num);
    pool->slots_.data[index].value = value;
}

// A pointer to the element at index. Anything that grows the pool may move it.
static inline RC_POOL_TYPE *RC_POOL_AT_(RC_POOL_ *pool, uint32_t index)
{
    RC_ASSERT(index < pool->slots_.num);
    return &pool->slots_.data[index].value;
}

// Like at, through a read-only pool.
static inline const RC_POOL_TYPE *RC_POOL_AT_CONST_(const RC_POOL_ *pool, uint32_t index)
{
    RC_ASSERT(index < pool->slots_.num);
    return &pool->slots_.data[index].value;
}

// ---- iteration ----

// The number of slots, live and free: every index below it is a slot.
static inline uint32_t RC_POOL_NUM_SLOTS_(const RC_POOL_ *pool)
{
    return pool->slots_.num;
}

// A bitset of the slots, from arena, with the free ones set: the clear bits are the live elements.
static inline rc_bitset RC_POOL_FREE_BITSET_(const RC_POOL_ *pool, rc_arena *arena)
{
    rc_bitset free_slots = rc_bitset_make(pool->slots_.num, arena);

    for (uint32_t link = pool->first_free_; link != 0; link = pool->slots_.data[link - 1].next_free_) {
        rc_bitset_set(&free_slots, link - 1);
    }
    return free_slots;
}

// ---- cleanup ----

#undef RC_POOL_MAKE_
#undef RC_POOL_RESERVE_
#undef RC_POOL_RESET_
#undef RC_POOL_RELEASE_
#undef RC_POOL_NUM_SLOTS_
#undef RC_POOL_ADD_
#undef RC_POOL_REMOVE_
#undef RC_POOL_GET_
#undef RC_POOL_SET_
#undef RC_POOL_AT_
#undef RC_POOL_AT_CONST_
#undef RC_POOL_FREE_BITSET_

#undef RC_POOL_SLOTS_MAKE_
#undef RC_POOL_SLOTS_RESERVE_
#undef RC_POOL_SLOTS_RESET_
#undef RC_POOL_SLOTS_RELEASE_
#undef RC_POOL_SLOTS_PUSH_

#undef RC_POOL_
#undef RC_POOL_SLOT_
#undef RC_POOL_SLOTS_

#undef RC_POOL_NAME
#undef RC_POOL_TYPE
