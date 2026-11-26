// test_thread.c
//
// Tests for rc_thread, and for every primitive in thread/ and the arenas with threads racing on
// them. Workers never
// check anything themselves, as a failing check leaves the test from the test's own thread only:
// they record what they saw, and the test checks it after joining them.
//
// wasm without threads cannot start any, so the tests that need them are skipped there.

#include "richc/arena_ts.h"
#include "richc/mem.h"
#include "richc/random.h"
#include "richc/test.h"
#include "richc/thread/cond.h"
#include "richc/thread/once.h"
#include "richc/thread/rwlock.h"
#include "richc/thread/semaphore.h"
#include "richc/thread/spinlock.h"
#include "richc/thread/thread.h"
#include "richc/thread/tls.h"
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


// How many workers run_workers started, once it has started them all, and UINT32_MAX until then.
static rc_atomic_u32 workers_started;


// Start WORKERS threads running fn, each with its own context, from contexts spaced size bytes
// apart, and join them all. Returns false if any would not start.
static bool run_workers(void (*fn)(void *), void *contexts, uint32_t size)
{
    rc_thread threads[WORKERS] = {0};
    uint32_t started = 0;

    rc_atomic_u32_store(&workers_started, UINT32_MAX, RC_MEMORY_ORDER_RELAXED);
    for (uint32_t i = 0; i < WORKERS; i++) {
        threads[i] = rc_thread_make(fn, (uint8_t *)contexts + i * size);
        if (rc_thread_is_valid(threads[i])) {
            started++;
        }
    }

    rc_atomic_u32_store(&workers_started, started, RC_MEMORY_ORDER_RELAXED);
    for (uint32_t i = 0; i < WORKERS; i++) {
        if (rc_thread_is_valid(threads[i])) {
            rc_thread_join(&threads[i]);
        }
    }

    return started == WORKERS;
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

// Workers that have recorded themselves, so each can wait for the rest.
static rc_atomic_u32 recorded;


// Record that this worker ran, its id, and that its thread-local counter starts from zero, then
// wait for every worker that started to do the same. An id is only different from those of
// threads running at the same time, and a thread that had finished could pass its own on.
static void record_self(void *context)
{
    worker_record *record = context;

    for (uint32_t i = 0; i < 1000; i++) {
        local_count++;
    }

    record->ran = 1;
    record->id = rc_thread_current_id();
    record->local_count = local_count;
    (void)rc_atomic_u32_fetch_add(&recorded, 1, RC_MEMORY_ORDER_RELAXED);
    while (rc_atomic_u32_load(&recorded, RC_MEMORY_ORDER_RELAXED) < rc_atomic_u32_load(&workers_started, RC_MEMORY_ORDER_RELAXED)) {
        rc_thread_yield();
    }
}


RC_TEST_THREADS(thread, every_worker_runs_with_its_own_locals)
{
    worker_record records[WORKERS] = {0};

    rc_atomic_u32_store(&recorded, 0, RC_MEMORY_ORDER_RELAXED);
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


// A thread-local with an initialiser, which each thread must find at it.
static RC_THREAD_LOCAL uint32_t initialised_local = 5;

// A key, and what one thread found of its thread-locals.
typedef struct fresh_locals {
    rc_tls key;
    uint32_t local_seen;
    void *key_seen;
} fresh_locals;


// Record this thread's thread-locals as it found them, then change them, so a thread that ran
// after it on the same OS thread or worker would see the change.
static void use_locals(void *context)
{
    fresh_locals *record = context;

    record->local_seen = initialised_local;
    record->key_seen = rc_tls_get(record->key);
    initialised_local = 99;
    rc_tls_set(record->key, record);
}


// Run fn(context) on a thread of its own, and wait for it. Returns false if it would not start.
static bool run_one(void (*fn)(void *), void *context)
{
    rc_thread thread = rc_thread_make(fn, context);

    if (!rc_thread_is_valid(thread)) {
        return false;
    }

    rc_thread_join(&thread);
    return true;
}


// One thread after another: on wasm the second runs on the same worker as the first.
RC_TEST_THREADS(thread, each_thread_starts_with_fresh_locals)
{
    rc_tls key = rc_tls_make();
    fresh_locals first = {.key = key};
    fresh_locals second = {.key = key};

    RC_CHECK_TRUE(run_one(use_locals, &first));
    RC_CHECK_TRUE(run_one(use_locals, &second));
    RC_CHECK(first.local_seen, ==, 5);
    RC_CHECK_TRUE(first.key_seen == NULL);
    RC_CHECK(second.local_seen, ==, 5);
    RC_CHECK_TRUE(second.key_seen == NULL);
    RC_CHECK(initialised_local, ==, 5);
    rc_tls_release(&key);
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


// ---- arenas ----

enum {
    ARENA_PAGE = 65536,
    CHURN_ROUNDS = 100,
    CHURN_BLOCKS = 8,
    SHARED_STEPS = 600
};

// Byte i of the pattern that seed fills a block with. Patterns of different seeds never line up,
// however they are offset, so blocks that overlapped would show it.
static uint8_t pattern_byte(uint32_t seed, uint32_t i)
{
    return (uint8_t)(((seed ^ (i * 0x9E3779B1u)) * 0x85EBCA6Bu) >> 24);
}


// Fill bytes from to to of p with seed's pattern.
static void fill_pattern(uint8_t *p, uint32_t from, uint32_t to, uint32_t seed)
{
    for (uint32_t i = from; i < to; i++) {
        p[i] = pattern_byte(seed, i);
    }
}


// True if the size bytes at p still hold seed's pattern.
static bool holds_pattern(const uint8_t *p, uint32_t size, uint32_t seed)
{
    for (uint32_t i = 0; i < size; i++) {
        if (p[i] != pattern_byte(seed, i)) {
            return false;
        }
    }
    return true;
}


// True if the size bytes at p are all zero.
static bool is_zeroed(const uint8_t *p, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        if (p[i] != 0) {
            return false;
        }
    }
    return true;
}


// A random block size: mostly small, now and then more than a page.
static uint32_t random_block_size(rc_random *random)
{
    uint32_t limit = rc_random_next(random) % 16 == 0 ? 2 * ARENA_PAGE : 2048;

    return 16 + rc_random_next(random) % limit;
}


// The first page of a new one-page arena, released again: the top of the page heap, once every
// other arena is released.
static uint32_t top_of_heap(void)
{
    rc_arena arena = rc_arena_make(ARENA_PAGE);
    uint32_t first = arena.first;

    rc_arena_release(&arena);
    return first;
}


// Grow a block of size bytes holding seed's pattern to grown bytes, and carry the pattern on.
static uint8_t *grow_block(rc_arena *arena, uint8_t *block, uint32_t size, uint32_t grown, uint32_t seed)
{
    uint8_t *result = rc_arena_realloc(arena, block, size, grown);

    fill_pattern(result, size, grown, seed);
    return result;
}


// What one worker with arenas of its own saw.
typedef struct arena_churn {
    uint32_t seed;
    uint32_t damaged;   // blocks that lost their pattern
} arena_churn;


// Make an arena, fill it with blocks, grow the last, then check them all and release it. Returns
// how many were damaged.
static uint32_t churn_once(rc_random *random, uint32_t seed)
{
    rc_arena arena = rc_arena_make((1 + rc_random_next(random) % 4) * ARENA_PAGE);
    uint32_t extra = random_block_size(random);
    uint8_t *blocks[CHURN_BLOCKS] = {0};
    uint32_t sizes[CHURN_BLOCKS] = {0};
    uint32_t damaged = 0;

    for (uint32_t i = 0; i < CHURN_BLOCKS; i++) {
        sizes[i] = random_block_size(random);
        blocks[i] = rc_arena_alloc(&arena, sizes[i]);
        fill_pattern(blocks[i], 0, sizes[i], seed + i);
    }

    blocks[CHURN_BLOCKS - 1] = grow_block(&arena, blocks[CHURN_BLOCKS - 1], sizes[CHURN_BLOCKS - 1],
        sizes[CHURN_BLOCKS - 1] + extra, seed + CHURN_BLOCKS - 1);
    sizes[CHURN_BLOCKS - 1] += extra;
    for (uint32_t i = 0; i < CHURN_BLOCKS; i++) {
        damaged += holds_pattern(blocks[i], sizes[i], seed + i) ? 0u : 1u;
    }

    rc_arena_release(&arena);
    return damaged;
}


// Make, fill and release arenas over and over, while one more arena of this worker's keeps growing
// a block, as other workers' chunks come and go around its own.
static void churn_arenas(void *context)
{
    arena_churn *churn = context;
    uint32_t seed = churn->seed << 16;
    rc_random random = rc_random_make(churn->seed);
    rc_arena kept = rc_arena_make(ARENA_PAGE);
    uint32_t kept_size = 1024;
    uint8_t *kept_block = rc_arena_alloc(&kept, kept_size);

    fill_pattern(kept_block, 0, kept_size, seed);
    for (uint32_t round = 0; round < CHURN_ROUNDS; round++) {
        churn->damaged += churn_once(&random, seed + 1 + round * CHURN_BLOCKS);
        kept_block = grow_block(&kept, kept_block, kept_size, kept_size + 1024, seed);
        kept_size += 1024;
    }

    churn->damaged += holds_pattern(kept_block, kept_size, seed) ? 0u : 1u;
    rc_arena_release(&kept);
}


RC_TEST_THREADS(thread, arenas_on_many_threads_keep_apart)
{
    // Every block keeps its pattern, and once every arena is released, every page is back at the
    // top of the heap
    arena_churn churns[WORKERS] = {{.seed = 1}, {.seed = 2}, {.seed = 3}, {.seed = 4}};
    uint32_t top = top_of_heap();

    RC_CHECK_TRUE(run_workers(churn_arenas, churns, sizeof(churns[0])));
    for (uint32_t i = 0; i < WORKERS; i++) {
        RC_CHECK(churns[i].damaged, ==, 0);
    }

    RC_CHECK(top_of_heap(), ==, top);
}


// An arena a worker made, and a block it filled in it.
typedef struct first_arena {
    rc_arena arena;
    uint8_t *block;
} first_arena;


// Make an arena, perhaps the first since the page heap was torn down, and fill a block in it.
static void make_first_arena(void *context)
{
    first_arena *first = context;

    first->arena = rc_arena_make(ARENA_PAGE);
    first->block = rc_arena_alloc(&first->arena, 1000);
    fill_pattern(first->block, 0, 1000, first->arena.first);
}


RC_TEST_THREADS(thread, arenas_made_at_once_set_the_heap_up_once)
{
    // Had two threads each set a heap up, their arenas would start on the same page
    first_arena firsts[WORKERS] = {0};

    rc_arena_deinit();
    RC_CHECK_TRUE(run_workers(make_first_arena, firsts, sizeof(firsts[0])));
    for (uint32_t i = 0; i < WORKERS; i++) {
        RC_CHECK_TRUE(holds_pattern(firsts[i].block, 1000, firsts[i].arena.first));
        for (uint32_t j = 0; j < i; j++) {
            RC_CHECK(firsts[i].arena.first, !=, firsts[j].arena.first);
        }
    }

    for (uint32_t i = 0; i < WORKERS; i++) {
        rc_arena_release(&firsts[i].arena);
    }
}

// ---- shared arena ----

// A block a worker took from the shared arena, and the pattern it filled it with.
typedef struct shared_block {
    uint8_t *p;
    uint32_t size;
    uint32_t seed;
} shared_block;

// What one worker sharing the arena took and saw.
typedef struct arena_share {
    uint32_t num;
    uint32_t bad;   // zeroed blocks that were not, and misaligned ones
    shared_block blocks[SHARED_STEPS * 3];
} arena_share;

static rc_arena_ts shared_arena;
static arena_share shares[WORKERS];


// The first seed of the patterns of the worker that share is for.
static uint32_t share_seed(const arena_share *share)
{
    return (uint32_t)(share - shares + 1) << 24;
}


// Fill a block the worker took with the next pattern, and record it.
static void share_record(arena_share *share, uint8_t *p, uint32_t size)
{
    uint32_t seed = share_seed(share) + share->num;

    fill_pattern(p, 0, size, seed);
    share->blocks[share->num] = (shared_block) {.p = p, .size = size, .seed = seed};
    share->num++;
}


// Take a zeroed block.
static void share_zeroed(arena_share *share, uint32_t size)
{
    uint8_t *p = rc_arena_ts_alloc_zero(&shared_arena, size);

    share->bad += is_zeroed(p, size) ? 0u : 1u;
    share_record(share, p, size);
}


// Take a block aligned to align.
static void share_aligned(arena_share *share, uint32_t size, uint32_t align)
{
    uint8_t *p = rc_arena_ts_alloc_aligned(&shared_arena, size, align);

    share->bad += (uintptr_t)p % align == 0 ? 0u : 1u;
    share_record(share, p, size);
}


// Grow the worker's last block by extra zeroed bytes. Other workers' blocks have usually followed
// it, so it usually moves.
static void share_grow(arena_share *share, uint32_t extra)
{
    shared_block *last = &share->blocks[share->num - 1];
    uint32_t size = last->size + extra;

    last->p = rc_arena_ts_realloc_zero(&shared_arena, last->p, last->size, size);
    share->bad += is_zeroed(last->p + last->size, extra) ? 0u : 1u;
    fill_pattern(last->p, last->size, size, last->seed);
    last->size = size;
}


// Take three blocks under one lock, through the arena it gives.
static void share_locked(arena_share *share, uint32_t size)
{
    rc_arena *arena = rc_arena_ts_lock(&shared_arena);
    uint8_t *first = rc_arena_alloc(arena, size);
    uint8_t *second = rc_arena_alloc(arena, size);
    uint8_t *third = rc_arena_alloc(arena, size);

    rc_arena_ts_unlock(&shared_arena);
    share_record(share, first, size);
    share_record(share, second, size);
    share_record(share, third, size);
}


// Take a block and give it straight back, which works unless another worker took one between.
static void share_freed(uint32_t size)
{
    uint8_t *p = rc_arena_ts_alloc(&shared_arena, size);

    fill_pattern(p, 0, size, 0);
    (void)rc_arena_ts_free(&shared_arena, p, size);
}


// One random step with the shared arena.
static void share_step(arena_share *share, rc_random *random)
{
    uint32_t size = random_block_size(random);
    uint32_t action = share->num == 0 ? 0 : rc_random_next(random) % 6;

    switch (action) {
    case 0:
        share_record(share, rc_arena_ts_alloc(&shared_arena, size), size);
        break;
    case 1:
        share_zeroed(share, size);
        break;
    case 2:
        share_aligned(share, size, 256u << (rc_random_next(random) % 5));
        break;
    case 3:
        share_grow(share, size);
        break;
    case 4:
        share_locked(share, size);
        break;
    default:
        share_freed(size);
        break;
    }
}


// Take blocks from the shared arena every way there is.
static void share_arena(void *context)
{
    arena_share *share = context;
    rc_random random = rc_random_make(share_seed(share));

    for (uint32_t step = 0; step < SHARED_STEPS; step++) {
        share_step(share, &random);
    }
}


// The blocks the workers took that no longer hold their patterns.
static uint32_t damaged_shares(void)
{
    uint32_t damaged = 0;

    for (uint32_t i = 0; i < WORKERS; i++) {
        for (uint32_t j = 0; j < shares[i].num; j++) {
            damaged += holds_pattern(shares[i].blocks[j].p, shares[i].blocks[j].size, shares[i].blocks[j].seed) ? 0u : 1u;
        }
    }
    return damaged;
}


RC_TEST_THREADS(thread, shared_arena_hands_out_separate_blocks)
{
    // A small chunk size has the workers adding chunks to it often
    uint32_t top = top_of_heap();

    shared_arena = rc_arena_ts_make(ARENA_PAGE);
    rc_memset(shares, 0, sizeof(shares));
    RC_CHECK_TRUE(run_workers(share_arena, shares, sizeof(shares[0])));
    for (uint32_t i = 0; i < WORKERS; i++) {
        RC_CHECK(shares[i].bad, ==, 0);
    }

    RC_CHECK(damaged_shares(), ==, 0);
    rc_arena_ts_release(&shared_arena);
    RC_CHECK(top_of_heap(), ==, top);
}
