// test_thread.c
//
// Tests for rc_thread, and for every primitive in thread/ with threads racing on it. Workers never
// check anything themselves, as a failing check leaves the test from the test's own thread only:
// they record what they saw, and the test checks it after joining them.
//
// wasm without threads cannot start any, so the tests that need them are skipped there.

#include "richc/test.h"
#include "richc/thread/cond.h"
#include "richc/thread/once.h"
#include "richc/thread/rwlock.h"
#include "richc/thread/semaphore.h"
#include "richc/thread/spinlock.h"
#include "richc/thread/thread.h"
#include "richc/time.h"

#if defined(__wasm__) && !defined(__wasm_atomics__)
#  define RC_TEST_THREADS RC_TEST_SKIP
#  define RC_TEST_NO_THREADS RC_TEST
#else
#  define RC_TEST_THREADS RC_TEST
#  define RC_TEST_NO_THREADS RC_TEST_SKIP
#endif

enum {
    WORKERS = 4,
    ROUNDS = 20000
};


// Start WORKERS threads running fn, each with its own context, from contexts spaced size bytes
// apart, and join them all. Returns false if any would not start.
static bool run_workers(void (*fn)(void *), void *contexts, uint32_t size)
{
    rc_thread threads[WORKERS] = {0};
    bool started = true;

    for (uint32_t i = 0; i < WORKERS; i++) {
        threads[i] = rc_thread_make(fn, (uint8_t *)contexts + i * size);
        started = started && rc_thread_is_valid(threads[i]);
    }

    for (uint32_t i = 0; i < WORKERS; i++) {
        if (rc_thread_is_valid(threads[i])) {
            rc_thread_join(&threads[i]);
        }
    }

    return started;
}


// ---- threads ----

// What one worker saw of itself.
typedef struct worker_record {
    uint32_t index;
    uint32_t ran;
    uint64_t id;
    uint32_t local_count;
} worker_record;

static RC_THREAD_LOCAL uint32_t local_count;


// Record that this worker ran, its id, and that its thread-local counter starts from zero.
static void record_self(void *context)
{
    worker_record *record = context;

    for (uint32_t i = 0; i < 1000; i++) {
        local_count++;
    }

    record->ran = 1;
    record->id = rc_thread_current_id();
    record->local_count = local_count;
}


RC_TEST_THREADS(thread, every_worker_runs_with_its_own_locals)
{
    worker_record records[WORKERS] = {0};

    local_count = 7;
    RC_CHECK_TRUE(run_workers(record_self, records, sizeof(records[0])));
    for (uint32_t i = 0; i < WORKERS; i++) {
        RC_CHECK(records[i].ran, ==, 1);
        RC_CHECK(records[i].local_count, ==, 1000);
        RC_CHECK_TRUE(records[i].id != rc_thread_current_id());
        for (uint32_t j = 0; j < i; j++) {
            RC_CHECK_TRUE(records[i].id != records[j].id);
        }
    }

    RC_CHECK(local_count, ==, 7);
}


// Do nothing, for a thread whose handle is all that matters.
static void do_nothing(void *context)
{
    (void)context;
}


RC_TEST_THREADS(thread, a_joined_handle_is_stale)
{
    rc_thread first = rc_thread_make(do_nothing, NULL);
    rc_thread copy = first;
    rc_thread second = {0};

    RC_CHECK_TRUE(rc_thread_is_valid(first));
    rc_thread_join(&first);
    RC_CHECK_FALSE(rc_thread_is_valid(first));
    RC_CHECK_FALSE(rc_thread_is_valid(copy));

    // The next thread most likely takes the same slot, under a new generation
    second = rc_thread_make(do_nothing, NULL);
    RC_CHECK_TRUE(rc_thread_is_valid(second));
    RC_CHECK_FALSE(rc_thread_is_valid(copy));
    rc_thread_join(&second);
}


RC_TEST_NO_THREADS(thread, start_fails_without_threads)
{
    RC_CHECK_FALSE(rc_thread_is_valid(rc_thread_make(do_nothing, NULL)));
}


RC_TEST(thread, a_zeroed_handle_is_not_a_thread)
{
    rc_thread thread = {0};

    RC_CHECK_FALSE(rc_thread_is_valid(thread));
}


RC_TEST(thread, sleep_yield_and_cores)
{
    uint64_t start = rc_time_now_ns();

    rc_thread_sleep_ns(1000000u);
    RC_CHECK(rc_time_now_ns() - start, >=, 1000000u);
    rc_thread_yield();
    RC_CHECK(rc_thread_hardware_concurrency(), >=, 1);
    RC_CHECK_TRUE(rc_thread_current_id() == rc_thread_current_id());
}


// ---- atomics and spinlock ----

// Counters every worker adds to at once.
typedef struct shared_counts {
    rc_atomic_u8 u8;
    rc_atomic_u16 u16;
    rc_atomic_u32 u32;
    rc_atomic_u64 u64;
    rc_atomic_i64 swapped;
    rc_spinlock lock;
    uint64_t guarded;
} shared_counts;

static shared_counts counts;


// Count ROUNDS times on every counter.
static void count_rounds(void *context)
{
    (void)context;
    for (uint32_t i = 0; i < ROUNDS; i++) {
        int64_t held = rc_atomic_i64_load(&counts.swapped, RC_MEMORY_ORDER_RELAXED);

        (void)rc_atomic_u8_fetch_add(&counts.u8, 1, RC_MEMORY_ORDER_RELAXED);
        (void)rc_atomic_u16_fetch_add(&counts.u16, 1, RC_MEMORY_ORDER_RELAXED);
        (void)rc_atomic_u32_fetch_add(&counts.u32, 1, RC_MEMORY_ORDER_ACQ_REL);
        (void)rc_atomic_u64_fetch_add(&counts.u64, 3, RC_MEMORY_ORDER_SEQ_CST);

        for (;;) {
            int64_t found = rc_atomic_i64_compare_exchange(&counts.swapped, held, held + 1, RC_MEMORY_ORDER_ACQ_REL);

            if (found == held) {
                break;
            }
            held = found;
        }

        rc_spinlock_lock(&counts.lock);
        counts.guarded++;
        rc_spinlock_unlock(&counts.lock);
    }
}


RC_TEST_THREADS(thread, atomics_and_spinlock_count_every_round)
{
    uint8_t unused[WORKERS] = {0};
    uint64_t total = (uint64_t)WORKERS * ROUNDS;
    shared_counts zero = {0};

    counts = zero;
    RC_CHECK_TRUE(run_workers(count_rounds, unused, 1));
    RC_CHECK(rc_atomic_u8_load(&counts.u8, RC_MEMORY_ORDER_SEQ_CST), ==, (uint8_t)total);
    RC_CHECK(rc_atomic_u16_load(&counts.u16, RC_MEMORY_ORDER_SEQ_CST), ==, (uint16_t)total);
    RC_CHECK(rc_atomic_u32_load(&counts.u32, RC_MEMORY_ORDER_SEQ_CST), ==, (uint32_t)total);
    RC_CHECK(rc_atomic_u64_load(&counts.u64, RC_MEMORY_ORDER_SEQ_CST), ==, 3 * total);
    RC_CHECK(rc_atomic_i64_load(&counts.swapped, RC_MEMORY_ORDER_SEQ_CST), ==, (int64_t)total);
    RC_CHECK(counts.guarded, ==, total);
}


// ---- mutex and cond ----

// A small queue, which producers fill and consumers empty, with a mutex and two conds.
typedef struct queue {
    rc_mutex mutex;
    rc_cond not_full;
    rc_cond not_empty;
    uint32_t items[4];
    uint32_t head;
    uint32_t num;
    uint32_t producers_left;
    uint64_t consumed;   // the sum of every item taken
    uint64_t guarded;    // counted under the mutex alone
} queue;

static queue shared_queue;


// Put ROUNDS items on the queue, then count ROUNDS times under the mutex.
static void produce(void *context)
{
    queue *q = &shared_queue;

    (void)context;
    for (uint32_t i = 1; i <= ROUNDS; i++) {
        rc_mutex_lock(&q->mutex);
        while (q->num == RC_COUNT(q->items)) {
            rc_cond_wait(&q->not_full, &q->mutex);
        }

        q->items[(q->head + q->num) % RC_COUNT(q->items)] = i;
        q->num++;
        rc_cond_signal(&q->not_empty);
        rc_mutex_unlock(&q->mutex);
    }

    rc_mutex_lock(&q->mutex);
    q->producers_left--;
    rc_cond_broadcast(&q->not_empty);
    rc_mutex_unlock(&q->mutex);

    for (uint32_t i = 0; i < ROUNDS; i++) {
        rc_mutex_lock(&q->mutex);
        q->guarded++;
        rc_mutex_unlock(&q->mutex);
    }
}


// Take items off the queue until it is empty and every producer has finished.
static void consume(void *context)
{
    queue *q = &shared_queue;
    uint64_t sum = 0;

    (void)context;
    rc_mutex_lock(&q->mutex);
    for (;;) {
        while (q->num == 0 && q->producers_left > 0) {
            // Some waits time out, so that path is used too
            (void)rc_cond_wait_for(&q->not_empty, &q->mutex, 100000u);
        }

        if (q->num == 0) {
            break;
        }

        sum += q->items[q->head];
        q->head = (q->head + 1) % RC_COUNT(q->items);
        q->num--;
        rc_cond_signal(&q->not_full);
    }

    q->consumed += sum;
    rc_mutex_unlock(&q->mutex);
}


// Produce or consume, as context says.
static void produce_or_consume(void *context)
{
    const bool *producer = context;

    if (*producer) {
        produce(context);
    } else {
        consume(context);
    }
}


RC_TEST_THREADS(thread, mutex_and_cond_hand_over_every_item)
{
    const bool roles[WORKERS] = {true, false, true, false};
    queue zero = {0};

    shared_queue = zero;
    shared_queue.producers_left = 2;
    RC_CHECK_TRUE(run_workers(produce_or_consume, (void *)roles, sizeof(roles[0])));
    RC_CHECK(shared_queue.consumed, ==, 2 * (uint64_t)ROUNDS * (ROUNDS + 1) / 2);
    RC_CHECK(shared_queue.guarded, ==, 2 * ROUNDS);
    RC_CHECK(shared_queue.num, ==, 0);
}


// ---- semaphore ----

// Two semaphores two threads bounce between, and one that lets two threads in at a time.
typedef struct semaphores {
    rc_semaphore ping;
    rc_semaphore pong;
    rc_semaphore slots;
    rc_atomic_u32 inside;
    rc_atomic_u32 most_inside;
} semaphores;

static semaphores shared_semaphores;


// Bounce between ping and pong, or pass through slots, as context says.
static void use_semaphores(void *context)
{
    semaphores *s = &shared_semaphores;
    uint32_t role = *(const uint32_t *)context;

    for (uint32_t i = 0; i < ROUNDS / 4; i++) {
        uint32_t now = 0;
        uint32_t most = 0;

        if (role == 0) {
            rc_semaphore_post(&s->ping, 1);
            rc_semaphore_wait(&s->pong);
            continue;
        }

        if (role == 1) {
            rc_semaphore_wait(&s->ping);
            rc_semaphore_post(&s->pong, 1);
            continue;
        }

        while (!rc_semaphore_wait_for(&s->slots, 50000u)) {
            // Timing out now and then is the point
        }

        now = rc_atomic_u32_fetch_add(&s->inside, 1, RC_MEMORY_ORDER_RELAXED) + 1;
        most = rc_atomic_u32_load(&s->most_inside, RC_MEMORY_ORDER_RELAXED);
        while (now > most) {
            uint32_t found = rc_atomic_u32_compare_exchange(&s->most_inside, most, now, RC_MEMORY_ORDER_RELAXED);

            if (found == most) {
                break;
            }
            most = found;
        }

        rc_thread_yield();
        (void)rc_atomic_u32_fetch_sub(&s->inside, 1, RC_MEMORY_ORDER_RELAXED);
        rc_semaphore_post(&s->slots, 1);
    }
}


RC_TEST_THREADS(thread, semaphores_pass_every_post)
{
    const uint32_t roles[WORKERS] = {0, 1, 2, 2};
    semaphores zero = {0};

    shared_semaphores = zero;
    shared_semaphores.slots = rc_semaphore_make(1);
    RC_CHECK_TRUE(run_workers(use_semaphores, (void *)roles, sizeof(roles[0])));
    RC_CHECK_FALSE(rc_semaphore_try_wait(&shared_semaphores.ping));
    RC_CHECK_FALSE(rc_semaphore_try_wait(&shared_semaphores.pong));
    RC_CHECK(rc_atomic_u32_load(&shared_semaphores.most_inside, RC_MEMORY_ORDER_RELAXED), ==, 1);
    RC_CHECK_TRUE(rc_semaphore_try_wait(&shared_semaphores.slots));
    RC_CHECK_FALSE(rc_semaphore_try_wait(&shared_semaphores.slots));
}


// ---- rwlock ----

// Two values a writer always changes together, and what readers and writers saw.
typedef struct guarded_pair {
    rc_rwlock lock;
    uint64_t first;
    uint64_t second;
    rc_atomic_u32 readers;
    rc_atomic_u32 writers;
    rc_atomic_u32 clashes;   // a writer that was not alone, or a reader that saw the pair differ
} guarded_pair;

static guarded_pair shared_pair;


// Write the pair now and then, otherwise read it, checking nobody else is in while writing.
static void read_or_write(void *context)
{
    guarded_pair *p = &shared_pair;
    uint32_t worker = *(const uint32_t *)context;

    for (uint32_t i = 0; i < ROUNDS / 4; i++) {
        if (worker == 0 || i % 32 == 0) {
            rc_rwlock_write_lock(&p->lock);
            if (rc_atomic_u32_fetch_add(&p->writers, 1, RC_MEMORY_ORDER_RELAXED) != 0) {
                (void)rc_atomic_u32_fetch_add(&p->clashes, 1, RC_MEMORY_ORDER_RELAXED);
            }

            if (rc_atomic_u32_load(&p->readers, RC_MEMORY_ORDER_RELAXED) != 0) {
                (void)rc_atomic_u32_fetch_add(&p->clashes, 1, RC_MEMORY_ORDER_RELAXED);
            }

            p->first++;
            p->second++;
            (void)rc_atomic_u32_fetch_sub(&p->writers, 1, RC_MEMORY_ORDER_RELAXED);
            rc_rwlock_write_unlock(&p->lock);
            continue;
        }

        rc_rwlock_read_lock(&p->lock);
        (void)rc_atomic_u32_fetch_add(&p->readers, 1, RC_MEMORY_ORDER_RELAXED);
        if (rc_atomic_u32_load(&p->writers, RC_MEMORY_ORDER_RELAXED) != 0 || p->first != p->second) {
            (void)rc_atomic_u32_fetch_add(&p->clashes, 1, RC_MEMORY_ORDER_RELAXED);
        }

        (void)rc_atomic_u32_fetch_sub(&p->readers, 1, RC_MEMORY_ORDER_RELAXED);
        rc_rwlock_read_unlock(&p->lock);
    }
}


RC_TEST_THREADS(thread, rwlock_writers_are_alone)
{
    const uint32_t workers[WORKERS] = {0, 1, 2, 3};
    guarded_pair zero = {0};
    uint64_t writes = ROUNDS / 4 + 3 * (ROUNDS / 4 / 32 + (ROUNDS / 4 % 32 != 0));

    shared_pair = zero;
    RC_CHECK_TRUE(run_workers(read_or_write, (void *)workers, sizeof(workers[0])));
    RC_CHECK(rc_atomic_u32_load(&shared_pair.clashes, RC_MEMORY_ORDER_RELAXED), ==, 0);
    RC_CHECK(shared_pair.first, ==, writes);
    RC_CHECK(shared_pair.second, ==, writes);
    RC_CHECK_TRUE(rc_rwlock_try_write_lock(&shared_pair.lock));
    rc_rwlock_write_unlock(&shared_pair.lock);
}


// ---- once ----

// A once every worker races to run, and what each saw after.
typedef struct once_race {
    rc_once once;
    rc_atomic_u32 go;
    uint32_t calls;
    uint32_t value;
    rc_atomic_u32 saw_value;
} once_race;

static once_race shared_once;


// Set the value slowly, so the other workers arrive while it runs.
static void set_value_slowly(void *context)
{
    once_race *race = context;

    rc_thread_sleep_ns(2000000u);
    race->calls++;
    race->value = 42;
}


// Wait for the start, then race for the once.
static void race_for_once(void *context)
{
    once_race *race = &shared_once;

    (void)context;
    while (rc_atomic_u32_load(&race->go, RC_MEMORY_ORDER_ACQUIRE) == 0) {
        rc_spin_pause();
    }

    rc_once_call(&race->once, set_value_slowly, race);
    if (race->value == 42) {
        (void)rc_atomic_u32_fetch_add(&race->saw_value, 1, RC_MEMORY_ORDER_RELAXED);
    }
}


// Start the race, then race too.
static void start_and_race(void *context)
{
    uint32_t worker = *(const uint32_t *)context;

    if (worker == 0) {
        rc_atomic_u32_store(&shared_once.go, 1, RC_MEMORY_ORDER_RELEASE);
    }

    race_for_once(context);
}


RC_TEST_THREADS(thread, once_runs_once_across_threads)
{
    const uint32_t workers[WORKERS] = {0, 1, 2, 3};
    once_race zero = {0};

    shared_once = zero;
    RC_CHECK_TRUE(run_workers(start_and_race, (void *)workers, sizeof(workers[0])));
    RC_CHECK(shared_once.calls, ==, 1);
    RC_CHECK(rc_atomic_u32_load(&shared_once.saw_value, RC_MEMORY_ORDER_RELAXED), ==, WORKERS);
}
