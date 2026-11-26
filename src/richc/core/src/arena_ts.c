// arena_ts.c
//
// The shared arena: each call is the rc_arena one with the lock taken around it, apart from the
// zeroing, which needs no lock.

#include "richc/arena_ts.h"

#include "richc/mem.h"

// Unlock a, passing ptr on, so a call can let the lock go as it returns.
static void *rc_arena_ts_unlock_ptr(rc_arena_ts *a, void *ptr)
{
    rc_mutex_unlock(&a->lock_);
    return ptr;
}


// Unlock a, passing value on.
static bool rc_arena_ts_unlock_bool(rc_arena_ts *a, bool value)
{
    rc_mutex_unlock(&a->lock_);
    return value;
}


rc_arena_ts rc_arena_ts_make(uint32_t chunk_size)
{
    // The lock is left zeroed, which is unlocked
    return (rc_arena_ts) {.arena_ = rc_arena_make(chunk_size)};
}


void rc_arena_ts_release(rc_arena_ts *a)
{
    RC_ASSERT(a);

    rc_arena_release(&a->arena_);
    *a = (rc_arena_ts) {0};
}


void *rc_arena_ts_alloc(rc_arena_ts *a, uint32_t size)
{
    RC_ASSERT(a);

    rc_mutex_lock(&a->lock_);
    return rc_arena_ts_unlock_ptr(a, rc_arena_alloc(&a->arena_, size));
}


void *rc_arena_ts_alloc_zero(rc_arena_ts *a, uint32_t size)
{
    void *result = rc_arena_ts_alloc(a, size);

    rc_memset(result, 0, size);
    return result;
}


void *rc_arena_ts_alloc_aligned(rc_arena_ts *a, uint32_t size, uint32_t align)
{
    RC_ASSERT(a);

    rc_mutex_lock(&a->lock_);
    return rc_arena_ts_unlock_ptr(a, rc_arena_alloc_aligned(&a->arena_, size, align));
}


bool rc_arena_ts_free(rc_arena_ts *a, void *ptr, uint32_t size)
{
    RC_ASSERT(a);

    rc_mutex_lock(&a->lock_);
    return rc_arena_ts_unlock_bool(a, rc_arena_free(&a->arena_, ptr, size));
}


void *rc_arena_ts_realloc(rc_arena_ts *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    RC_ASSERT(a);

    rc_mutex_lock(&a->lock_);
    return rc_arena_ts_unlock_ptr(a, rc_arena_realloc(&a->arena_, ptr, old_size, new_size));
}


void *rc_arena_ts_realloc_zero(rc_arena_ts *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    uint32_t kept = ptr ? old_size : 0;
    uint8_t *result = rc_arena_ts_realloc(a, ptr, old_size, new_size);

    // Everything past what came over from ptr is new, and zeroed
    if (new_size > kept) {
        rc_memset(result + kept, 0, new_size - kept);
    }

    return result;
}


void rc_arena_ts_reset(rc_arena_ts *a)
{
    RC_ASSERT(a);

    rc_mutex_lock(&a->lock_);
    rc_arena_reset(&a->arena_);
    rc_mutex_unlock(&a->lock_);
}


rc_arena *rc_arena_ts_lock(rc_arena_ts *a)
{
    RC_ASSERT(a);

    rc_mutex_lock(&a->lock_);
    return &a->arena_;
}


void rc_arena_ts_unlock(rc_arena_ts *a)
{
    RC_ASSERT(a);

    rc_mutex_unlock(&a->lock_);
}
