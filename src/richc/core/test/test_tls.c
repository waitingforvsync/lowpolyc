// test_tls.c
//
// Tests for rc_tls keys. Each thread's own values are tested here too, with threads, which wasm
// without threads skips.

#include "richc/test.h"
#include "richc/thread/thread.h"
#include "richc/thread/tls.h"

#if defined(__wasm__) && !defined(__wasm_atomics__)
#  define RC_TEST_THREADS RC_TEST_SKIP
#else
#  define RC_TEST_THREADS RC_TEST
#endif


RC_TEST(tls, a_new_key_holds_null)
{
    rc_tls key = rc_tls_make();

    RC_CHECK_TRUE(rc_tls_get(key) == NULL);
    rc_tls_release(&key);
}


RC_TEST(tls, keys_hold_their_own_values)
{
    int first_value = 1;
    int second_value = 2;
    rc_tls first = rc_tls_make();
    rc_tls second = rc_tls_make();

    rc_tls_set(first, &first_value);
    RC_CHECK_TRUE(rc_tls_get(first) == &first_value);
    RC_CHECK_TRUE(rc_tls_get(second) == NULL);
    rc_tls_set(second, &second_value);
    rc_tls_set(first, NULL);
    RC_CHECK_TRUE(rc_tls_get(first) == NULL);
    RC_CHECK_TRUE(rc_tls_get(second) == &second_value);
    rc_tls_release(&first);
    rc_tls_release(&second);
}


RC_TEST(tls, a_reused_slot_starts_at_null)
{
    int value = 1;
    rc_tls key = rc_tls_make();
    rc_tls again = {0};
    uint32_t index = key.index_;

    rc_tls_set(key, &value);
    rc_tls_release(&key);
    RC_CHECK(key.gen_, ==, 0);

    // The lowest free slot is taken first, so this is the one just released
    again = rc_tls_make();
    RC_CHECK(again.index_, ==, index);
    RC_CHECK_TRUE(rc_tls_get(again) == NULL);
    rc_tls_release(&again);
}


RC_TEST(tls, every_key_can_exist_at_once)
{
    rc_tls keys[RC_TLS_MAX] = {0};

    for (uint32_t i = 0; i < RC_TLS_MAX; i++) {
        keys[i] = rc_tls_make();
        rc_tls_set(keys[i], &keys[i]);
    }

    for (uint32_t i = 0; i < RC_TLS_MAX; i++) {
        RC_CHECK_TRUE(rc_tls_get(keys[i]) == &keys[i]);
        rc_tls_release(&keys[i]);
    }
}


// A key, and what each worker read back from it.
typedef struct tls_worker {
    rc_tls key;
    void *before;
    void *after;
} tls_worker;


// Read the key, set it to something of this worker's own, and read it again.
static void use_key(void *context)
{
    tls_worker *worker = context;

    worker->before = rc_tls_get(worker->key);
    rc_tls_set(worker->key, worker);
    rc_thread_yield();
    worker->after = rc_tls_get(worker->key);
}


RC_TEST_THREADS(tls, each_thread_has_its_own_value)
{
    int value = 1;
    rc_tls key = rc_tls_make();
    tls_worker workers[4] = {0};
    rc_thread threads[4] = {0};

    rc_tls_set(key, &value);
    for (uint32_t i = 0; i < 4; i++) {
        workers[i].key = key;
        threads[i] = rc_thread_make(use_key, &workers[i]);
    }

    for (uint32_t i = 0; i < 4; i++) {
        RC_CHECK_TRUE(rc_thread_is_valid(threads[i]));
        rc_thread_join(&threads[i]);
        RC_CHECK_TRUE(workers[i].before == NULL);
        RC_CHECK_TRUE(workers[i].after == &workers[i]);
    }

    RC_CHECK_TRUE(rc_tls_get(key) == &value);
    rc_tls_release(&key);
}
