// tls.c
//
// Thread-local storage keys over one RC_THREAD_LOCAL array, which works the same on every target,
// wasm included, with no OS keys to run out of. Each entry holds the generation of the key it was
// set under, so a get matches it against the key's own and finds NULL for anything older.

#include "richc/thread/tls.h"

#include "richc/thread/atomic.h"
#include "richc/thread/mutex.h"
#include "richc/thread/thread.h"

// One thread's value for one key.
typedef struct rc_tls_value {
    void *value;
    uint32_t gen;   // the key's generation when this thread set it; 0 if never
} rc_tls_value;

// Each key's generation, odd while the key exists. A key carries its own copy, so get and set
// read these only in debug builds, to check the key is still live.
static rc_atomic_u32 rc_tls_gens[RC_TLS_MAX];
static rc_mutex rc_tls_gens_lock;

static RC_THREAD_LOCAL rc_tls_value rc_tls_values[RC_TLS_MAX];


// Whether key exists: it was made and not yet released.
static bool rc_tls_is_live(rc_tls key)
{
    if (key.index_ >= RC_TLS_MAX) {
        return false;
    }

    return (key.gen_ & 1u) != 0 && rc_atomic_u32_load(&rc_tls_gens[key.index_], RC_MEMORY_ORDER_RELAXED) == key.gen_;
}


rc_tls rc_tls_make(void)
{
    rc_tls key = {0};

    rc_mutex_lock(&rc_tls_gens_lock);
    for (uint32_t i = 0; i < RC_TLS_MAX; i++) {
        uint32_t gen = rc_atomic_u32_load(&rc_tls_gens[i], RC_MEMORY_ORDER_RELAXED);

        if ((gen & 1u) == 0) {
            rc_atomic_u32_store(&rc_tls_gens[i], gen + 1, RC_MEMORY_ORDER_RELAXED);
            key.index_ = i;
            key.gen_ = gen + 1;
            break;
        }
    }

    rc_mutex_unlock(&rc_tls_gens_lock);
    RC_PANIC(key.gen_ != 0);
    return key;
}


void rc_tls_release(rc_tls *key)
{
    rc_tls zero = {0};

    RC_ASSERT(rc_tls_is_live(*key));
    rc_mutex_lock(&rc_tls_gens_lock);
    (void)rc_atomic_u32_fetch_add(&rc_tls_gens[key->index_], 1, RC_MEMORY_ORDER_RELAXED);
    rc_mutex_unlock(&rc_tls_gens_lock);
    *key = zero;
}


void *rc_tls_get(rc_tls key)
{
    rc_tls_value *entry = NULL;

#ifndef NDEBUG
    RC_ASSERT(rc_tls_is_live(key));
#endif
    entry = &rc_tls_values[key.index_];
    return entry->gen == key.gen_ ? entry->value : NULL;
}


void rc_tls_set(rc_tls key, void *value)
{
    rc_tls_value *entry = NULL;

#ifndef NDEBUG
    RC_ASSERT(rc_tls_is_live(key));
#endif
    entry = &rc_tls_values[key.index_];
    entry->value = value;
    entry->gen = key.gen_;
}
