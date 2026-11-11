// test_mstr.c
//
// Tests for the rc_mstr growable string.

#include "richc/mstr.h"

#include "richc/arena.h"
#include "richc/test.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(mstr) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(mstr, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(mstr, fix)
{
    rc_arena_release(&fix->a);
}


// True when the terminator invariant holds: room for it, and in place.
static bool is_terminated(const rc_mstr *s)
{
    return s->len + 1 <= s->cap && s->data[s->len] == '\0';
}


// ---- construction ----

RC_TEST_STEP(mstr, make, fix)
{
    rc_mstr s = rc_mstr_make(16, &fix->a);

    RC_CHECK_TRUE(rc_mstr_is_valid(&s));
    RC_CHECK_TRUE(rc_mstr_is_empty(&s));
    RC_CHECK(s.len, ==, 0u);
    RC_CHECK(s.cap, ==, 16u);
    RC_CHECK(s.view, ==, RC_STR(""));
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, make_one_byte, fix)
{
    // Just room for the terminator
    rc_mstr s = rc_mstr_make(1, &fix->a);

    RC_CHECK_TRUE(rc_mstr_is_valid(&s));
    RC_CHECK(s.cap, ==, 1u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, make_zero_cap, fix)
{
    // Capacity 0 gives the invalid string, and allocates nothing
    rc_arena_mark before = rc_arena_save(&fix->a);
    rc_mstr s = rc_mstr_make(0, &fix->a);
    rc_arena_mark after = rc_arena_save(&fix->a);

    RC_CHECK_FALSE(rc_mstr_is_valid(&s));
    RC_CHECK(s.cap, ==, 0u);
    RC_CHECK_TRUE(s.data == NULL);
    RC_CHECK(after.top, ==, before.top);
}


RC_TEST_STEP(mstr, zero_is_invalid, fix)
{
    rc_mstr s = {0};

    (void)fix;
    RC_CHECK_FALSE(rc_mstr_is_valid(&s));
    RC_CHECK_TRUE(rc_mstr_is_empty(&s));
    RC_CHECK_FALSE(rc_str_is_valid(s.view));
}


RC_TEST_STEP(mstr, from_cstr, fix)
{
    rc_mstr s = rc_mstr_from_cstr("hello", 0, &fix->a);

    RC_CHECK(s.view, ==, RC_STR("hello"));
    RC_CHECK(s.len, ==, 5u);
    RC_CHECK(s.cap, ==, 6u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, from_cstr_minimum_capacity, fix)
{
    rc_mstr s = rc_mstr_from_cstr("hi", 10, &fix->a);

    RC_CHECK(s.view, ==, RC_STR("hi"));
    RC_CHECK(s.len, ==, 2u);
    RC_CHECK(s.cap, ==, 10u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, from_cstr_null, fix)
{
    rc_mstr s = rc_mstr_from_cstr(NULL, 4, &fix->a);

    RC_CHECK_FALSE(rc_mstr_is_valid(&s));
    RC_CHECK(s.cap, ==, 0u);
}


RC_TEST_STEP(mstr, from_cstr_empty, fix)
{
    // An empty source still gives a valid string
    rc_mstr s = rc_mstr_from_cstr("", 0, &fix->a);

    RC_CHECK_TRUE(rc_mstr_is_valid(&s));
    RC_CHECK(s.cap, ==, 1u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, from_str, fix)
{
    rc_mstr s = rc_mstr_from_str(RC_STR("world"), 2, &fix->a);

    RC_CHECK(s.view, ==, RC_STR("world"));
    RC_CHECK(s.len, ==, 5u);
    RC_CHECK(s.cap, ==, 6u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, from_str_unterminated, fix)
{
    // A view into the middle of a string is copied and terminated
    rc_mstr s = rc_mstr_from_str(rc_str_substr(RC_STR("hello world"), 3, 8), 0, &fix->a);

    RC_CHECK(s.view, ==, RC_STR("lo wo"));
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, from_str_invalid, fix)
{
    rc_mstr s = rc_mstr_from_str(rc_str_from_cstr(NULL), 4, &fix->a);

    RC_CHECK_FALSE(rc_mstr_is_valid(&s));
}


// ---- predicates ----

RC_TEST_STEP(mstr, is_empty, fix)
{
    rc_mstr s = rc_mstr_make(8, &fix->a);

    RC_CHECK_TRUE(rc_mstr_is_empty(&s));
    rc_mstr_append_char(&s, 'x', &fix->a);
    RC_CHECK_FALSE(rc_mstr_is_empty(&s));
}


RC_TEST_STEP(mstr, view_is_cstr, fix)
{
    // The terminator means the view never needs copying
    rc_mstr s = rc_mstr_from_cstr("abc", 0, &fix->a);

    rc_mstr_append(&s, RC_STR("def"), &fix->a);
    RC_CHECK_TRUE(rc_str_as_cstr(s.view, NULL, 0) == s.data);
}


// ---- mutation ----

RC_TEST_STEP(mstr, reset, fix)
{
    rc_mstr s = rc_mstr_from_cstr("hello", 0, &fix->a);
    uint32_t cap = s.cap;

    rc_mstr_reset(&s);
    RC_CHECK(s.len, ==, 0u);
    RC_CHECK_TRUE(rc_mstr_is_empty(&s));
    RC_CHECK(s.cap, ==, cap);
    RC_CHECK(s.view, ==, RC_STR(""));
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, reset_invalid, fix)
{
    rc_mstr s = {0};

    (void)fix;
    rc_mstr_reset(&s);
    RC_CHECK_FALSE(rc_mstr_is_valid(&s));
}


RC_TEST_STEP(mstr, reserve_grows, fix)
{
    rc_mstr s = rc_mstr_from_cstr("abc", 0, &fix->a);

    rc_mstr_reserve(&s, 100, &fix->a);
    RC_CHECK(s.cap, ==, 100u);
    RC_CHECK(s.len, ==, 3u);
    RC_CHECK(s.view, ==, RC_STR("abc"));
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, reserve_noop, fix)
{
    rc_mstr s = rc_mstr_make(50, &fix->a);
    const char *before = s.data;

    rc_mstr_reserve(&s, 10, &fix->a);
    RC_CHECK(s.cap, ==, 50u);
    RC_CHECK_TRUE(s.data == before);
}


RC_TEST_STEP(mstr, reserve_invalid, fix)
{
    // Reserving gives an invalid string its buffer, terminated
    rc_mstr s = {0};

    rc_mstr_reserve(&s, 4, &fix->a);
    RC_CHECK_TRUE(rc_mstr_is_valid(&s));
    RC_CHECK(s.cap, ==, 4u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, reserve_moves_when_not_last, fix)
{
    // With another allocation after it, growth copies to a new buffer
    rc_mstr s = rc_mstr_from_cstr("abc", 0, &fix->a);
    const char *before = s.data;

    rc_arena_alloc(&fix->a, 16);
    rc_mstr_reserve(&s, 64, &fix->a);
    RC_CHECK_TRUE(s.data != before);
    RC_CHECK(s.view, ==, RC_STR("abc"));
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, append, fix)
{
    rc_mstr s = rc_mstr_make(0, &fix->a);

    rc_mstr_append(&s, RC_STR("foo"), &fix->a);
    rc_mstr_append(&s, RC_STR("bar"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("foobar"));
    RC_CHECK(s.len, ==, 6u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, append_to_invalid, fix)
{
    // The zero value allocates on its first append
    rc_mstr s = {0};

    rc_mstr_append(&s, RC_STR("hello"), &fix->a);
    RC_CHECK_TRUE(rc_mstr_is_valid(&s));
    RC_CHECK(s.view, ==, RC_STR("hello"));
    RC_CHECK(s.cap, ==, 8u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, append_empty, fix)
{
    rc_mstr s = rc_mstr_from_cstr("x", 0, &fix->a);

    rc_mstr_append(&s, RC_STR(""), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("x"));
    RC_CHECK(s.len, ==, 1u);
}


RC_TEST_STEP(mstr, append_empty_to_invalid, fix)
{
    // Appending nothing allocates nothing
    rc_mstr s = {0};

    rc_mstr_append(&s, RC_STR(""), &fix->a);
    RC_CHECK_FALSE(rc_mstr_is_valid(&s));
}


RC_TEST_STEP(mstr, append_fills_exactly, fix)
{
    // Text that exactly fills the buffer, terminator included, does not grow it
    rc_mstr s = rc_mstr_make(4, &fix->a);

    rc_mstr_append(&s, RC_STR("abc"), &fix->a);
    RC_CHECK(s.cap, ==, 4u);
    RC_CHECK_TRUE(is_terminated(&s));
    rc_mstr_append_char(&s, 'd', &fix->a);
    RC_CHECK(s.cap, ==, 8u);
    RC_CHECK(s.view, ==, RC_STR("abcd"));
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, append_growth, fix)
{
    rc_mstr s = rc_mstr_make(0, &fix->a);

    for (uint32_t i = 0; i < 100; i++) {
        rc_mstr_append(&s, RC_STR("ab"), &fix->a);
    }
    RC_CHECK(s.len, ==, 200u);
    RC_CHECK_TRUE(is_terminated(&s));
    for (uint32_t i = 0; i < 100; i++) {
        RC_CHECK(rc_str_substr(s.view, 2 * i, 2 * i + 2), ==, RC_STR("ab"));
    }
}


RC_TEST_STEP(mstr, append_growth_is_geometric, fix)
{
    // Doubling means a hundred appends reallocate only a handful of times
    rc_mstr s = {0};
    uint32_t grows = 0;

    for (uint32_t i = 0; i < 100; i++) {
        uint32_t cap = s.cap;

        rc_mstr_append_char(&s, 'z', &fix->a);
        if (s.cap != cap) {
            grows++;
        }
    }
    RC_CHECK(grows, <=, 5u);
}


RC_TEST_STEP(mstr, append_own_view, fix)
{
    // The view is read before the string grows over it
    rc_mstr s = rc_mstr_from_cstr("abc", 0, &fix->a);

    rc_mstr_append(&s, s.view, &fix->a);
    RC_CHECK(s.view, ==, RC_STR("abcabc"));
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, append_char, fix)
{
    rc_mstr s = rc_mstr_make(0, &fix->a);

    rc_mstr_append_char(&s, 'h', &fix->a);
    rc_mstr_append_char(&s, 'i', &fix->a);
    RC_CHECK(s.view, ==, RC_STR("hi"));
    RC_CHECK(s.len, ==, 2u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, append_char_growth, fix)
{
    rc_mstr s = rc_mstr_make(0, &fix->a);

    for (uint32_t i = 0; i < 50; i++) {
        rc_mstr_append_char(&s, 'z', &fix->a);
    }
    RC_CHECK(s.len, ==, 50u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, append_n, fix)
{
    rc_mstr s = {0};

    rc_mstr_append_n(&s, '-', 3, &fix->a);
    rc_mstr_append_n(&s, ' ', 0, &fix->a);
    rc_mstr_append_n(&s, '=', 1, &fix->a);
    RC_CHECK(s.view, ==, RC_STR("---="));
    RC_CHECK(s.len, ==, 4u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, append_n_large, fix)
{
    rc_mstr s = rc_mstr_from_cstr("[", 0, &fix->a);

    rc_mstr_append_n(&s, '.', 1000, &fix->a);
    rc_mstr_append_char(&s, ']', &fix->a);
    RC_CHECK(s.len, ==, 1002u);
    RC_CHECK(rc_str_find_first(s.view, RC_STR("]")), ==, 1001u);
    RC_CHECK(rc_str_find_last(s.view, RC_STR("..")), ==, 999u);
    RC_CHECK_TRUE(is_terminated(&s));
}


// ---- number appends ----

RC_TEST_STEP(mstr, append_u64, fix)
{
    rc_mstr s = {0};

    rc_mstr_append_u64(&s, 0, &fix->a);
    rc_mstr_append_char(&s, ' ', &fix->a);
    rc_mstr_append_u64(&s, 10, &fix->a);
    rc_mstr_append_char(&s, ' ', &fix->a);
    rc_mstr_append_u64(&s, UINT64_MAX, &fix->a);
    RC_CHECK(s.view, ==, RC_STR("0 10 18446744073709551615"));
}


RC_TEST_STEP(mstr, append_i64, fix)
{
    rc_mstr s = {0};
    rc_mstr t = {0};
    rc_mstr u = {0};

    rc_mstr_append_i64(&s, 0, &fix->a);
    rc_mstr_append_i64(&s, -7, &fix->a);
    RC_CHECK(s.view, ==, RC_STR("0-7"));
    rc_mstr_append_i64(&t, INT64_MAX, &fix->a);
    RC_CHECK(t.view, ==, RC_STR("9223372036854775807"));

    // INT64_MIN cannot be negated in signed arithmetic
    rc_mstr_append_i64(&u, INT64_MIN, &fix->a);
    RC_CHECK(u.view, ==, RC_STR("-9223372036854775808"));
}


RC_TEST_STEP(mstr, append_narrow_widths, fix)
{
    rc_mstr s = {0};

    rc_mstr_append_i32(&s, INT32_MIN, &fix->a);
    rc_mstr_append_char(&s, ' ', &fix->a);
    rc_mstr_append_i32(&s, INT32_MAX, &fix->a);
    rc_mstr_append_char(&s, ' ', &fix->a);
    rc_mstr_append_u32(&s, UINT32_MAX, &fix->a);
    RC_CHECK(s.view, ==, RC_STR("-2147483648 2147483647 4294967295"));
}


RC_TEST_STEP(mstr, append_hex, fix)
{
    // Every variant always gives its full digit count
    rc_mstr s = {0};
    rc_mstr z = {0};
    rc_mstr m = {0};

    rc_mstr_append_hex8(&s, 0x0A, &fix->a);
    rc_mstr_append_char(&s, ' ', &fix->a);
    rc_mstr_append_hex16(&s, 0x0900, &fix->a);
    rc_mstr_append_char(&s, ' ', &fix->a);
    rc_mstr_append_hex32(&s, 0xDEADBEEF, &fix->a);
    rc_mstr_append_char(&s, ' ', &fix->a);
    rc_mstr_append_hex64(&s, 0x0123456789ABCDEFull, &fix->a);
    RC_CHECK(s.view, ==, RC_STR("0A 0900 DEADBEEF 0123456789ABCDEF"));

    rc_mstr_append_hex8(&z, 0, &fix->a);
    rc_mstr_append_hex16(&z, 0, &fix->a);
    rc_mstr_append_hex32(&z, 0, &fix->a);
    rc_mstr_append_hex64(&z, 0, &fix->a);
    RC_CHECK(z.view, ==, RC_STR("000000000000000000000000000000"));

    rc_mstr_append_hex8(&m, UINT8_MAX, &fix->a);
    rc_mstr_append_hex64(&m, UINT64_MAX, &fix->a);
    RC_CHECK(m.view, ==, RC_STR("FFFFFFFFFFFFFFFFFF"));
}


// ---- replace ----

RC_TEST_STEP(mstr, replace_delete, fix)
{
    rc_mstr s = rc_mstr_from_cstr("aXbXc", 0, &fix->a);

    rc_mstr_replace(&s, RC_STR("X"), RC_STR(""), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("abc"));
    RC_CHECK(s.len, ==, 3u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, replace_shrinking, fix)
{
    rc_mstr s = rc_mstr_from_cstr("<br><br>x<br>", 0, &fix->a);

    rc_mstr_replace(&s, RC_STR("<br>"), RC_STR("\n"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("\n\nx\n"));
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, replace_all_of_it, fix)
{
    rc_mstr s = rc_mstr_from_cstr("abcabc", 0, &fix->a);

    rc_mstr_replace(&s, RC_STR("abc"), RC_STR(""), &fix->a);
    RC_CHECK(s.view, ==, RC_STR(""));
    RC_CHECK_TRUE(rc_mstr_is_valid(&s));
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, replace_same_length, fix)
{
    rc_mstr s = rc_mstr_from_cstr("a.b.c", 0, &fix->a);
    uint32_t cap = s.cap;

    rc_mstr_replace(&s, RC_STR("."), RC_STR("-"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("a-b-c"));
    RC_CHECK(s.cap, ==, cap);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, replace_longer, fix)
{
    rc_mstr s = rc_mstr_from_cstr("a.b", 0, &fix->a);

    rc_mstr_replace(&s, RC_STR("."), RC_STR("___"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("a___b"));
    RC_CHECK(s.len, ==, 5u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, replace_grows_buffer, fix)
{
    // The result (8) outgrows the starting capacity (5)
    rc_mstr s = rc_mstr_from_cstr("xxxx", 0, &fix->a);

    rc_mstr_replace(&s, RC_STR("x"), RC_STR("yy"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("yyyyyyyy"));
    RC_CHECK(s.len, ==, 8u);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, replace_growing_in_place, fix)
{
    // Spare capacity means no reallocation, so the shuffle is all in place
    rc_mstr s = rc_mstr_from_cstr("1,22,333", 64, &fix->a);
    const char *before = s.data;

    rc_mstr_replace(&s, RC_STR(","), RC_STR(" -> "), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("1 -> 22 -> 333"));
    RC_CHECK_TRUE(s.data == before);
    RC_CHECK_TRUE(is_terminated(&s));
}


RC_TEST_STEP(mstr, replace_boundaries, fix)
{
    rc_mstr s = rc_mstr_from_cstr("XaX", 0, &fix->a);
    rc_mstr t = rc_mstr_from_cstr("XaX", 0, &fix->a);

    rc_mstr_replace(&s, RC_STR("X"), RC_STR("YY"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("YYaYY"));
    rc_mstr_replace(&t, RC_STR("X"), RC_STR(""), &fix->a);
    RC_CHECK(t.view, ==, RC_STR("a"));
}


RC_TEST_STEP(mstr, replace_adjacent, fix)
{
    rc_mstr s = rc_mstr_from_cstr("XXX", 0, &fix->a);
    rc_mstr t = rc_mstr_from_cstr("XXX", 0, &fix->a);

    rc_mstr_replace(&s, RC_STR("X"), RC_STR("ab"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("ababab"));
    rc_mstr_replace(&t, RC_STR("XX"), RC_STR("-"), &fix->a);
    RC_CHECK(t.view, ==, RC_STR("-X"));
}


RC_TEST_STEP(mstr, replace_non_overlapping, fix)
{
    // Matches are taken left to right, so "aaa" holds one "aa", not two
    rc_mstr s = rc_mstr_from_cstr("aaa", 0, &fix->a);
    rc_mstr t = rc_mstr_from_cstr("aaa", 0, &fix->a);

    rc_mstr_replace(&s, RC_STR("aa"), RC_STR("b"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("ba"));
    rc_mstr_replace(&t, RC_STR("aa"), RC_STR("bbb"), &fix->a);
    RC_CHECK(t.view, ==, RC_STR("bbba"));
}


RC_TEST_STEP(mstr, replace_replacement_contains_find, fix)
{
    // The new text is not searched again
    rc_mstr s = rc_mstr_from_cstr("a-b", 0, &fix->a);

    rc_mstr_replace(&s, RC_STR("-"), RC_STR("--"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("a--b"));
}


RC_TEST_STEP(mstr, replace_not_found, fix)
{
    rc_mstr s = rc_mstr_from_cstr("hello", 0, &fix->a);

    rc_mstr_replace(&s, RC_STR("z"), RC_STR("Q"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("hello"));
    RC_CHECK(s.len, ==, 5u);
}


RC_TEST_STEP(mstr, replace_empty_find, fix)
{
    rc_mstr s = rc_mstr_from_cstr("hello", 0, &fix->a);

    rc_mstr_replace(&s, RC_STR(""), RC_STR("Q"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("hello"));
}


RC_TEST_STEP(mstr, replace_in_empty, fix)
{
    rc_mstr s = rc_mstr_from_cstr("", 0, &fix->a);
    rc_mstr z = {0};

    rc_mstr_replace(&s, RC_STR("x"), RC_STR("y"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR(""));
    rc_mstr_replace(&z, RC_STR("x"), RC_STR("y"), &fix->a);
    RC_CHECK_FALSE(rc_mstr_is_valid(&z));
}


// ---- teardown ----

RC_TEST_STEP(mstr, release, fix)
{
    rc_mstr s = rc_mstr_from_cstr("hello world", 0, &fix->a);

    rc_mstr_append(&s, RC_STR("!"), &fix->a);
    rc_mstr_release(&s, &fix->a);
    RC_CHECK_FALSE(rc_mstr_is_valid(&s));
    RC_CHECK(s.len, ==, 0u);
    RC_CHECK(s.cap, ==, 0u);
    RC_CHECK_TRUE(s.data == NULL);

    // A second release of the zeroed string does nothing
    rc_mstr_release(&s, &fix->a);
    RC_CHECK_FALSE(rc_mstr_is_valid(&s));
}


RC_TEST_STEP(mstr, release_reclaims_last, fix)
{
    // As the arena's last allocation, the buffer goes straight back
    rc_arena_mark before = rc_arena_save(&fix->a);
    rc_mstr s = rc_mstr_from_cstr("hello", 0, &fix->a);

    rc_mstr_release(&s, &fix->a);
    RC_CHECK(rc_arena_save(&fix->a).top, ==, before.top);
}
