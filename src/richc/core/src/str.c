// str.c
//
// The out-of-line half of rc_str: comparison, searching and splitting.

#include "richc/str.h"

#include "richc/mem.h"

// ASCII lower case, whatever the locale.
static char rc_str_ascii_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}


rc_str rc_str_from_cstr(const char *s)
{
    if (!s) {
        return rc_str_make(NULL, 0);
    }

    // A string too long for 32 bits is clamped rather than wrapped
    uint32_t len = 0;
    while (s[len] != '\0' && len < UINT32_MAX) {
        len++;
    }
    return rc_str_make(s, len);
}


bool rc_str_is_equal(rc_str a, rc_str b)
{
    RC_ASSERT(rc_str_is_valid(a));
    RC_ASSERT(rc_str_is_valid(b));
    return a.len == b.len && rc_memcmp(a.data, b.data, a.len) == 0;
}


bool rc_str_is_equal_insensitive(rc_str a, rc_str b)
{
    return rc_str_compare_insensitive(a, b) == 0;
}


int32_t rc_str_compare(rc_str a, rc_str b)
{
    RC_ASSERT(rc_str_is_valid(a));
    RC_ASSERT(rc_str_is_valid(b));

    uint32_t min_len = a.len < b.len ? a.len : b.len;
    int32_t r = rc_memcmp(a.data, b.data, min_len);
    if (r != 0) {
        return r;
    }
    return (a.len > b.len) - (a.len < b.len);
}


int32_t rc_str_compare_insensitive(rc_str a, rc_str b)
{
    RC_ASSERT(rc_str_is_valid(a));
    RC_ASSERT(rc_str_is_valid(b));

    uint32_t min_len = a.len < b.len ? a.len : b.len;
    for (uint32_t i = 0; i < min_len; i++) {
        // Compared as unsigned bytes, as memcmp would
        uint8_t ca = (uint8_t)rc_str_ascii_lower(a.data[i]);
        uint8_t cb = (uint8_t)rc_str_ascii_lower(b.data[i]);

        if (ca != cb) {
            return (int32_t)ca - (int32_t)cb;
        }
    }
    return (a.len > b.len) - (a.len < b.len);
}


bool rc_str_starts_with(rc_str s, rc_str prefix)
{
    RC_ASSERT(rc_str_is_valid(s));
    RC_ASSERT(rc_str_is_valid(prefix));
    return prefix.len <= s.len && rc_memcmp(s.data, prefix.data, prefix.len) == 0;
}


bool rc_str_ends_with(rc_str s, rc_str suffix)
{
    RC_ASSERT(rc_str_is_valid(s));
    RC_ASSERT(rc_str_is_valid(suffix));
    return suffix.len <= s.len && rc_memcmp(s.data + s.len - suffix.len, suffix.data, suffix.len) == 0;
}


uint32_t rc_str_find_first(rc_str haystack, rc_str needle)
{
    RC_ASSERT(rc_str_is_valid(haystack));
    RC_ASSERT(rc_str_is_valid(needle));
    if (needle.len > haystack.len) {
        return RC_INDEX_NONE;
    }
    for (uint32_t i = 0; i <= haystack.len - needle.len; i++) {
        if (rc_memcmp(haystack.data + i, needle.data, needle.len) == 0) {
            return i;
        }
    }
    return RC_INDEX_NONE;
}


uint32_t rc_str_find_last(rc_str haystack, rc_str needle)
{
    RC_ASSERT(rc_str_is_valid(haystack));
    RC_ASSERT(rc_str_is_valid(needle));
    if (needle.len > haystack.len) {
        return RC_INDEX_NONE;
    }

    // Counting down from one past the last start, so the loop ends at zero
    for (uint32_t i = haystack.len - needle.len + 1; i > 0; i--) {
        if (rc_memcmp(haystack.data + i - 1, needle.data, needle.len) == 0) {
            return i - 1;
        }
    }
    return RC_INDEX_NONE;
}


bool rc_str_contains(rc_str haystack, rc_str needle)
{
    return rc_str_find_first(haystack, needle) != RC_INDEX_NONE;
}


rc_str rc_str_remove_prefix(rc_str s, rc_str prefix)
{
    return rc_str_starts_with(s, prefix) ? rc_str_skip(s, prefix.len) : s;
}


rc_str rc_str_remove_suffix(rc_str s, rc_str suffix)
{
    return rc_str_ends_with(s, suffix) ? rc_str_left(s, s.len - suffix.len) : s;
}


// Split s around the split_by found at pos (RC_INDEX_NONE: not found).
static rc_str_pair rc_str_split_at(rc_str s, uint32_t pos, rc_str split_by)
{
    if (pos == RC_INDEX_NONE) {
        return (rc_str_pair) {.first = s, .second = {0}};
    }
    return (rc_str_pair) {
        .first = rc_str_left(s, pos),
        .second = rc_str_skip(s, pos + split_by.len)
    };
}


rc_str_pair rc_str_first_split(rc_str s, rc_str split_by)
{
    return rc_str_split_at(s, rc_str_find_first(s, split_by), split_by);
}


rc_str_pair rc_str_last_split(rc_str s, rc_str split_by)
{
    return rc_str_split_at(s, rc_str_find_last(s, split_by), split_by);
}


const char *rc_str_as_cstr(rc_str s, char *buf, uint32_t buf_size)
{
    RC_ASSERT(rc_str_is_valid(s));
    if (s.data && s.data[s.len] == '\0') {
        return s.data;
    }
    if (!buf || buf_size == 0) {
        return NULL;
    }

    uint32_t copy_len = s.len < buf_size - 1 ? s.len : buf_size - 1;
    rc_memcpy(buf, s.data, copy_len);
    buf[copy_len] = '\0';
    return buf;
}
