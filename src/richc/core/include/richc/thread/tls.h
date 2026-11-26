// thread/tls.h
//
// Thread-local storage keys, for when how many thread-local variables there are is only known at
// run time; RC_THREAD_LOCAL in thread.h is simpler when it is known. A key names one void * that
// has its own value in every thread, NULL until that thread sets it. Keys have no destructors.
//
// Each key is a slot in a per-thread array of RC_TLS_MAX, with a generation, so a value set under
// a key that has since been released reads as NULL under the key that reuses its slot.

#ifndef RC_THREAD_TLS_H_
#define RC_THREAD_TLS_H_

#include <stdint.h>

enum {
    RC_TLS_MAX = 64   // keys that can exist at once
};

// A thread-local storage key, until it is released. A zeroed one is not a key.
typedef struct rc_tls {
    uint32_t index_;   // its slot in each thread's array
    uint32_t gen_;     // the slot's generation, odd while in use
} rc_tls;

// A new key, whose value is NULL in every thread. Traps if RC_TLS_MAX keys exist already.
rc_tls rc_tls_make(void);

// Free key, and zero it. No thread may use it again.
void rc_tls_release(rc_tls *key);

// This thread's value for key.
void *rc_tls_get(rc_tls key);

// Set this thread's value for key.
void rc_tls_set(rc_tls key, void *value);

#endif // RC_THREAD_TLS_H_
