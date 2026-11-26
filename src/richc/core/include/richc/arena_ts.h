// arena_ts.h
//
// An arena that threads share: an rc_arena behind a mutex. Each call takes the lock only to move
// the arena on, and zeroes memory after letting it go. For a run of allocations, or to hand the
// arena to code that takes an rc_arena, lock it once and work on the arena that gives.
//
// An arena of its own on each thread needs no lock, so prefer that, and share one only where
// allocations made on many threads must last as long as one another. A zeroed rc_arena_ts has not
// been made. Never copy one: a copy would have a lock of its own. Release and reset are for when
// no other thread is using it, and the out-of-memory handler runs with it locked, so one that
// leaves by a jump leaves it locked for good.

#ifndef RC_ARENA_TS_H_
#define RC_ARENA_TS_H_

#include "richc/arena.h"
#include "richc/thread/mutex.h"

typedef struct rc_arena_ts {
    rc_mutex lock_;
    rc_arena arena_;
} rc_arena_ts;

// Make a shared arena whose chunks are at least chunk_size bytes (0: the default), as
// rc_arena_make does.
rc_arena_ts rc_arena_ts_make(uint32_t chunk_size);

// Give all of the arena's chunks back and zero it.
void rc_arena_ts_release(rc_arena_ts *a);

// Allocate size (non-zero) bytes aligned to RC_MAX_ALIGN. Not zeroed; never NULL.
void *rc_arena_ts_alloc(rc_arena_ts *a, uint32_t size);

// Like rc_arena_ts_alloc, but zeroed.
void *rc_arena_ts_alloc_zero(rc_arena_ts *a, uint32_t size);

// Allocate with a power-of-two alignment up to RC_ARENA_MAX_ALIGN.
void *rc_arena_ts_alloc_aligned(rc_arena_ts *a, uint32_t size, uint32_t align);

// Reclaim ptr if it is still the last allocation, by any thread; otherwise return false.
bool rc_arena_ts_free(rc_arena_ts *a, void *ptr, uint32_t size);

// Resize an allocation, in place if it is still the last one (ptr NULL allocates). Growing bytes
// are not zeroed; never NULL.
void *rc_arena_ts_realloc(rc_arena_ts *a, void *ptr, uint32_t old_size, uint32_t new_size);

// Like rc_arena_ts_realloc, but zeroes any bytes added when growing.
void *rc_arena_ts_realloc_zero(rc_arena_ts *a, void *ptr, uint32_t old_size, uint32_t new_size);

// Rewind to the start of the first chunk, freeing everything every thread allocated.
void rc_arena_ts_reset(rc_arena_ts *a);

// Lock the arena and return it, for this thread alone until rc_arena_ts_unlock.
rc_arena *rc_arena_ts_lock(rc_arena_ts *a);

// Unlock the arena, which this thread locked; the rc_arena that rc_arena_ts_lock gave is not to be
// used after.
void rc_arena_ts_unlock(rc_arena_ts *a);

// Make a shared arena with the default chunk size.
static inline rc_arena_ts rc_arena_ts_make_default(void)
{
    return rc_arena_ts_make(RC_ARENA_DEFAULT_CHUNK);
}

#endif // RC_ARENA_TS_H_
