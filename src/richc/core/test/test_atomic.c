// test_atomic.c
//
// Tests for richc/thread/atomic.h, on one thread: each operation's result, wrapping at every
// width, and every order each operation takes. Threads racing on them are in test_thread.c.

#include "richc/test.h"
#include "richc/thread/atomic.h"

// The memory orders, for running an operation under each.
static const rc_memory_order every_order[] = {
    RC_MEMORY_ORDER_RELAXED,
    RC_MEMORY_ORDER_ACQUIRE,
    RC_MEMORY_ORDER_RELEASE,
    RC_MEMORY_ORDER_ACQ_REL,
    RC_MEMORY_ORDER_SEQ_CST
};

// Every operation on one integer type, at its extremes so its top bit is used, under seq_cst.
// Arithmetic wraps as it does for unsigned integers, at the type's own width.
#define CHECK_INTEGER(name, type, min, max) \
    do { \
        rc_atomic_##name atomic = rc_atomic_##name##_make(max); \
        \
        RC_CHECK(rc_atomic_##name##_load(&atomic, RC_MEMORY_ORDER_SEQ_CST), ==, max); \
        RC_CHECK(rc_atomic_##name##_fetch_add(&atomic, 1, RC_MEMORY_ORDER_SEQ_CST), ==, max); \
        RC_CHECK(rc_atomic_##name##_load(&atomic, RC_MEMORY_ORDER_SEQ_CST), ==, min); \
        RC_CHECK(rc_atomic_##name##_fetch_sub(&atomic, 1, RC_MEMORY_ORDER_SEQ_CST), ==, min); \
        RC_CHECK(rc_atomic_##name##_load(&atomic, RC_MEMORY_ORDER_SEQ_CST), ==, max); \
        \
        rc_atomic_##name##_store(&atomic, min, RC_MEMORY_ORDER_SEQ_CST); \
        RC_CHECK(rc_atomic_##name##_exchange(&atomic, max, RC_MEMORY_ORDER_SEQ_CST), ==, min); \
        RC_CHECK(rc_atomic_##name##_compare_exchange(&atomic, min, 0, RC_MEMORY_ORDER_SEQ_CST), ==, max); \
        RC_CHECK(rc_atomic_##name##_load(&atomic, RC_MEMORY_ORDER_SEQ_CST), ==, max); \
        RC_CHECK(rc_atomic_##name##_compare_exchange(&atomic, max, min, RC_MEMORY_ORDER_SEQ_CST), ==, max); \
        RC_CHECK(rc_atomic_##name##_load(&atomic, RC_MEMORY_ORDER_SEQ_CST), ==, min); \
        \
        rc_atomic_##name##_store(&atomic, (type)0x5a, RC_MEMORY_ORDER_SEQ_CST); \
        RC_CHECK(rc_atomic_##name##_fetch_and(&atomic, (type)0x0f, RC_MEMORY_ORDER_SEQ_CST), ==, 0x5a); \
        RC_CHECK(rc_atomic_##name##_fetch_or(&atomic, (type)0x30, RC_MEMORY_ORDER_SEQ_CST), ==, 0x0a); \
        RC_CHECK(rc_atomic_##name##_fetch_xor(&atomic, (type)0x3f, RC_MEMORY_ORDER_SEQ_CST), ==, 0x3a); \
        RC_CHECK(rc_atomic_##name##_load(&atomic, RC_MEMORY_ORDER_SEQ_CST), ==, 0x05); \
    } while (0)


RC_TEST(atomic, zeroed_holds_zero)
{
    rc_atomic_u8 u8 = {0};
    rc_atomic_i16 i16 = {0};
    rc_atomic_u32 u32 = {0};
    rc_atomic_i64 i64 = {0};
    rc_atomic_bool flag = {0};
    rc_atomic_ptr ptr = {0};

    RC_CHECK(rc_atomic_u8_load(&u8, RC_MEMORY_ORDER_RELAXED), ==, 0);
    RC_CHECK(rc_atomic_i16_load(&i16, RC_MEMORY_ORDER_RELAXED), ==, 0);
    RC_CHECK(rc_atomic_u32_load(&u32, RC_MEMORY_ORDER_RELAXED), ==, 0);
    RC_CHECK(rc_atomic_i64_load(&i64, RC_MEMORY_ORDER_RELAXED), ==, 0);
    RC_CHECK_FALSE(rc_atomic_bool_load(&flag, RC_MEMORY_ORDER_RELAXED));
    RC_CHECK_TRUE(rc_atomic_ptr_load(&ptr, RC_MEMORY_ORDER_RELAXED) == NULL);
}


RC_TEST(atomic, sizes_match_their_types)
{
    RC_CHECK((uint32_t)sizeof(rc_atomic_u8), ==, 1);
    RC_CHECK((uint32_t)sizeof(rc_atomic_i16), ==, 2);
    RC_CHECK((uint32_t)sizeof(rc_atomic_u32), ==, 4);
    RC_CHECK((uint32_t)sizeof(rc_atomic_i64), ==, 8);
    RC_CHECK((uint32_t)alignof(rc_atomic_i64), ==, 8);
    RC_CHECK((uint32_t)sizeof(rc_atomic_bool), ==, 1);
    RC_CHECK((uint32_t)sizeof(rc_atomic_ptr), ==, (uint32_t)sizeof(void *));
}


RC_TEST(atomic, unsigned_integers)
{
    CHECK_INTEGER(u8, uint8_t, 0, UINT8_MAX);
    CHECK_INTEGER(u16, uint16_t, 0, UINT16_MAX);
    CHECK_INTEGER(u32, uint32_t, 0, UINT32_MAX);
    CHECK_INTEGER(u64, uint64_t, 0, UINT64_MAX);
}


RC_TEST(atomic, signed_integers)
{
    CHECK_INTEGER(i8, int8_t, INT8_MIN, INT8_MAX);
    CHECK_INTEGER(i16, int16_t, INT16_MIN, INT16_MAX);
    CHECK_INTEGER(i32, int32_t, INT32_MIN, INT32_MAX);
    CHECK_INTEGER(i64, int64_t, INT64_MIN, INT64_MAX);
}


RC_TEST(atomic, top_halves_of_64_bits)
{
    rc_atomic_u64 atomic = rc_atomic_u64_make(0xffffffffu);

    RC_CHECK(rc_atomic_u64_fetch_add(&atomic, 1, RC_MEMORY_ORDER_SEQ_CST), ==, 0xffffffffu);
    RC_CHECK(rc_atomic_u64_load(&atomic, RC_MEMORY_ORDER_SEQ_CST), ==, 0x100000000u);
    RC_CHECK(rc_atomic_u64_compare_exchange(&atomic, 0, 1, RC_MEMORY_ORDER_SEQ_CST), ==, 0x100000000u);
    RC_CHECK(rc_atomic_u64_load(&atomic, RC_MEMORY_ORDER_SEQ_CST), ==, 0x100000000u);
}


RC_TEST(atomic, bools)
{
    rc_atomic_bool flag = rc_atomic_bool_make(true);

    RC_CHECK_TRUE(rc_atomic_bool_load(&flag, RC_MEMORY_ORDER_ACQUIRE));
    RC_CHECK_TRUE(rc_atomic_bool_exchange(&flag, false, RC_MEMORY_ORDER_ACQ_REL));
    RC_CHECK_FALSE(rc_atomic_bool_exchange(&flag, true, RC_MEMORY_ORDER_ACQ_REL));
    RC_CHECK_TRUE(rc_atomic_bool_compare_exchange(&flag, false, false, RC_MEMORY_ORDER_ACQ_REL));
    RC_CHECK_TRUE(rc_atomic_bool_load(&flag, RC_MEMORY_ORDER_ACQUIRE));
    RC_CHECK_TRUE(rc_atomic_bool_compare_exchange(&flag, true, false, RC_MEMORY_ORDER_ACQ_REL));
    RC_CHECK_FALSE(rc_atomic_bool_load(&flag, RC_MEMORY_ORDER_ACQUIRE));
    rc_atomic_bool_store(&flag, true, RC_MEMORY_ORDER_RELEASE);
    RC_CHECK_TRUE(rc_atomic_bool_load(&flag, RC_MEMORY_ORDER_ACQUIRE));
}


RC_TEST(atomic, pointers)
{
    int first = 1;
    int second = 2;
    rc_atomic_ptr ptr = rc_atomic_ptr_make(&first);

    RC_CHECK_TRUE(rc_atomic_ptr_load(&ptr, RC_MEMORY_ORDER_ACQUIRE) == &first);
    RC_CHECK_TRUE(rc_atomic_ptr_exchange(&ptr, &second, RC_MEMORY_ORDER_ACQ_REL) == &first);
    RC_CHECK_TRUE(rc_atomic_ptr_compare_exchange(&ptr, &first, NULL, RC_MEMORY_ORDER_ACQ_REL) == &second);
    RC_CHECK_TRUE(rc_atomic_ptr_load(&ptr, RC_MEMORY_ORDER_ACQUIRE) == &second);
    RC_CHECK_TRUE(rc_atomic_ptr_compare_exchange(&ptr, &second, NULL, RC_MEMORY_ORDER_ACQ_REL) == &second);
    RC_CHECK_TRUE(rc_atomic_ptr_load(&ptr, RC_MEMORY_ORDER_ACQUIRE) == NULL);
    rc_atomic_ptr_store(&ptr, &first, RC_MEMORY_ORDER_RELEASE);
    RC_CHECK_TRUE(rc_atomic_ptr_load(&ptr, RC_MEMORY_ORDER_ACQUIRE) == &first);
}


RC_TEST(atomic, every_order)
{
    rc_atomic_u32 atomic = {0};
    uint32_t expected = 0;

    for (uint32_t i = 0; i < RC_COUNT(every_order); i++) {
        rc_memory_order order = every_order[i];

        if (rc_atomic_is_load_order_(order)) {
            RC_CHECK(rc_atomic_u32_load(&atomic, order), ==, expected);
        }

        if (rc_atomic_is_store_order_(order)) {
            rc_atomic_u32_store(&atomic, ++expected, order);
        }

        RC_CHECK(rc_atomic_u32_exchange(&atomic, expected + 1, order), ==, expected);
        RC_CHECK(rc_atomic_u32_compare_exchange(&atomic, expected, 0, order), ==, expected + 1);
        RC_CHECK(rc_atomic_u32_compare_exchange(&atomic, expected + 1, expected, order), ==, expected + 1);
        RC_CHECK(rc_atomic_u32_fetch_add(&atomic, 2, order), ==, expected);
        RC_CHECK(rc_atomic_u32_fetch_sub(&atomic, 1, order), ==, expected + 2);
        RC_CHECK(rc_atomic_u32_fetch_or(&atomic, 0, order), ==, expected + 1);
        RC_CHECK(rc_atomic_u32_fetch_and(&atomic, UINT32_MAX, order), ==, expected + 1);
        RC_CHECK(rc_atomic_u32_fetch_xor(&atomic, 0, order), ==, expected + 1);
        expected++;
        rc_atomic_thread_fence(order);
        rc_atomic_signal_fence(order);
    }

    RC_CHECK(rc_atomic_u32_load(&atomic, RC_MEMORY_ORDER_RELAXED), ==, expected);
}


RC_TEST(atomic, compare_exchange_loop)
{
    rc_atomic_i32 highest = rc_atomic_i32_make(INT32_MIN);
    const int32_t values[] = {-5, 12, 3, 40, -100, 39};

    // The usual loop: retry with the value the failed swap found until the swap lands or is moot
    for (uint32_t i = 0; i < RC_COUNT(values); i++) {
        int32_t held = rc_atomic_i32_load(&highest, RC_MEMORY_ORDER_RELAXED);

        while (held < values[i]) {
            int32_t found = rc_atomic_i32_compare_exchange(&highest, held, values[i], RC_MEMORY_ORDER_RELAXED);

            if (found == held) {
                break;
            }
            held = found;
        }
    }

    RC_CHECK(rc_atomic_i32_load(&highest, RC_MEMORY_ORDER_RELAXED), ==, 40);
}
