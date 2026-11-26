// platform/futex.h
//
// Sleeping until a 32-bit word changes, which every lock in thread/ is built on, so each lock is
// written once for every target. Private to core, and implemented once per target in
// platform/<target>/futex.c: the futex syscall on Linux, os_sync_wait_on_address or __ulock on
// macOS, WaitOnAddress on Windows, and memory.atomic.wait32 on wasm with threads, or without them
// a wait no other thread can end.

#ifndef RC_PLATFORM_FUTEX_H_
#define RC_PLATFORM_FUTEX_H_

#include "richc/thread/atomic.h"

// A timeout that never runs out.
#define RC_FUTEX_FOREVER UINT64_MAX

// How many times a lock looks again before going to sleep. Sleeping and waking cost a syscall
// each, and most locks are held only briefly.
enum {
    RC_FUTEX_SPINS = 100
};

// Sleep while word holds expected, until woken or timeout_ns passes. It may also return for no
// reason, so callers look at the word again. Returns false only if the time ran out.
bool rc_futex_wait(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns);

// Wake one thread sleeping on word, returning whether there was one. Where the OS does not say,
// this is false, so a caller must treat false as "perhaps nobody".
bool rc_futex_wake_one(rc_atomic_u32 *word);

// Wake every thread sleeping on word.
void rc_futex_wake_all(rc_atomic_u32 *word);

#endif // RC_PLATFORM_FUTEX_H_
