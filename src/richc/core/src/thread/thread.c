// thread.c
//
// The table of threads behind rc_thread. A slot's generation is odd while a thread holds it, and
// goes up by one when a starter claims it and again when the thread is joined, so a handle to a
// joined thread never matches the slot's next thread. Claiming is a compare-exchange on the
// generation, so the table needs no lock, and starting a thread allocates nothing.

#include "richc/thread/thread.h"

#include "platform/os_thread.h"
#include "richc/thread/atomic.h"

// One thread's slot. Only the thread that claimed it writes fn, context and handle.
typedef struct rc_thread_slot {
    void (*fn)(void *);
    void *context;
    uintptr_t handle;    // the OS's handle, once it has started
    rc_atomic_u32 gen;   // odd while in use
} rc_thread_slot;

static rc_thread_slot rc_thread_slots[RC_THREAD_MAX];

// Its address is the thread's id, as each thread has its own.
static RC_THREAD_LOCAL uint8_t rc_thread_marker;


// Claim a free slot for fn(context), returning its index, or RC_INDEX_NONE if all are in use.
static uint32_t rc_thread_take_slot(void (*fn)(void *), void *context)
{
    for (uint32_t i = 0; i < RC_THREAD_MAX; i++) {
        rc_thread_slot *slot = &rc_thread_slots[i];
        uint32_t gen = rc_atomic_u32_load(&slot->gen, RC_MEMORY_ORDER_RELAXED);

        if ((gen & 1u) != 0) {
            continue;
        }

        // Another starter may claim it first
        if (rc_atomic_u32_compare_exchange(&slot->gen, gen, gen + 1, RC_MEMORY_ORDER_ACQUIRE) != gen) {
            continue;
        }

        slot->fn = fn;
        slot->context = context;
        slot->handle = 0;
        return i;
    }

    return RC_INDEX_NONE;
}


// Give back the slot at index.
static void rc_thread_free_slot(uint32_t index)
{
    (void)rc_atomic_u32_fetch_add(&rc_thread_slots[index].gen, 1, RC_MEMORY_ORDER_RELEASE);
}


// Start the thread in the slot at index, which this thread has just claimed.
static rc_thread rc_thread_start_slot(uint32_t index)
{
    rc_thread_slot *slot = &rc_thread_slots[index];
    uintptr_t handle = rc_os_thread_start(index, RC_THREAD_STACK_SIZE);

    if (handle == 0) {
        rc_thread_free_slot(index);
        return (rc_thread) {0};
    }

    // Only the joiner reads the handle, after whatever handed it the rc_thread
    slot->handle = handle;
    return (rc_thread) {
        .index_ = index,
        .gen_ = rc_atomic_u32_load(&slot->gen, RC_MEMORY_ORDER_RELAXED)
    };
}


void rc_thread_run_(uint32_t index)
{
    const rc_thread_slot *slot = &rc_thread_slots[index];

    slot->fn(slot->context);
}


rc_thread rc_thread_make(void (*fn)(void *), void *context)
{
    uint32_t index = rc_thread_take_slot(fn, context);

    if (index == RC_INDEX_NONE) {
        return (rc_thread) {0};
    }

    return rc_thread_start_slot(index);
}


bool rc_thread_is_valid(rc_thread thread)
{
    if ((thread.gen_ & 1u) == 0) {
        return false;
    }

    if (thread.index_ >= RC_THREAD_MAX) {
        return false;
    }

    return rc_atomic_u32_load(&rc_thread_slots[thread.index_].gen, RC_MEMORY_ORDER_ACQUIRE) == thread.gen_;
}


void rc_thread_join(rc_thread *thread)
{
    RC_ASSERT(rc_thread_is_valid(*thread));
    rc_os_thread_join(rc_thread_slots[thread->index_].handle);
    rc_thread_free_slot(thread->index_);
    *thread = (rc_thread) {0};
}


uint64_t rc_thread_current_id(void)
{
    return (uint64_t)(uintptr_t)&rc_thread_marker;
}


void rc_thread_yield(void)
{
    rc_os_thread_yield();
}


void rc_thread_sleep_ns(uint64_t ns)
{
    rc_os_thread_sleep_ns(ns);
}


uint32_t rc_thread_hardware_concurrency(void)
{
    return rc_os_thread_core_count();
}
