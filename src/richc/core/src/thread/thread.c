// thread.c
//
// The table of threads behind rc_thread. A slot's generation is odd while a thread holds it and
// goes up by one when the thread starts and again when it is joined, so a handle to a joined
// thread never matches the slot's next thread. The table is static and taken under a mutex, so
// starting a thread allocates nothing.

#include "richc/thread/thread.h"

#include "platform/os_thread.h"
#include "richc/thread/mutex.h"

// One thread's slot.
typedef struct rc_thread_slot {
    void (*fn)(void *);
    void *context;
    uintptr_t handle;   // the OS's handle, 0 while it is starting
    uint32_t gen;       // odd while in use
} rc_thread_slot;

static rc_thread_slot rc_thread_slots[RC_THREAD_MAX];
static rc_mutex rc_thread_slots_lock;

// Its address is the thread's id, as each thread has its own.
static RC_THREAD_LOCAL uint8_t rc_thread_marker;


// Take a free slot for fn(context), returning its index, or RC_INDEX_NONE if all are in use.
static uint32_t rc_thread_take_slot(void (*fn)(void *), void *context)
{
    uint32_t index = RC_INDEX_NONE;

    rc_mutex_lock(&rc_thread_slots_lock);
    for (uint32_t i = 0; i < RC_THREAD_MAX; i++) {
        rc_thread_slot *slot = &rc_thread_slots[i];

        if ((slot->gen & 1u) == 0) {
            slot->fn = fn;
            slot->context = context;
            slot->handle = 0;
            slot->gen++;
            index = i;
            break;
        }
    }

    rc_mutex_unlock(&rc_thread_slots_lock);
    return index;
}


// Give back the slot at index.
static void rc_thread_free_slot(uint32_t index)
{
    rc_mutex_lock(&rc_thread_slots_lock);
    rc_thread_slots[index].gen++;
    rc_mutex_unlock(&rc_thread_slots_lock);
}


void rc_thread_run_(void *slot)
{
    rc_thread_slot *running = slot;

    running->fn(running->context);
}


rc_thread rc_thread_make(void (*fn)(void *), void *context)
{
    rc_thread thread = {0};
    uint32_t index = rc_thread_take_slot(fn, context);
    uintptr_t handle = 0;

    if (index == RC_INDEX_NONE) {
        return thread;
    }

    handle = rc_os_thread_start(&rc_thread_slots[index], RC_THREAD_STACK_SIZE);
    if (handle == 0) {
        rc_thread_free_slot(index);
        return thread;
    }

    // Only the joiner reads the handle, after whatever handed it the rc_thread
    rc_thread_slots[index].handle = handle;
    thread.index_ = index;
    thread.gen_ = rc_thread_slots[index].gen;
    return thread;
}


bool rc_thread_is_valid(rc_thread thread)
{
    bool valid = false;

    if ((thread.gen_ & 1u) == 0 || thread.index_ >= RC_THREAD_MAX) {
        return false;
    }

    rc_mutex_lock(&rc_thread_slots_lock);
    valid = rc_thread_slots[thread.index_].gen == thread.gen_;
    rc_mutex_unlock(&rc_thread_slots_lock);
    return valid;
}


void rc_thread_join(rc_thread *thread)
{
    rc_thread zero = {0};

    // Checking takes the table's lock, so only in debug builds
#ifndef NDEBUG
    RC_ASSERT(rc_thread_is_valid(*thread));
#endif
    rc_os_thread_join(rc_thread_slots[thread->index_].handle);
    rc_thread_free_slot(thread->index_);
    *thread = zero;
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
