// thread/semaphore.h
//
// A count that threads take from one at a time, sleeping while it is 0, and add to. A zeroed
// rc_semaphore holds 0, rc_semaphore_make starts one at any other count, and none needs a deinit.
// A waiter may free the semaphore as soon as its wait returns, even while the post that woke it is
// still returning.

#ifndef RC_THREAD_SEMAPHORE_H_
#define RC_THREAD_SEMAPHORE_H_

#include "richc/thread/atomic.h"

// One word holds the count and the sleepers, so a post adds to the count and finds the sleepers
// in one step. Sleepers wait on the count, the low half, as every target is little-endian.
typedef struct rc_semaphore {
    union {
        rc_atomic_u64 state_;   // the count in the low 32 bits, and the threads asleep in the high 32
        rc_atomic_u32 count_;   // the low half alone, for the futex
    };
} rc_semaphore;

// A semaphore holding count. It is not itself atomic, so make one before sharing it.
rc_semaphore rc_semaphore_make(uint32_t count);

// Take one from the count if it is not 0, returning whether it was.
bool rc_semaphore_try_wait(rc_semaphore *semaphore);

// Take one from the count, sleeping while it is 0.
void rc_semaphore_wait(rc_semaphore *semaphore);

// rc_semaphore_wait for at most timeout_ns, returning false, with the count untouched, if the time
// ran out.
bool rc_semaphore_wait_for(rc_semaphore *semaphore, uint64_t timeout_ns);

// Add count to the count, waking that many sleepers.
void rc_semaphore_post(rc_semaphore *semaphore, uint32_t count);

#endif // RC_THREAD_SEMAPHORE_H_
