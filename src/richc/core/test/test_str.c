// test_str.c
//
// Tests for the rc_str string view.

#include "richc/str.h"

#include "richc/test.h"

// RC_STR_INIT must work in a static initialiser, where RC_STR cannot on MSVC.
static const rc_str static_hello = RC_STR_INIT("hello");


// ---- construction ----

RC_TEST(str, literal)
{
    rc_str s = RC_STR("hello");
    rc_str empty = RC_STR("");

    RC_CHECK(s.len, ==, 5u);
    RC_CHECK(s, ==, rc_str_from_cstr("hello"));
    RC_CHECK(empty.len, ==, 0u);
    RC_CHECK_TRUE(rc_str_is_valid(empty));
}


RC_TEST(str, literal_static_init)
{
    RC_CHECK(static_hello, ==, RC_STR("hello"));
}


RC_TEST(str, from_ptr)
{
    const char text[] = {'a', 'b', 'c'};
    rc_str s = rc_str_from_ptr(text, 2);

    RC_CHECK_TRUE(s.data == text);
    RC_CHECK(s, ==, RC_STR("ab"));
}


RC_TEST(str, from_cstr)
{
    rc_str s = rc_str_from_cstr("hello");

    RC_CHECK(s.len, ==, 5u);
    RC_CHECK(s, ==, RC_STR("hello"));
}


RC_TEST(str, from_cstr_null)
{
    rc_str s = rc_str_from_cstr(NULL);

    RC_CHECK_FALSE(rc_str_is_valid(s));
    RC_CHECK_TRUE(rc_str_is_empty(s));
    RC_CHECK(s.len, ==, 0u);
}


RC_TEST(str, from_cstr_empty)
{
    rc_str s = rc_str_from_cstr("");

    RC_CHECK_TRUE(rc_str_is_valid(s));
    RC_CHECK_TRUE(rc_str_is_empty(s));
}


// ---- predicates ----

RC_TEST(str, is_valid)
{
    RC_CHECK_TRUE(rc_str_is_valid(RC_STR("x")));
    RC_CHECK_TRUE(rc_str_is_valid(RC_STR("")));
    RC_CHECK_FALSE(rc_str_is_valid(rc_str_from_cstr(NULL)));
    RC_CHECK_FALSE(rc_str_is_valid((rc_str) {0}));
}


RC_TEST(str, is_empty)
{
    RC_CHECK_FALSE(rc_str_is_empty(RC_STR("x")));
    RC_CHECK_TRUE(rc_str_is_empty(RC_STR("")));
    RC_CHECK_TRUE(rc_str_is_empty(rc_str_from_cstr(NULL)));
}


RC_TEST(str, is_equal)
{
    RC_CHECK_TRUE(rc_str_is_equal(RC_STR("abc"), RC_STR("abc")));
    RC_CHECK_FALSE(rc_str_is_equal(RC_STR("abc"), RC_STR("abd")));
    RC_CHECK_FALSE(rc_str_is_equal(RC_STR("abc"), RC_STR("ab")));
    RC_CHECK_FALSE(rc_str_is_equal(RC_STR("abc"), RC_STR("ABC")));
}


RC_TEST(str, is_equal_empty)
{
    // Empty views are equal wherever they point
    rc_str e1 = rc_str_first(RC_STR("abc"), 0);
    rc_str e2 = rc_str_first(RC_STR("xyz"), 0);

    RC_CHECK_TRUE(rc_str_is_equal(e1, e2));
    RC_CHECK_TRUE(rc_str_is_equal(RC_STR(""), e1));
}


RC_TEST(str, is_equal_insensitive)
{
    RC_CHECK_TRUE(rc_str_is_equal_insensitive(RC_STR("Hello"), RC_STR("hELLO")));
    RC_CHECK_FALSE(rc_str_is_equal_insensitive(RC_STR("Hello"), RC_STR("World")));
    RC_CHECK_FALSE(rc_str_is_equal_insensitive(RC_STR("abc"), RC_STR("ab")));
    RC_CHECK_TRUE(rc_str_is_equal_insensitive(RC_STR(""), RC_STR("")));
}


RC_TEST(str, is_equal_insensitive_letters_only)
{
    // '@' and '[' sit either side of A-Z, '`' and '{' either side of a-z
    RC_CHECK_FALSE(rc_str_is_equal_insensitive(RC_STR("@"), RC_STR("`")));
    RC_CHECK_FALSE(rc_str_is_equal_insensitive(RC_STR("["), RC_STR("{")));
    RC_CHECK_TRUE(rc_str_is_equal_insensitive(RC_STR("AZ"), RC_STR("az")));
    RC_CHECK_TRUE(rc_str_is_equal_insensitive(RC_STR("1-2"), RC_STR("1-2")));
}


RC_TEST(str, compare)
{
    RC_CHECK(rc_str_compare(RC_STR("abc"), RC_STR("abc")), ==, 0);
    RC_CHECK(rc_str_compare(RC_STR("abc"), RC_STR("abd")), <, 0);
    RC_CHECK(rc_str_compare(RC_STR("abd"), RC_STR("abc")), >, 0);
}


RC_TEST(str, compare_length)
{
    // A prefix sorts before the longer string
    RC_CHECK(rc_str_compare(RC_STR("ab"), RC_STR("abc")), <, 0);
    RC_CHECK(rc_str_compare(RC_STR("abc"), RC_STR("ab")), >, 0);
    RC_CHECK(rc_str_compare(RC_STR(""), RC_STR("a")), <, 0);
    RC_CHECK(rc_str_compare(RC_STR(""), RC_STR("")), ==, 0);
}


RC_TEST(str, compare_unsigned_bytes)
{
    // Bytes compare unsigned, so 0x80 and up sort after ASCII whatever char is
    RC_CHECK(rc_str_compare(RC_STR("\x80"), RC_STR("a")), >, 0);
    RC_CHECK(rc_str_compare_insensitive(RC_STR("\x80"), RC_STR("a")), >, 0);
}


RC_TEST(str, compare_insensitive)
{
    RC_CHECK(rc_str_compare_insensitive(RC_STR("ABC"), RC_STR("abc")), ==, 0);
    RC_CHECK(rc_str_compare_insensitive(RC_STR("abc"), RC_STR("ABD")), <, 0);
    RC_CHECK(rc_str_compare_insensitive(RC_STR("ABD"), RC_STR("abc")), >, 0);
    RC_CHECK(rc_str_compare_insensitive(RC_STR("ab"), RC_STR("ABC")), <, 0);
    RC_CHECK(rc_str_compare_insensitive(RC_STR("ABC"), RC_STR("ab")), >, 0);
}


// ---- slicing ----

RC_TEST(str, first)
{
    RC_CHECK(rc_str_first(RC_STR("hello"), 3), ==, RC_STR("hel"));
    RC_CHECK(rc_str_first(RC_STR("hello"), 0), ==, RC_STR(""));
    RC_CHECK(rc_str_first(RC_STR("hello"), 5), ==, RC_STR("hello"));
    RC_CHECK(rc_str_first(RC_STR("hello"), 99), ==, RC_STR("hello"));
}


RC_TEST(str, last)
{
    RC_CHECK(rc_str_last(RC_STR("hello"), 2), ==, RC_STR("lo"));
    RC_CHECK(rc_str_last(RC_STR("hello"), 0), ==, RC_STR(""));
    RC_CHECK(rc_str_last(RC_STR("hello"), 5), ==, RC_STR("hello"));
    RC_CHECK(rc_str_last(RC_STR("hello"), 99), ==, RC_STR("hello"));
}


RC_TEST(str, drop_first)
{
    RC_CHECK(rc_str_drop_first(RC_STR("hello"), 2), ==, RC_STR("llo"));
    RC_CHECK(rc_str_drop_first(RC_STR("hello"), 0), ==, RC_STR("hello"));
    RC_CHECK(rc_str_drop_first(RC_STR("hello"), 5), ==, RC_STR(""));
    RC_CHECK(rc_str_drop_first(RC_STR("hello"), 99), ==, RC_STR(""));
}


RC_TEST(str, drop_last)
{
    RC_CHECK(rc_str_drop_last(RC_STR("hello"), 2), ==, RC_STR("hel"));
    RC_CHECK(rc_str_drop_last(RC_STR("hello"), 0), ==, RC_STR("hello"));
    RC_CHECK(rc_str_drop_last(RC_STR("hello"), 5), ==, RC_STR(""));
    RC_CHECK(rc_str_drop_last(RC_STR("hello"), 99), ==, RC_STR(""));
}


RC_TEST(str, first_and_drop_first_partition)
{
    // Splitting at n with first and drop_first loses nothing, and the same for last
    rc_str s = RC_STR("abcdef");

    for (uint32_t n = 0; n <= 7; n++) {
        RC_CHECK(rc_str_first(s, n).len + rc_str_drop_first(s, n).len, ==, s.len);
        RC_CHECK(rc_str_last(s, n).len + rc_str_drop_last(s, n).len, ==, s.len);
    }
}


RC_TEST(str, substr)
{
    RC_CHECK(rc_str_substr(RC_STR("hello world"), 6, 11), ==, RC_STR("world"));
    RC_CHECK(rc_str_substr(RC_STR("hello world"), 2, 4), ==, RC_STR("ll"));
    RC_CHECK(rc_str_substr(RC_STR("hello"), 0, 5), ==, RC_STR("hello"));
    RC_CHECK(rc_str_substr(RC_STR("hello"), 2, 2), ==, RC_STR(""));
}


RC_TEST(str, substr_clamps)
{
    // An end past the string stops at it, and an end before start gives nothing at start
    rc_str s = RC_STR("hello");

    RC_CHECK(rc_str_substr(s, 3, 99), ==, RC_STR("lo"));
    RC_CHECK(rc_str_substr(s, 5, 6), ==, RC_STR(""));
    RC_CHECK(rc_str_substr(s, 10, 3), ==, RC_STR(""));
    RC_CHECK(rc_str_substr(s, 4, 1), ==, RC_STR(""));
    RC_CHECK_TRUE(rc_str_substr(s, 4, 1).data == s.data + 4);
    RC_CHECK(rc_str_substr(s, 0, RC_INDEX_NONE), ==, s);
}


RC_TEST(str, slices_share_data)
{
    rc_str s = RC_STR("hello");

    RC_CHECK_TRUE(rc_str_first(s, 2).data == s.data);
    RC_CHECK_TRUE(rc_str_last(s, 2).data == s.data + 3);
    RC_CHECK_TRUE(rc_str_drop_first(s, 4).data == s.data + 4);
    RC_CHECK_TRUE(rc_str_drop_last(s, 4).data == s.data);
    RC_CHECK_TRUE(rc_str_substr(s, 1, 3).data == s.data + 1);
}


// ---- searching ----

RC_TEST(str, starts_with)
{
    RC_CHECK_TRUE(rc_str_starts_with(RC_STR("hello"), RC_STR("he")));
    RC_CHECK_TRUE(rc_str_starts_with(RC_STR("hello"), RC_STR("hello")));
    RC_CHECK_FALSE(rc_str_starts_with(RC_STR("hello"), RC_STR("lo")));
    RC_CHECK_TRUE(rc_str_starts_with(RC_STR("hello"), RC_STR("")));
    RC_CHECK_TRUE(rc_str_starts_with(RC_STR(""), RC_STR("")));
    RC_CHECK_FALSE(rc_str_starts_with(RC_STR("he"), RC_STR("hello")));
}


RC_TEST(str, ends_with)
{
    RC_CHECK_TRUE(rc_str_ends_with(RC_STR("hello"), RC_STR("lo")));
    RC_CHECK_TRUE(rc_str_ends_with(RC_STR("hello"), RC_STR("hello")));
    RC_CHECK_FALSE(rc_str_ends_with(RC_STR("hello"), RC_STR("he")));
    RC_CHECK_TRUE(rc_str_ends_with(RC_STR("hello"), RC_STR("")));
    RC_CHECK_TRUE(rc_str_ends_with(RC_STR(""), RC_STR("")));
    RC_CHECK_FALSE(rc_str_ends_with(RC_STR("lo"), RC_STR("hello")));
}


RC_TEST(str, find_first)
{
    RC_CHECK(rc_str_find_first(RC_STR("hello"), RC_STR("he")), ==, 0u);
    RC_CHECK(rc_str_find_first(RC_STR("hello"), RC_STR("llo")), ==, 2u);
    RC_CHECK(rc_str_find_first(RC_STR("hello"), RC_STR("o")), ==, 4u);
    RC_CHECK(rc_str_find_first(RC_STR("hello"), RC_STR("hello")), ==, 0u);
    RC_CHECK(rc_str_find_first(RC_STR("hello"), RC_STR("xyz")), ==, RC_INDEX_NONE);
    RC_CHECK(rc_str_find_first(RC_STR("hello"), RC_STR("lox")), ==, RC_INDEX_NONE);
    RC_CHECK(rc_str_find_first(RC_STR("hi"), RC_STR("hello")), ==, RC_INDEX_NONE);
}


RC_TEST(str, find_first_empty)
{
    // An empty needle is found at the very start, even of an empty haystack
    RC_CHECK(rc_str_find_first(RC_STR("hello"), RC_STR("")), ==, 0u);
    RC_CHECK(rc_str_find_first(RC_STR(""), RC_STR("")), ==, 0u);
    RC_CHECK(rc_str_find_first(RC_STR(""), RC_STR("a")), ==, RC_INDEX_NONE);
}


RC_TEST(str, find_first_multi)
{
    RC_CHECK(rc_str_find_first(RC_STR("abcabc"), RC_STR("bc")), ==, 1u);
}


RC_TEST(str, find_last)
{
    RC_CHECK(rc_str_find_last(RC_STR("abcabc"), RC_STR("bc")), ==, 4u);
    RC_CHECK(rc_str_find_last(RC_STR("abcabc"), RC_STR("ab")), ==, 3u);
    RC_CHECK(rc_str_find_last(RC_STR("abcxyz"), RC_STR("ab")), ==, 0u);
    RC_CHECK(rc_str_find_last(RC_STR("hello"), RC_STR("hello")), ==, 0u);
    RC_CHECK(rc_str_find_last(RC_STR("hello"), RC_STR("xyz")), ==, RC_INDEX_NONE);
    RC_CHECK(rc_str_find_last(RC_STR("hi"), RC_STR("hello")), ==, RC_INDEX_NONE);
}


RC_TEST(str, find_last_empty)
{
    // An empty needle is found just past the last character
    RC_CHECK(rc_str_find_last(RC_STR("hello"), RC_STR("")), ==, 5u);
    RC_CHECK(rc_str_find_last(RC_STR(""), RC_STR("")), ==, 0u);
    RC_CHECK(rc_str_find_last(RC_STR(""), RC_STR("a")), ==, RC_INDEX_NONE);
}


RC_TEST(str, contains)
{
    RC_CHECK_TRUE(rc_str_contains(RC_STR("hello"), RC_STR("ell")));
    RC_CHECK_FALSE(rc_str_contains(RC_STR("hello"), RC_STR("xyz")));
    RC_CHECK_TRUE(rc_str_contains(RC_STR("hello"), RC_STR("")));
}


// ---- trimming ----

RC_TEST(str, remove_prefix)
{
    RC_CHECK(rc_str_remove_prefix(RC_STR("foobar"), RC_STR("foo")), ==, RC_STR("bar"));
    RC_CHECK(rc_str_remove_prefix(RC_STR("foobar"), RC_STR("foobar")), ==, RC_STR(""));
    RC_CHECK(rc_str_remove_prefix(RC_STR("foobar"), RC_STR("baz")), ==, RC_STR("foobar"));
    RC_CHECK(rc_str_remove_prefix(RC_STR("foobar"), RC_STR("bar")), ==, RC_STR("foobar"));
    RC_CHECK(rc_str_remove_prefix(RC_STR("foobar"), RC_STR("")), ==, RC_STR("foobar"));
}


RC_TEST(str, remove_suffix)
{
    RC_CHECK(rc_str_remove_suffix(RC_STR("foobar"), RC_STR("bar")), ==, RC_STR("foo"));
    RC_CHECK(rc_str_remove_suffix(RC_STR("foobar"), RC_STR("foobar")), ==, RC_STR(""));
    RC_CHECK(rc_str_remove_suffix(RC_STR("foobar"), RC_STR("baz")), ==, RC_STR("foobar"));
    RC_CHECK(rc_str_remove_suffix(RC_STR("foobar"), RC_STR("foo")), ==, RC_STR("foobar"));
    RC_CHECK(rc_str_remove_suffix(RC_STR("foobar"), RC_STR("")), ==, RC_STR("foobar"));
}


// ---- splitting ----

RC_TEST(str, first_split)
{
    rc_str_pair p = rc_str_first_split(RC_STR("a/b/c"), RC_STR("/"));

    RC_CHECK(p.first, ==, RC_STR("a"));
    RC_CHECK(p.second, ==, RC_STR("b/c"));
}


RC_TEST(str, first_split_absent)
{
    // With no delimiter, first is the whole string and second is invalid
    rc_str_pair p = rc_str_first_split(RC_STR("abc"), RC_STR("/"));

    RC_CHECK(p.first, ==, RC_STR("abc"));
    RC_CHECK_FALSE(rc_str_is_valid(p.second));
}


RC_TEST(str, first_split_edges)
{
    // A delimiter at either end leaves a valid empty side
    rc_str_pair start = rc_str_first_split(RC_STR("/ab"), RC_STR("/"));
    rc_str_pair end = rc_str_first_split(RC_STR("ab/"), RC_STR("/"));

    RC_CHECK(start.first, ==, RC_STR(""));
    RC_CHECK_TRUE(rc_str_is_valid(start.first));
    RC_CHECK(start.second, ==, RC_STR("ab"));
    RC_CHECK(end.first, ==, RC_STR("ab"));
    RC_CHECK(end.second, ==, RC_STR(""));
    RC_CHECK_TRUE(rc_str_is_valid(end.second));
}


RC_TEST(str, first_split_long_delimiter)
{
    rc_str_pair p = rc_str_first_split(RC_STR("key::value::more"), RC_STR("::"));

    RC_CHECK(p.first, ==, RC_STR("key"));
    RC_CHECK(p.second, ==, RC_STR("value::more"));
}


RC_TEST(str, last_split)
{
    rc_str_pair p = rc_str_last_split(RC_STR("a/b/c"), RC_STR("/"));

    RC_CHECK(p.first, ==, RC_STR("a/b"));
    RC_CHECK(p.second, ==, RC_STR("c"));
}


RC_TEST(str, last_split_absent)
{
    rc_str_pair p = rc_str_last_split(RC_STR("abc"), RC_STR("/"));

    RC_CHECK(p.first, ==, RC_STR("abc"));
    RC_CHECK_FALSE(rc_str_is_valid(p.second));
}


RC_TEST(str, last_split_long_delimiter)
{
    rc_str_pair p = rc_str_last_split(RC_STR("key::value::more"), RC_STR("::"));

    RC_CHECK(p.first, ==, RC_STR("key::value"));
    RC_CHECK(p.second, ==, RC_STR("more"));
}


// ---- conversion ----

RC_TEST(str, as_cstr_fast)
{
    // A view already followed by a null comes back without a copy
    rc_str s = RC_STR("hello");
    char buf[16];
    const char *c = rc_str_as_cstr(s, buf, sizeof buf);

    RC_CHECK_TRUE(c == s.data);
    RC_CHECK(rc_str_from_cstr(c), ==, RC_STR("hello"));
}


RC_TEST(str, as_cstr_fast_without_buffer)
{
    rc_str s = rc_str_drop_first(RC_STR("hello"), 2);

    RC_CHECK_TRUE(rc_str_as_cstr(s, NULL, 0) == s.data);
}


RC_TEST(str, as_cstr_copy)
{
    // A view not followed by a null is copied into the buffer
    rc_str s = rc_str_first(RC_STR("hello world"), 5);
    char buf[16];
    const char *c = rc_str_as_cstr(s, buf, sizeof buf);

    RC_CHECK_TRUE(c == buf);
    RC_CHECK(rc_str_from_cstr(c), ==, RC_STR("hello"));
}


RC_TEST(str, as_cstr_copy_exact_fit)
{
    rc_str s = rc_str_first(RC_STR("hello world"), 5);
    char buf[6];
    const char *c = rc_str_as_cstr(s, buf, sizeof buf);

    RC_CHECK(rc_str_from_cstr(c), ==, RC_STR("hello"));
}


RC_TEST(str, as_cstr_truncate)
{
    // The copy is cut to fit, and always terminated
    rc_str s = rc_str_first(RC_STR("hello world"), 5);
    char buf[3];
    const char *c = rc_str_as_cstr(s, buf, sizeof buf);

    RC_CHECK(rc_str_from_cstr(c), ==, RC_STR("he"));
}


RC_TEST(str, as_cstr_one_byte_buffer)
{
    rc_str s = rc_str_first(RC_STR("hello world"), 5);
    char buf[1];
    const char *c = rc_str_as_cstr(s, buf, sizeof buf);

    RC_CHECK(rc_str_from_cstr(c), ==, RC_STR(""));
}


RC_TEST(str, as_cstr_no_buffer)
{
    // No fast path and nowhere to copy to
    rc_str s = rc_str_first(RC_STR("hello world"), 5);
    char buf[4];

    RC_CHECK_TRUE(rc_str_as_cstr(s, NULL, 0) == NULL);
    RC_CHECK_TRUE(rc_str_as_cstr(s, buf, 0) == NULL);
}
