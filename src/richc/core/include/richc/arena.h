// arena.h
//
// The bump allocator that owns every allocation in richc.
//
// An arena is a chain of chunks, each a run of 64 KiB pages taken from one global page heap (linear
// memory on wasm, a reserved address range natively). Pointers never move. The last allocation can
// grow in place, and when the current chunk is the newest in the heap, the chunk itself grows with
// it. Releasing an arena hands its chunks to a global pool for the next arena to reuse.
//
// Passing an arena by value gives the callee a private scratch cursor: its allocations vanish when
// it returns, and any chunk it had to add stays in the chain for the caller to reuse later. Only
// the owner may release an arena.
//
// The page heap and pool are global and not thread-safe.
// rc_arena_init sets them up (rc_arena_make calls it, so that is optional), and rc_arena_deinit
// tears them down, invalidating every arena at once.

#ifndef RC_ARENA_H_
#define RC_ARENA_H_

#include "richc/macros.h"

// Alignment of every plain allocation: enough for any standard C type.
#define RC_MAX_ALIGN 16u

// Largest alignment rc_arena_alloc_aligned supports: the page size.
#define RC_ARENA_MAX_ALIGN 0x10000u

// Chunk size for rc_arena_make_default, and for rc_arena_make(0).
#define RC_ARENA_DEFAULT_CHUNK 0x10000u

typedef struct rc_arena {
    uint32_t first;  // page index of the first chunk; 0 until made
    uint32_t chunk;  // page index of the current chunk
    uint32_t top;    // offset of the next free byte in the current chunk
    uint32_t end;    // size of the current chunk in bytes, cached from its header
} rc_arena;

// A saved allocation position, for rc_arena_restore.
typedef struct rc_arena_mark {
    uint32_t chunk;
    uint32_t top;
} rc_arena_mark;

// Called with the failed request size when memory runs out. If it returns, we panic.
typedef void rc_arena_oom_fn(uint32_t size);

// Set up the page heap. Does nothing if it is already set up.
// False if its address space cannot be reserved.
bool rc_arena_init(void);

// Tear the page heap down, invalidating every arena and pooled chunk. Natively the memory goes back
// to the OS; on wasm, where linear memory cannot shrink, the next rc_arena_init reuses it.
void rc_arena_deinit(void);

// Make an arena whose chunks are at least chunk_size bytes (0: the default). Sets up the page heap
// if need be. Out of memory goes to the OOM handler, so this never fails.
rc_arena rc_arena_make(uint32_t chunk_size);

// Hand all of the arena's chunks back to the pool and zero it.
void rc_arena_release(rc_arena *a);

// Allocate size (non-zero) bytes aligned to RC_MAX_ALIGN. Not zeroed; never NULL.
void *rc_arena_alloc(rc_arena *a, uint32_t size);

// Like rc_arena_alloc, but zeroed.
void *rc_arena_alloc_zero(rc_arena *a, uint32_t size);

// Allocate with a power-of-two alignment up to RC_ARENA_MAX_ALIGN. A later realloc keeps the
// alignment only if it stays in place.
void *rc_arena_alloc_aligned(rc_arena *a, uint32_t size, uint32_t align);

// Reclaim ptr if it is the last allocation; otherwise do nothing and return false.
bool rc_arena_free(rc_arena *a, void *ptr, uint32_t size);

// Resize an allocation, in place when it is the last one (ptr NULL allocates). Growing bytes are
// not zeroed; never NULL.
void *rc_arena_realloc(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size);

// Like rc_arena_realloc, but zeroes any bytes added when growing.
void *rc_arena_realloc_zero(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size);

// Rewind to the start of the first chunk. All chunks stay in the chain for reuse.
void rc_arena_reset(rc_arena *a);

// The current allocation position, to rewind to later with rc_arena_restore.
rc_arena_mark rc_arena_save(const rc_arena *a);

// Free everything allocated since mark was saved.
void rc_arena_restore(rc_arena *a, rc_arena_mark mark);

// Install the out-of-memory handler for all arenas (NULL: just panic).
void rc_arena_set_oom_handler(rc_arena_oom_fn *fn);

// Make an arena with the default chunk size.
static inline rc_arena rc_arena_make_default(void)
{
    return rc_arena_make(RC_ARENA_DEFAULT_CHUNK);
}

// True if a and b share their chunks, e.g. an arena and a by-value copy of it. A function taking
// both an arena and a scratch should assert this is false.
static inline bool rc_arena_is_same(const rc_arena *a, const rc_arena *b)
{
    return a->first != 0 && a->first == b->first;
}

#endif // RC_ARENA_H_
