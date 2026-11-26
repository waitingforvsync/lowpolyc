// thread/cond.h
//
// A condition variable: threads sleep on it, with a mutex held, until another wakes them. A
// waiter can also wake for no reason, so it waits in a loop until what it waits for is true. Change
// what waiters wait for only with the mutex held, or a signal sent after the change can miss a
// waiter that had not yet begun to wait. A zeroed rc_cond is ready, and needs no init or deinit.

#ifndef RC_THREAD_COND_H_
#define RC_THREAD_COND_H_

#include "richc/thread/mutex.h"

typedef struct rc_cond {
    rc_atomic_u32 seq_;       // bumped by every signal, so a waiter can tell it missed one
    rc_atomic_u32 waiters_;   // threads in a wait, counted under the mutex
} rc_cond;

// Release mutex, which this thread holds, and sleep until woken, then take mutex again.
void rc_cond_wait(rc_cond *cond, rc_mutex *mutex);

// rc_cond_wait for at most timeout_ns, returning false if the time ran out. mutex is held again
// either way.
bool rc_cond_wait_for(rc_cond *cond, rc_mutex *mutex, uint64_t timeout_ns);

// Wake one thread waiting on cond, if any are.
void rc_cond_signal(rc_cond *cond);

// Wake every thread waiting on cond.
void rc_cond_broadcast(rc_cond *cond);

#endif // RC_THREAD_COND_H_
