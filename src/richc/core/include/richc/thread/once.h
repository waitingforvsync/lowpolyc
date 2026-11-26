// thread/once.h
//
// Running something exactly once, however many threads ask at the same time: the first runs it,
// and the rest sleep until it has finished. A zeroed rc_once has not run.

#ifndef RC_THREAD_ONCE_H_
#define RC_THREAD_ONCE_H_

#include "richc/thread/atomic.h"

typedef struct rc_once {
    rc_atomic_u32 state_;   // 0 not run, 1 running, 2 running with threads perhaps asleep, 3 done
} rc_once;

// Call fn(context) if once has not run it yet, and return once it has run, by this thread or
// another. fn must not call this with the same once.
void rc_once_call(rc_once *once, void (*fn)(void *), void *context);

#endif // RC_THREAD_ONCE_H_
