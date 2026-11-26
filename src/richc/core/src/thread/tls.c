// tls.c
//
// Thread-local storage keys over one RC_THREAD_LOCAL array, which works the same on every target,
// wasm included, with no OS keys to run out of. Each entry holds the generation of the key it was
// set under, so a get matches it against the key's own and finds NULL for anything older. Making
// a key is a compare-exchange on its slot's generation, so no lock is needed.

#include "richc/thread/tls.h"

#include "richc/thread/atomic.h"
#include "richc/thread/thread.h"

// One thread's value for one key.
typedef struct rc_tls_value {
    void *value;
    uint32_t gen;   // the key's generation when this thread set it; 0 if never
} rc_tls_value;

// Each key's generation, odd while the key exists. A key carries its own copy, so get and set
// read these only in debug builds, to check the key is still live.
static rc_atomic_u32 rc_tls_gens[RC_TLS_MAX];

static RC_THREAD_LOCAL rc_tls_value rc_tls_values[RC_TLS_MAX];


// Whether key exists: it was made and not yet released.
static bool rc_tls_is_live(rc_tls key)
{
    if (key.index_ >= RC_TLS_MAX) {
        return false;
    }

    if ((key.gen_ & 1u) == 0) {
        return false;
    }

    return rc_atomic_u32_load(&rc_tls_gens[key.index_], RC_MEMORY_ORDER_RELAXED) == key.gen_;
}


rc_tls rc_tls_make(void)
{
    for (uint32_t i = 0; i < RC_TLS_MAX; i++) {
        uint32_t gen = rc_atomic_u32_load(&rc_tls_gens[i], RC_MEMORY_ORDER_RELAXED);

        if ((gen & 1u) != 0) {
            continue;
        }

        // Another thread may make a key in this slot first
        if (rc_atomic_u32_compare_exchange(&rc_tls_gens[i], gen, gen + 1, RC_MEMORY_ORDER_RELAXED) == gen) {
            return (rc_tls) {.index_ = i, .gen_ = gen + 1};
        }
    }

    // RC_TLS_MAX keys exist already
    RC_PANIC(false);
    return (rc_tls) {0};
}


void rc_tls_release(rc_tls *key)
{
    RC_ASSERT(rc_tls_is_live(*key));
    (void)rc_atomic_u32_fetch_add(&rc_tls_gens[key->index_], 1, RC_MEMORY_ORDER_RELAXED);
    *key = (rc_tls) {0};
}


void *rc_tls_get(rc_tls key)
{
    RC_ASSERT(rc_tls_is_live(key));
    return rc_tls_values[key.index_].gen == key.gen_ ? rc_tls_values[key.index_].value : NULL;
}


void rc_tls_set(rc_tls key, void *value)
{
    RC_ASSERT(rc_tls_is_live(key));
    rc_tls_values[key.index_] = (rc_tls_value) {.value = value, .gen = key.gen_};
}
