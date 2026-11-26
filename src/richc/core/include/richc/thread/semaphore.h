// thread/semaphore.h
//
// A count that threads take from one at a time, sleeping while it is 0, and add to. A zeroed
// rc_semaphore holds 0, rc_semaphore_make starts one at any other count, and none needs a deinit.

#ifndef RC_THREAD_SEMAPHORE_H_
#define RC_THREAD_SEMAPHORE_H_

#include "richc/thread/atomic.h"

typedef struct rc_semaphore {
    rc_atomic_u32 count_;
    rc_atomic_u32 sleepers_;   // threads asleep or about to be, so a post knows to wake them
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
