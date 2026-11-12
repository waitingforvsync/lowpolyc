// genpool.template.h
//
// Template for a generational pool: a pool whose slots count their lifetimes, and which hands out
// handles rather than bare indices. A handle carries its slot's generation, and removing the
// element moves the generation on, so a handle that outlives its element is caught rather than
// quietly reaching whatever takes the slot next.
//
//   RC_GENPOOL_TYPE    the element type (required)
//   RC_GENPOOL_NAME    the name suffix; defaults to RC_GENPOOL_TYPE, which must then be one
//                      identifier
//   RC_GENPOOL_HANDLE  the handle type, made with handle.template.h; defaults to rc_handle_<name>,
//                      which this generates
//
// The pool is rc_genpool_<name>, and {0} is an empty pool. Like a pool, freed slots are reused
// first. Everything that takes a handle asserts it is valid; is_valid is the check that does not.
//
// add and remove each move a slot's generation on by one, so a slot is live exactly when its
// generation is odd. Handles only carry odd generations, which keeps {0} free to mean none. To
// visit the live elements, test each slot below num_slots with is_live, and take handle_at.
//
// reset and release start the generations again from 0, so a handle from before can match one
// from after. A slot's generation also wraps after 2^31 lifetimes. Neither is guarded against.

// ---- shared by every instantiation ----

#ifndef RC_GENPOOL_TEMPLATE_H_
#define RC_GENPOOL_TEMPLATE_H_

#include "richc/arena.h"

#endif // RC_GENPOOL_TEMPLATE_H_

// ---- per-type names ----

// The fallback type keeps IntelliSense happy when it parses this file on its own
#ifndef RC_GENPOOL_TYPE
#  define RC_GENPOOL_TYPE int
#  error "RC_GENPOOL_TYPE must be defined before including richc/genpool.template.h"
#endif

#ifndef RC_GENPOOL_NAME
#  define RC_GENPOOL_NAME RC_GENPOOL_TYPE
#endif

#ifndef RC_GENPOOL_HANDLE
#  define RC_GENPOOL_HANDLE RC_CONCAT(rc_handle_, RC_GENPOOL_NAME)
#  define RC_HANDLE_TYPE RC_GENPOOL_HANDLE
#  include "richc/handle.template.h"
#endif

#define RC_GENPOOL_ RC_CONCAT(rc_genpool_, RC_GENPOOL_NAME)
#define RC_GENPOOL_SLOT_ RC_CONCAT(RC_GENPOOL_, _slot_)
#define RC_GENPOOL_SLOTS_ RC_CONCAT(rc_array_genpool_, RC_CONCAT(RC_GENPOOL_NAME, _slot_))

#define RC_GENPOOL_MAKE_ RC_CONCAT(RC_GENPOOL_, _make)
#define RC_GENPOOL_RESERVE_ RC_CONCAT(RC_GENPOOL_, _reserve)
#define RC_GENPOOL_RESET_ RC_CONCAT(RC_GENPOOL_, _reset)
#define RC_GENPOOL_RELEASE_ RC_CONCAT(RC_GENPOOL_, _release)
#define RC_GENPOOL_IS_VALID_ RC_CONCAT(RC_GENPOOL_, _is_valid)
#define RC_GENPOOL_ADD_ RC_CONCAT(RC_GENPOOL_, _add)
#define RC_GENPOOL_REMOVE_ RC_CONCAT(RC_GENPOOL_, _remove)
#define RC_GENPOOL_GET_ RC_CONCAT(RC_GENPOOL_, _get)
#define RC_GENPOOL_SET_ RC_CONCAT(RC_GENPOOL_, _set)
#define RC_GENPOOL_AT_ RC_CONCAT(RC_GENPOOL_, _at)
#define RC_GENPOOL_AT_CONST_ RC_CONCAT(RC_GENPOOL_, _at_const)
#define RC_GENPOOL_NUM_SLOTS_ RC_CONCAT(RC_GENPOOL_, _num_slots)
#define RC_GENPOOL_IS_LIVE_ RC_CONCAT(RC_GENPOOL_, _is_live)
#define RC_GENPOOL_HANDLE_AT_ RC_CONCAT(RC_GENPOOL_, _handle_at)

#define RC_GENPOOL_SLOTS_MAKE_ RC_CONCAT(RC_GENPOOL_SLOTS_, _make)
#define RC_GENPOOL_SLOTS_RESERVE_ RC_CONCAT(RC_GENPOOL_SLOTS_, _reserve)
#define RC_GENPOOL_SLOTS_RESET_ RC_CONCAT(RC_GENPOOL_SLOTS_, _reset)
#define RC_GENPOOL_SLOTS_RELEASE_ RC_CONCAT(RC_GENPOOL_SLOTS_, _release)
#define RC_GENPOOL_SLOTS_PUSH_ RC_CONCAT(RC_GENPOOL_SLOTS_, _push)

// ---- types ----

// An element, or while the slot is free, the link to the next free slot; and the generation,
// which outlives both.
typedef struct RC_GENPOOL_SLOT_ {
    union {
        uint32_t next_free_;
        RC_GENPOOL_TYPE value;
    };
    uint32_t gen;  // odd while the slot is live
} RC_GENPOOL_SLOT_;

#define RC_ARRAY_TYPE RC_GENPOOL_SLOT_
#define RC_ARRAY_NAME RC_CONCAT(genpool_, RC_CONCAT(RC_GENPOOL_NAME, _slot_))
#include "richc/array.template.h"

typedef struct RC_GENPOOL_ {
    RC_GENPOOL_SLOTS_ slots_;
    uint32_t first_free_;  // the first free slot's index plus one; 0 if none is free
} RC_GENPOOL_;

// ---- lifecycle ----

// An empty pool with room for capacity elements. Only needed to reserve room up front.
static inline RC_GENPOOL_ RC_GENPOOL_MAKE_(uint32_t capacity, rc_arena *arena)
{
    return (RC_GENPOOL_) {.slots_ = RC_GENPOOL_SLOTS_MAKE_(capacity, arena)};
}

// Make room for at least capacity elements, exactly; never shrinks.
static inline void RC_GENPOOL_RESERVE_(RC_GENPOOL_ *pool, uint32_t capacity, rc_arena *arena)
{
    RC_GENPOOL_SLOTS_RESERVE_(&pool->slots_, capacity, arena);
}

// Remove every element, keeping the slots' memory. Generations start again from 0.
static inline void RC_GENPOOL_RESET_(RC_GENPOOL_ *pool)
{
    RC_GENPOOL_SLOTS_RESET_(&pool->slots_);
    pool->first_free_ = 0;
}

// Free the slots (if they are the arena's last allocation) and zero the pool.
static inline void RC_GENPOOL_RELEASE_(RC_GENPOOL_ *pool, rc_arena *arena)
{
    RC_GENPOOL_SLOTS_RELEASE_(&pool->slots_, arena);
    pool->first_free_ = 0;
}

// ---- handles ----

// True when handle names a live element of this pool: an odd generation, in range, and matching
// the slot's current one.
static inline bool RC_GENPOOL_IS_VALID_(const RC_GENPOOL_ *pool, RC_GENPOOL_HANDLE handle)
{
    return (handle.gen_ & 1u) != 0 && handle.index_ < pool->slots_.num
        && pool->slots_.data[handle.index_].gen == handle.gen_;
}

// ---- add and remove ----

// Add value, returning its handle: the most recently freed slot, or else a new one at the end.
static inline RC_GENPOOL_HANDLE RC_GENPOOL_ADD_(RC_GENPOOL_ *pool, RC_GENPOOL_TYPE value, rc_arena *arena)
{
    if (pool->first_free_ == 0) {
        RC_GENPOOL_SLOT_ slot = {.value = value, .gen = 1};
        uint32_t index = RC_GENPOOL_SLOTS_PUSH_(&pool->slots_, slot, arena);

        return (RC_GENPOOL_HANDLE) {.index_ = index, .gen_ = 1};
    }

    uint32_t index = pool->first_free_ - 1;
    RC_GENPOOL_SLOT_ *slot = &pool->slots_.data[index];
    pool->first_free_ = slot->next_free_;
    slot->value = value;
    slot->gen++;
    return (RC_GENPOOL_HANDLE) {.index_ = index, .gen_ = slot->gen};
}

// Remove the element, moving its slot's generation on so every handle to it goes stale. Asserts
// the handle is valid, which catches removing twice.
static inline void RC_GENPOOL_REMOVE_(RC_GENPOOL_ *pool, RC_GENPOOL_HANDLE handle)
{
    RC_ASSERT(RC_GENPOOL_IS_VALID_(pool, handle));

    RC_GENPOOL_SLOT_ *slot = &pool->slots_.data[handle.index_];
    slot->gen++;
    slot->next_free_ = pool->first_free_;
    pool->first_free_ = handle.index_ + 1;
}

// ---- access ----

// The element, by value.
static inline RC_GENPOOL_TYPE RC_GENPOOL_GET_(const RC_GENPOOL_ *pool, RC_GENPOOL_HANDLE handle)
{
    RC_ASSERT(RC_GENPOOL_IS_VALID_(pool, handle));
    return pool->slots_.data[handle.index_].value;
}

// Overwrite the element.
static inline void RC_GENPOOL_SET_(RC_GENPOOL_ *pool, RC_GENPOOL_HANDLE handle, RC_GENPOOL_TYPE value)
{
    RC_ASSERT(RC_GENPOOL_IS_VALID_(pool, handle));
    pool->slots_.data[handle.index_].value = value;
}

// A pointer to the element. Anything that grows the pool may move it.
static inline RC_GENPOOL_TYPE *RC_GENPOOL_AT_(RC_GENPOOL_ *pool, RC_GENPOOL_HANDLE handle)
{
    RC_ASSERT(RC_GENPOOL_IS_VALID_(pool, handle));
    return &pool->slots_.data[handle.index_].value;
}

// Like at, through a read-only pool.
static inline const RC_GENPOOL_TYPE *RC_GENPOOL_AT_CONST_(const RC_GENPOOL_ *pool, RC_GENPOOL_HANDLE handle)
{
    RC_ASSERT(RC_GENPOOL_IS_VALID_(pool, handle));
    return &pool->slots_.data[handle.index_].value;
}

// ---- iteration ----

// The number of slots, live and free: every index below it is a slot.
static inline uint32_t RC_GENPOOL_NUM_SLOTS_(const RC_GENPOOL_ *pool)
{
    return pool->slots_.num;
}

// True when slot index holds an element.
static inline bool RC_GENPOOL_IS_LIVE_(const RC_GENPOOL_ *pool, uint32_t index)
{
    RC_ASSERT(index < pool->slots_.num);
    return (pool->slots_.data[index].gen & 1u) != 0;
}

// The handle to the element in slot index, which must be live.
static inline RC_GENPOOL_HANDLE RC_GENPOOL_HANDLE_AT_(const RC_GENPOOL_ *pool, uint32_t index)
{
    RC_ASSERT(RC_GENPOOL_IS_LIVE_(pool, index));
    return (RC_GENPOOL_HANDLE) {.index_ = index, .gen_ = pool->slots_.data[index].gen};
}

// ---- cleanup ----

#undef RC_GENPOOL_MAKE_
#undef RC_GENPOOL_RESERVE_
#undef RC_GENPOOL_RESET_
#undef RC_GENPOOL_RELEASE_
#undef RC_GENPOOL_IS_VALID_
#undef RC_GENPOOL_ADD_
#undef RC_GENPOOL_REMOVE_
#undef RC_GENPOOL_GET_
#undef RC_GENPOOL_SET_
#undef RC_GENPOOL_AT_
#undef RC_GENPOOL_AT_CONST_
#undef RC_GENPOOL_NUM_SLOTS_
#undef RC_GENPOOL_IS_LIVE_
#undef RC_GENPOOL_HANDLE_AT_

#undef RC_GENPOOL_SLOTS_MAKE_
#undef RC_GENPOOL_SLOTS_RESERVE_
#undef RC_GENPOOL_SLOTS_RESET_
#undef RC_GENPOOL_SLOTS_RELEASE_
#undef RC_GENPOOL_SLOTS_PUSH_

#undef RC_GENPOOL_
#undef RC_GENPOOL_SLOT_
#undef RC_GENPOOL_SLOTS_

#undef RC_GENPOOL_HANDLE
#undef RC_GENPOOL_NAME
#undef RC_GENPOOL_TYPE
