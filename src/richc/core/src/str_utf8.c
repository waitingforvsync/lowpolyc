// str_utf8.c
//
// Walking UTF-8 a codepoint at a time. Every step decodes forward from a known start: stepping back
// first finds where the character before the cursor starts, then decodes it as stepping forward
// would, in the text up to the cursor.

#include "richc/str.h"

// What a byte leading a character says about the rest of it: how many bytes it has in all (0 if
// the byte can lead none), and the range the second must be in. The ranges rule out overlong
// forms, surrogates and anything past U+10FFFF, as the Unicode Standard's table 3-7 does.
typedef struct rc_utf8_lead {
    uint32_t len;
    uint8_t low;
    uint8_t high;
} rc_utf8_lead;


// The byte at index, which is in s.
static uint8_t rc_utf8_byte(rc_str s, uint32_t index)
{
    return (uint8_t)s.data[index];
}


// True if the byte at index is a continuation byte, 10xxxxxx, which no character starts with.
static bool rc_utf8_is_continuation(rc_str s, uint32_t index)
{
    return (rc_utf8_byte(s, index) & 0xC0u) == 0x80u;
}


// What byte, which is not ASCII, says as the lead of a character.
static rc_utf8_lead rc_utf8_lead_of(uint8_t byte)
{
    if (byte < 0xC2) {
        // A continuation byte, or the lead of an overlong two-byte form
        return (rc_utf8_lead) {.len = 0};
    }

    if (byte < 0xE0) {
        return (rc_utf8_lead) {.len = 2, .low = 0x80, .high = 0xBF};
    }

    if (byte < 0xF0) {
        // E0 would be overlong below A0, and ED a surrogate from A0
        uint8_t low = byte == 0xE0 ? 0xA0 : 0x80;
        uint8_t high = byte == 0xED ? 0x9F : 0xBF;

        return (rc_utf8_lead) {.len = 3, .low = low, .high = high};
    }

    if (byte < 0xF5) {
        // F0 would be overlong below 90, and F4 past U+10FFFF from 90
        uint8_t low = byte == 0xF0 ? 0x90 : 0x80;
        uint8_t high = byte == 0xF4 ? 0x8F : 0xBF;

        return (rc_utf8_lead) {.len = 4, .low = low, .high = high};
    }

    return (rc_utf8_lead) {.len = 0};
}


// True if the byte at index can follow those before it, as the nth byte (from 1) of a character
// led by lead.
static bool rc_utf8_can_follow(rc_str s, uint32_t index, uint32_t n, rc_utf8_lead lead)
{
    uint8_t byte = rc_utf8_byte(s, index);
    uint8_t low = n == 1 ? lead.low : 0x80;
    uint8_t high = n == 1 ? lead.high : 0xBF;

    return byte >= low && byte <= high;
}


// The character at cursor, which is in s and not ASCII. Where it is malformed, the step covers the
// bytes before the first that cannot follow, or the end of the text: the maximal subpart.
static rc_str_utf8_char rc_utf8_decode(rc_str s, uint32_t cursor)
{
    uint8_t first = rc_utf8_byte(s, cursor);
    rc_utf8_lead lead = rc_utf8_lead_of(first);
    uint32_t codepoint = first & (0x7Fu >> lead.len);

    if (lead.len == 0) {
        return (rc_str_utf8_char) {.codepoint = RC_CODEPOINT_NONE, .cursor = cursor + 1};
    }

    for (uint32_t n = 1; n < lead.len; n++) {
        uint32_t index = cursor + n;

        if (index == s.len || !rc_utf8_can_follow(s, index, n, lead)) {
            return (rc_str_utf8_char) {.codepoint = RC_CODEPOINT_NONE, .cursor = index};
        }

        codepoint = codepoint << 6 | (rc_utf8_byte(s, index) & 0x3Fu);
    }

    return (rc_str_utf8_char) {.codepoint = codepoint, .cursor = cursor + lead.len};
}


// The nearest byte at or before index, which is in s and no more than three back, that is not a
// continuation byte; the furthest back it looked if they all are.
static uint32_t rc_utf8_lead_before(rc_str s, uint32_t index)
{
    uint32_t stop = index > 3 ? index - 3 : 0;
    uint32_t lead = index;

    while (lead > stop && rc_utf8_is_continuation(s, lead)) {
        lead--;
    }
    return lead;
}


rc_str_utf8_char rc_str_utf8_next(rc_str s, uint32_t cursor)
{
    if (cursor >= s.len) {
        return (rc_str_utf8_char) {.codepoint = RC_CODEPOINT_NONE, .cursor = s.len};
    }

    if (rc_utf8_byte(s, cursor) < 0x80) {
        return (rc_str_utf8_char) {.codepoint = rc_utf8_byte(s, cursor), .cursor = cursor + 1};
    }

    return rc_utf8_decode(s, cursor);
}


rc_str_utf8_char rc_str_utf8_prev(rc_str s, uint32_t cursor)
{
    // Only the text before the cursor, so a character the cursor cuts is malformed
    rc_str before = rc_str_first(s, cursor);
    uint32_t start = before.len > 0 ? rc_str_utf8_floor(before, before.len - 1) : 0;

    if (before.len == 0) {
        return (rc_str_utf8_char) {.codepoint = RC_CODEPOINT_NONE, .cursor = 0};
    }

    return (rc_str_utf8_char) {.codepoint = rc_str_utf8_next(before, start).codepoint, .cursor = start};
}


uint32_t rc_str_utf8_floor(rc_str s, uint32_t index)
{
    uint32_t lead = index < s.len ? rc_utf8_lead_before(s, index) : s.len;

    if (index >= s.len) {
        return s.len;
    }

    // A character is at most four bytes, so one that covers index starts no more than three back.
    // If the step from lead does not reach past index, index starts a step of its own.
    return rc_str_utf8_next(s, lead).cursor > index ? lead : index;
}


uint32_t rc_str_utf8_ceil(rc_str s, uint32_t index)
{
    uint32_t start = rc_str_utf8_floor(s, index);

    if (start == index || start == s.len) {
        return start;
    }
    return rc_str_utf8_next(s, start).cursor;
}


bool rc_str_utf8_is_boundary(rc_str s, uint32_t index)
{
    if (index > s.len) {
        return false;
    }
    return rc_str_utf8_floor(s, index) == index;
}


bool rc_str_utf8_is_valid(rc_str s)
{
    rc_str_utf8_char c = {.codepoint = 0, .cursor = 0};

    while (c.cursor < s.len) {
        c = rc_str_utf8_next(s, c.cursor);
        if (c.codepoint == RC_CODEPOINT_NONE) {
            return false;
        }
    }
    return true;
}


uint32_t rc_str_utf8_count(rc_str s)
{
    uint32_t count = 0;

    for (uint32_t cursor = 0; cursor < s.len; cursor = rc_str_utf8_next(s, cursor).cursor) {
        count++;
    }
    return count;
}
