// test_bytes.c
//
// Tests for the byte containers' bridges to strings. The containers themselves are the array
// template, tested in test_array.c.

#include "richc/bytes.h"

#include "richc/test.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(bytes) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(bytes, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(bytes, fix)
{
    rc_arena_release(&fix->a);
}


// An array of capacity bytes holding "hi", with no terminator.
static rc_array_bytes hi_bytes(uint32_t capacity, rc_arena *arena)
{
    rc_array_bytes bytes = rc_array_bytes_make(capacity, arena);

    rc_array_bytes_push(&bytes, 'h', NULL);
    rc_array_bytes_push(&bytes, 'i', NULL);
    return bytes;
}


RC_TEST(bytes, as_str_shares_the_bytes)
{
    static uint8_t data[] = {'a', 'b', 'c'};
    rc_view_bytes view = RC_VIEW(data);
    rc_span_bytes span = RC_SPAN(data);
    rc_str s = rc_view_bytes_as_str(view);

    RC_CHECK(s, ==, RC_STR("abc"));
    RC_CHECK_TRUE(s.data == (const char *)data);
    RC_CHECK(rc_span_bytes_as_str(span), ==, RC_STR("abc"));
}


RC_TEST(bytes, as_str_of_nothing)
{
    rc_str s = rc_view_bytes_as_str((rc_view_bytes) {0});

    RC_CHECK(s.len, ==, 0u);
    RC_CHECK_FALSE(rc_str_is_valid(s));
}


RC_TEST_STEP(bytes, to_mstr_uses_spare_room, fix)
{
    // Room for the terminator already, so the buffer stays put
    rc_array_bytes bytes = hi_bytes(8, &fix->a);
    uint8_t *before = bytes.data;
    rc_mstr s = rc_array_bytes_to_mstr(&bytes, NULL);

    RC_CHECK(s.view, ==, RC_STR("hi"));
    RC_CHECK_TRUE(s.data[2] == '\0');
    RC_CHECK(s.cap, ==, 8u);
    RC_CHECK_TRUE((uint8_t *)s.data == before);
    RC_CHECK_FALSE(rc_array_bytes_is_valid(&bytes));
}


RC_TEST_STEP(bytes, to_mstr_grows_a_full_buffer, fix)
{
    rc_array_bytes bytes = hi_bytes(2, &fix->a);
    rc_mstr s = rc_array_bytes_to_mstr(&bytes, &fix->a);

    RC_CHECK(s.view, ==, RC_STR("hi"));
    RC_CHECK_TRUE(s.data[2] == '\0');
    RC_CHECK(s.cap, >=, 3u);
    RC_CHECK(bytes.num, ==, 0u);
}
