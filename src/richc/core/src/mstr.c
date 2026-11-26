// mstr.c
//
// rc_mstr growth, appends and replace.

#include "richc/mstr.h"

#include "richc/arena.h"
#include "richc/mem.h"

// Make the buffer hold need bytes, growing it to the larger of double the capacity, the need, or 8,
// as rc_array does.
static void rc_mstr_reserve_geometric(rc_mstr *s, uint32_t need, rc_arena *arena)
{
    uint32_t doubled = s->cap > UINT32_MAX / 2 ? UINT32_MAX : s->cap * 2;
    uint32_t new_cap = need > doubled ? need : doubled;

    if (need <= s->cap) {
        return;
    }

    rc_mstr_reserve(s, new_cap < 8 ? 8 : new_cap, arena);
}


// Make room for n more characters and the terminator. Panics (in release too) if the length would
// wrap: we would skip the growth and write past the end.
static void rc_mstr_grow(rc_mstr *s, uint32_t n, rc_arena *arena)
{
    RC_PANIC(n < UINT32_MAX - s->len);
    rc_mstr_reserve_geometric(s, s->len + n + 1, arena);
}


// A new string holding a copy of s, which is valid and leaves room for its terminator.
static rc_mstr rc_mstr_copy_of(rc_str s, uint32_t minimum_capacity, rc_arena *arena)
{
    uint32_t need = s.len + 1;
    uint32_t cap = need > minimum_capacity ? need : minimum_capacity;
    char *data = rc_arena_alloc(arena, cap);

    rc_memcpy(data, s.data, s.len);
    data[s.len] = '\0';
    return (rc_mstr) {.data = data, .len = s.len, .cap = cap};
}


// Move the text into a buffer of capacity bytes. realloc of NULL allocates, so this also gives an
// invalid string its first buffer, which then needs its terminator.
static void rc_mstr_realloc(rc_mstr *s, uint32_t capacity, rc_arena *arena)
{
    bool was_invalid = s->data == NULL;

    s->data = rc_arena_realloc(arena, s->data, s->cap, capacity);
    s->cap = capacity;
    if (was_invalid) {
        s->data[0] = '\0';
    }
}


rc_mstr rc_mstr_make(uint32_t capacity, rc_arena *arena)
{
    rc_mstr s = {0};

    rc_mstr_reserve(&s, capacity, arena);
    return s;
}


rc_mstr rc_mstr_from_cstr(const char *s, uint32_t minimum_capacity, rc_arena *arena)
{
    return rc_mstr_from_str(rc_str_from_cstr(s), minimum_capacity, arena);
}


rc_mstr rc_mstr_from_str(rc_str s, uint32_t minimum_capacity, rc_arena *arena)
{
    if (!rc_str_is_valid(s)) {
        return (rc_mstr) {0};
    }

    RC_PANIC(s.len < UINT32_MAX);
    return rc_mstr_copy_of(s, minimum_capacity, arena);
}


void rc_mstr_reset(rc_mstr *s)
{
    RC_ASSERT(s);
    if (s->data) {
        s->len = 0;
        s->data[0] = '\0';
    }
}


void rc_mstr_release(rc_mstr *s, rc_arena *arena)
{
    RC_ASSERT(s);
    if (s->data) {
        rc_arena_free(arena, s->data, s->cap);
    }

    *s = (rc_mstr) {0};
}


void rc_mstr_reserve(rc_mstr *s, uint32_t capacity, rc_arena *arena)
{
    RC_ASSERT(s);
    if (capacity <= s->cap) {
        return;
    }

    RC_ASSERT(arena);
    rc_mstr_realloc(s, capacity, arena);
}


void rc_mstr_append(rc_mstr *s, rc_str str, rc_arena *arena)
{
    RC_ASSERT(s);
    RC_ASSERT(rc_str_is_valid(str));
    if (str.len == 0) {
        return;
    }

    rc_mstr_grow(s, str.len, arena);
    rc_memcpy(s->data + s->len, str.data, str.len);
    s->len += str.len;
    s->data[s->len] = '\0';
}


void rc_mstr_append_char(rc_mstr *s, char c, rc_arena *arena)
{
    RC_ASSERT(s);
    rc_mstr_grow(s, 1, arena);
    s->data[s->len] = c;
    s->len++;
    s->data[s->len] = '\0';
}


// True if codepoint is one UTF-8 can hold: up to U+10FFFF, and no surrogate.
static bool rc_mstr_is_scalar(uint32_t codepoint)
{
    if (codepoint > 0x10FFFF) {
        return false;
    }
    return codepoint < 0xD800 || codepoint > 0xDFFF;
}


// How many bytes UTF-8 takes for c, a scalar.
static uint32_t rc_mstr_utf8_len(uint32_t c)
{
    if (c < 0x80) {
        return 1;
    }

    if (c < 0x800) {
        return 2;
    }
    return c < 0x10000 ? 3 : 4;
}


void rc_mstr_append_utf8(rc_mstr *s, uint32_t codepoint, rc_arena *arena)
{
    uint32_t c = rc_mstr_is_scalar(codepoint) ? codepoint : RC_CODEPOINT_REPLACEMENT;
    uint32_t len = rc_mstr_utf8_len(c);
    char bytes[4] = {0};

    if (len == 1) {
        rc_mstr_append_char(s, (char)c, arena);
        return;
    }

    // The lead byte has len high bits set, then the top bits of c; each continuation byte has 10
    // and then six bits more
    for (uint32_t i = len - 1; i > 0; i--) {
        bytes[i] = (char)(0x80u | (c & 0x3Fu));
        c >>= 6;
    }

    bytes[0] = (char)((0xFF00u >> len & 0xFFu) | c);
    rc_mstr_append(s, rc_str_from_ptr(bytes, len), arena);
}


void rc_mstr_append_n(rc_mstr *s, char c, uint32_t n, rc_arena *arena)
{
    RC_ASSERT(s);
    if (n == 0) {
        return;
    }

    rc_mstr_grow(s, n, arena);
    rc_memset(s->data + s->len, (uint8_t)c, n);
    s->len += n;
    s->data[s->len] = '\0';
}


void rc_mstr_append_u64(rc_mstr *s, uint64_t value, rc_arena *arena)
{
    // 2^64 - 1 is twenty digits, the widest we can be asked for
    char buf[20] = {0};
    uint32_t i = RC_COUNT(buf);

    // Built back to front, no printf in sight
    do {
        buf[--i] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    rc_mstr_append(s, rc_str_from_ptr(buf + i, RC_COUNT(buf) - i), arena);
}


void rc_mstr_append_i64(rc_mstr *s, int64_t value, rc_arena *arena)
{
    if (value < 0) {
        rc_mstr_append_char(s, '-', arena);
    }

    // The magnitude in unsigned, so INT64_MIN does not overflow its own negation
    rc_mstr_append_u64(s, value < 0 ? 0 - (uint64_t)value : (uint64_t)value, arena);
}


void rc_mstr_append_u32(rc_mstr *s, uint32_t value, rc_arena *arena)
{
    rc_mstr_append_u64(s, value, arena);
}


void rc_mstr_append_i32(rc_mstr *s, int32_t value, rc_arena *arena)
{
    rc_mstr_append_i64(s, value, arena);
}


// value as exactly digits hex digits, upper case and zero-padded.
static void rc_mstr_append_hex(rc_mstr *s, uint64_t value, uint32_t digits, rc_arena *arena)
{
    static const char hex_digit[] = "0123456789ABCDEF";
    char buf[16] = {0};

    for (uint32_t i = 0; i < digits; i++) {
        buf[digits - 1 - i] = hex_digit[(value >> (4 * i)) & 0xF];
    }
    rc_mstr_append(s, rc_str_from_ptr(buf, digits), arena);
}


void rc_mstr_append_hex8(rc_mstr *s, uint8_t value, rc_arena *arena)
{
    rc_mstr_append_hex(s, value, 2, arena);
}


void rc_mstr_append_hex16(rc_mstr *s, uint16_t value, rc_arena *arena)
{
    rc_mstr_append_hex(s, value, 4, arena);
}


void rc_mstr_append_hex32(rc_mstr *s, uint32_t value, rc_arena *arena)
{
    rc_mstr_append_hex(s, value, 8, arena);
}


void rc_mstr_append_hex64(rc_mstr *s, uint64_t value, rc_arena *arena)
{
    rc_mstr_append_hex(s, value, 16, arena);
}


// Copy the text from src to end over to dst, replacing count matches of find as we go, left to
// right. dst may overlap the text as long as it never gets ahead of what is still to be read.
static char *rc_mstr_replace_into(char *dst, const char *src, const char *end, rc_str find, rc_str replacement, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        uint32_t pos = rc_str_find_first(rc_str_from_ptr(src, (uint32_t)(end - src)), find);

        rc_memmove(dst, src, pos);
        dst += pos;
        rc_memcpy(dst, replacement.data, replacement.len);
        dst += replacement.len;
        src += pos + find.len;
    }

    // The text after the last match
    rc_memmove(dst, src, (uint32_t)(end - src));
    return dst + (end - src);
}


// The number of non-overlapping matches of find, which is not empty, in text.
static uint32_t rc_mstr_count_matches(rc_str text, rc_str find)
{
    uint32_t count = 0;
    rc_str rest = text;

    for (uint32_t pos = rc_str_find_first(rest, find); pos != RC_INDEX_NONE; pos = rc_str_find_first(rest, find)) {
        count++;
        rest = rc_str_drop_first(rest, pos + find.len);
    }

    return count;
}


// Write the text, which starts shift bytes into the buffer, back from the start with its count
// matches of find replaced, ending at new_len.
static void rc_mstr_rewrite(rc_mstr *s, uint32_t shift, rc_str find, rc_str replacement, uint32_t count, uint32_t new_len)
{
    char *text = s->data + shift;
    char *end = rc_mstr_replace_into(s->data, text, text + s->len, find, replacement, count);

    RC_ASSERT(end == s->data + new_len);
    *end = '\0';
    s->len = new_len;
}


// Replace the count matches of find in s with replacement.
static void rc_mstr_replace_counted(rc_mstr *s, rc_str find, rc_str replacement, uint32_t count, rc_arena *arena)
{
    // The new length can outgrow 32 bits, so we work it out in 64
    uint64_t new_len = (uint64_t)s->len - (uint64_t)count * find.len + (uint64_t)count * replacement.len;
    uint32_t shift = new_len > s->len ? (uint32_t)(new_len - s->len) : 0;

    if (count == 0) {
        return;
    }

    RC_PANIC(new_len < UINT32_MAX);

    // When the string grows, its text first moves up so it ends where the result will. Writing from
    // the start then closes on the reading by the growth of one match at a time, and catches up
    // only as the last byte is read.
    if (shift > 0) {
        rc_mstr_grow(s, shift, arena);
        rc_memmove(s->data + shift, s->data, s->len);
    }

    rc_mstr_rewrite(s, shift, find, replacement, count, (uint32_t)new_len);
}


void rc_mstr_replace(rc_mstr *s, rc_str find, rc_str replacement, rc_arena *arena)
{
    RC_ASSERT(s);
    RC_ASSERT(rc_str_is_valid(find));
    RC_ASSERT(rc_str_is_valid(replacement));
    if (find.len == 0 || s->len == 0) {
        return;
    }

    rc_mstr_replace_counted(s, find, replacement, rc_mstr_count_matches(s->view, find), arena);
}
