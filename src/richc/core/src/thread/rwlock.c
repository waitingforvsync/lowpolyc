// rwlock.c
//
// The reader-writer lock, over two futexes, after the one in Rust's standard library. state_
// holds the number of readers in its low 30 bits, or all ones there for a writer, plus a flag for
// readers waiting and one for writers waiting. Readers sleep on state_ itself. Writers sleep on
// writer_wakes_, so that waking one writer does not wake every reader too.
//
// Readers do not take the lock while a writer waits, which is what puts writers first. Unlocking
// wakes a writer if one waits, and only the readers if none does.

#include "richc/thread/rwlock.h"

#include "platform/futex.h"
#include "richc/thread/spinlock.h"

#define RC_RWLOCK_READ_LOCKED 1u
#define RC_RWLOCK_MASK 0x3fffffffu
#define RC_RWLOCK_WRITE_LOCKED RC_RWLOCK_MASK
#define RC_RWLOCK_MAX_READERS (RC_RWLOCK_MASK - 1u)
#define RC_RWLOCK_READERS_WAITING 0x40000000u
#define RC_RWLOCK_WRITERS_WAITING 0x80000000u
#define RC_RWLOCK_WAITING (RC_RWLOCK_READERS_WAITING | RC_RWLOCK_WRITERS_WAITING)


static bool rc_rwlock_is_unlocked(uint32_t state)
{
    return (state & RC_RWLOCK_MASK) == 0;
}


static bool rc_rwlock_is_write_locked(uint32_t state)
{
    return (state & RC_RWLOCK_MASK) == RC_RWLOCK_WRITE_LOCKED;
}


static bool rc_rwlock_has_readers_waiting(uint32_t state)
{
    return (state & RC_RWLOCK_READERS_WAITING) != 0;
}


static bool rc_rwlock_has_writers_waiting(uint32_t state)
{
    return (state & RC_RWLOCK_WRITERS_WAITING) != 0;
}


// Whether a reader may join: no writer holds the lock, there is room for another reader, and
// nobody waits. A reader waiting means a writer was waiting when it slept, and still may be.
static bool rc_rwlock_is_read_lockable(uint32_t state)
{
    return (state & RC_RWLOCK_MASK) < RC_RWLOCK_MAX_READERS && (state & RC_RWLOCK_WAITING) == 0;
}


// Spin while a writer holds the lock and nobody waits, returning the state once that changes or
// spinning has gone on long enough.
static uint32_t rc_rwlock_spin_read(rc_rwlock *lock)
{
    uint32_t state = rc_atomic_u32_load(&lock->state_, RC_MEMORY_ORDER_RELAXED);

    for (uint32_t i = 0; i < RC_FUTEX_SPINS; i++) {
        if (!rc_rwlock_is_write_locked(state) || (state & RC_RWLOCK_WAITING) != 0) {
            break;
        }

        rc_spin_pause();
        state = rc_atomic_u32_load(&lock->state_, RC_MEMORY_ORDER_RELAXED);
    }

    return state;
}


// Spin while anyone holds the lock and no writer waits, returning the state once that changes or
// spinning has gone on long enough.
static uint32_t rc_rwlock_spin_write(rc_rwlock *lock)
{
    uint32_t state = rc_atomic_u32_load(&lock->state_, RC_MEMORY_ORDER_RELAXED);

    for (uint32_t i = 0; i < RC_FUTEX_SPINS; i++) {
        if (rc_rwlock_is_unlocked(state) || rc_rwlock_has_writers_waiting(state)) {
            break;
        }

        rc_spin_pause();
        state = rc_atomic_u32_load(&lock->state_, RC_MEMORY_ORDER_RELAXED);
    }

    return state;
}


// Wake one writer, returning whether there was one asleep to wake. False may also mean the OS
// did not say.
static bool rc_rwlock_wake_writer(rc_rwlock *lock)
{
    (void)rc_atomic_u32_fetch_add(&lock->writer_wakes_, 1, RC_MEMORY_ORDER_RELEASE);
    return rc_futex_wake_one(&lock->writer_wakes_);
}


// Wake whoever waits on the lock, which has just come free: one writer if any waits, or else
// every reader.
static void rc_rwlock_wake_writer_or_readers(rc_rwlock *lock, uint32_t state)
{
    RC_ASSERT(rc_rwlock_is_unlocked(state));

    if (state == RC_RWLOCK_WRITERS_WAITING) {
        uint32_t found = rc_atomic_u32_compare_exchange(&lock->state_, state, 0, RC_MEMORY_ORDER_RELAXED);

        if (found == state) {
            (void)rc_rwlock_wake_writer(lock);
            return;
        }

        // Readers may have started waiting too
        state = found;
    }

    // Leave the readers waiting and wake a writer. If there was no writer asleep to wake (or the
    // OS cannot say), wake the readers as well rather than risk leaving them asleep.
    if (state == RC_RWLOCK_WAITING) {
        if (rc_atomic_u32_compare_exchange(&lock->state_, state, RC_RWLOCK_READERS_WAITING, RC_MEMORY_ORDER_RELAXED) != state) {
            return;
        }

        if (rc_rwlock_wake_writer(lock)) {
            return;
        }

        state = RC_RWLOCK_READERS_WAITING;
    }

    if (state == RC_RWLOCK_READERS_WAITING) {
        if (rc_atomic_u32_compare_exchange(&lock->state_, state, 0, RC_MEMORY_ORDER_RELAXED) == state) {
            rc_futex_wake_all(&lock->state_);
        }
    }
}


// Take a read lock once the quick attempt has failed.
static void rc_rwlock_read_contended(rc_rwlock *lock)
{
    uint32_t state = rc_rwlock_spin_read(lock);

    for (;;) {
        if (rc_rwlock_is_read_lockable(state)) {
            uint32_t found =
                rc_atomic_u32_compare_exchange(&lock->state_, state, state + RC_RWLOCK_READ_LOCKED, RC_MEMORY_ORDER_ACQUIRE);

            if (found == state) {
                return;
            }

            state = found;
            continue;
        }

        RC_PANIC((state & RC_RWLOCK_MASK) != RC_RWLOCK_MAX_READERS);

        // Flag ourselves before sleeping, so the unlock that frees the lock knows to wake us
        if (!rc_rwlock_has_readers_waiting(state)) {
            uint32_t found =
                rc_atomic_u32_compare_exchange(&lock->state_, state, state | RC_RWLOCK_READERS_WAITING, RC_MEMORY_ORDER_RELAXED);

            if (found != state) {
                state = found;
                continue;
            }
        }

        (void)rc_futex_wait(&lock->state_, state | RC_RWLOCK_READERS_WAITING, RC_FUTEX_FOREVER);
        state = rc_rwlock_spin_read(lock);
    }
}


// Sleep as a waiting writer until woken, unless the lock has come free, or lost its writers
// waiting flag to an unlock, since this writer set it. The wake counter is read before the lock,
// so a wake that comes after the look changes the counter and the futex will not sleep through it.
static void rc_rwlock_writer_sleep(rc_rwlock *lock)
{
    uint32_t wakes = rc_atomic_u32_load(&lock->writer_wakes_, RC_MEMORY_ORDER_ACQUIRE);
    uint32_t state = rc_atomic_u32_load(&lock->state_, RC_MEMORY_ORDER_RELAXED);

    if (rc_rwlock_is_unlocked(state)) {
        return;
    }

    if (!rc_rwlock_has_writers_waiting(state)) {
        return;
    }

    (void)rc_futex_wait(&lock->writer_wakes_, wakes, RC_FUTEX_FOREVER);
}


// Take the write lock once the quick attempt has failed.
static void rc_rwlock_write_contended(rc_rwlock *lock)
{
    uint32_t state = rc_rwlock_spin_write(lock);
    uint32_t other_writers_waiting = 0;

    for (;;) {
        // Once we have slept, other writers may be asleep too, so taking the lock keeps their flag
        if (rc_rwlock_is_unlocked(state)) {
            uint32_t found = rc_atomic_u32_compare_exchange(&lock->state_, state,
                state | RC_RWLOCK_WRITE_LOCKED | other_writers_waiting, RC_MEMORY_ORDER_ACQUIRE);

            if (found == state) {
                return;
            }

            state = found;
            continue;
        }

        if (!rc_rwlock_has_writers_waiting(state)) {
            uint32_t found =
                rc_atomic_u32_compare_exchange(&lock->state_, state, state | RC_RWLOCK_WRITERS_WAITING, RC_MEMORY_ORDER_RELAXED);

            if (found != state) {
                state = found;
                continue;
            }
        }

        other_writers_waiting = RC_RWLOCK_WRITERS_WAITING;
        rc_rwlock_writer_sleep(lock);
        state = rc_rwlock_spin_write(lock);
    }
}


bool rc_rwlock_try_read_lock(rc_rwlock *lock)
{
    uint32_t state = rc_atomic_u32_load(&lock->state_, RC_MEMORY_ORDER_RELAXED);

    while (rc_rwlock_is_read_lockable(state)) {
        uint32_t found = rc_atomic_u32_compare_exchange(&lock->state_, state, state + RC_RWLOCK_READ_LOCKED, RC_MEMORY_ORDER_ACQUIRE);

        if (found == state) {
            return true;
        }

        state = found;
    }

    return false;
}


void rc_rwlock_read_lock(rc_rwlock *lock)
{
    uint32_t state = rc_atomic_u32_load(&lock->state_, RC_MEMORY_ORDER_RELAXED);

    if (rc_rwlock_is_read_lockable(state)) {
        if (rc_atomic_u32_compare_exchange(&lock->state_, state, state + RC_RWLOCK_READ_LOCKED, RC_MEMORY_ORDER_ACQUIRE) == state) {
            return;
        }
    }

    rc_rwlock_read_contended(lock);
}


void rc_rwlock_read_unlock(rc_rwlock *lock)
{
    uint32_t before = rc_atomic_u32_fetch_sub(&lock->state_, RC_RWLOCK_READ_LOCKED, RC_MEMORY_ORDER_RELEASE);
    uint32_t state = before - RC_RWLOCK_READ_LOCKED;

    // There was a reader to remove, and readers only wait while a writer does
    RC_ASSERT((before & RC_RWLOCK_MASK) - 1u < RC_RWLOCK_MAX_READERS);
    RC_ASSERT(!rc_rwlock_has_readers_waiting(state) || rc_rwlock_has_writers_waiting(state));

    // The last reader out wakes a waiting writer
    if (rc_rwlock_is_unlocked(state) && rc_rwlock_has_writers_waiting(state)) {
        rc_rwlock_wake_writer_or_readers(lock, state);
    }
}


bool rc_rwlock_try_write_lock(rc_rwlock *lock)
{
    uint32_t state = rc_atomic_u32_load(&lock->state_, RC_MEMORY_ORDER_RELAXED);

    while (rc_rwlock_is_unlocked(state)) {
        uint32_t found = rc_atomic_u32_compare_exchange(&lock->state_, state, state | RC_RWLOCK_WRITE_LOCKED, RC_MEMORY_ORDER_ACQUIRE);

        if (found == state) {
            return true;
        }

        state = found;
    }

    return false;
}


void rc_rwlock_write_lock(rc_rwlock *lock)
{
    if (rc_atomic_u32_compare_exchange(&lock->state_, 0, RC_RWLOCK_WRITE_LOCKED, RC_MEMORY_ORDER_ACQUIRE) != 0) {
        rc_rwlock_write_contended(lock);
    }
}


void rc_rwlock_write_unlock(rc_rwlock *lock)
{
    uint32_t before = rc_atomic_u32_fetch_sub(&lock->state_, RC_RWLOCK_WRITE_LOCKED, RC_MEMORY_ORDER_RELEASE);
    uint32_t state = before - RC_RWLOCK_WRITE_LOCKED;

    RC_ASSERT(rc_rwlock_is_write_locked(before));
    if ((state & RC_RWLOCK_WAITING) != 0) {
        rc_rwlock_wake_writer_or_readers(lock, state);
    }
}
