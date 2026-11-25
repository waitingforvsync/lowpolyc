// test_array_headers.c
//
// Tests for the per-type array headers in richc/array/: they all go in one file together, and
// with bytes.h, whose uint8_t arrays are a type of their own.

#include "richc/arena.h"
#include "richc/array/bitset.h"
#include "richc/array/f32.h"
#include "richc/array/f64.h"
#include "richc/array/i16.h"
#include "richc/array/i32.h"
#include "richc/array/i64.h"
#include "richc/array/i8.h"
#include "richc/array/mstr.h"
#include "richc/array/str.h"
#include "richc/array/u16.h"
#include "richc/array/u32.h"
#include "richc/array/u64.h"
#include "richc/array/u8.h"
#include "richc/bytes.h"
#include "richc/test.h"

// Each test runs against a fresh default arena.
RC_TEST_GROUP_DATA(array_headers) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(array_headers, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(array_headers, fix)
{
    rc_arena_release(&fix->a);
}


RC_TEST_STEP(array_headers, numbers, fix)
{
    rc_array_u8 u8s = {0};
    rc_array_i64 i64s = {0};
    rc_array_f64 f64s = {0};

    rc_array_u8_push(&u8s, 0xff, &fix->a);
    rc_array_i64_push(&i64s, INT64_MIN, &fix->a);
    rc_array_f64_push(&f64s, 0.5, &fix->a);
    RC_CHECK(rc_array_u8_get(&u8s, 0), ==, 0xff);
    RC_CHECK(rc_array_i64_get(&i64s, 0), ==, INT64_MIN);
    RC_CHECK(rc_array_f64_get(&f64s, 0), ==, 0.5);
}


RC_TEST_STEP(array_headers, strings_and_bitsets, fix)
{
    rc_array_str strs = {0};
    rc_array_mstr mstrs = {0};
    rc_array_bitset bitsets = {0};
    rc_bitset bits = rc_bitset_make(10, &fix->a);

    rc_bitset_set(&bits, 7);
    rc_array_str_push(&strs, RC_STR("word"), &fix->a);
    rc_array_mstr_push(&mstrs, rc_mstr_from_str(RC_STR("text"), 0, &fix->a), &fix->a);
    rc_array_bitset_push(&bitsets, bits, &fix->a);
    RC_CHECK_TRUE(rc_str_is_equal(rc_array_str_get(&strs, 0), RC_STR("word")));
    RC_CHECK_TRUE(rc_str_is_equal(rc_array_mstr_get(&mstrs, 0).view, RC_STR("text")));
    RC_CHECK_TRUE(rc_bitset_is_set(rc_array_bitset_at(&bitsets, 0), 7));
}
