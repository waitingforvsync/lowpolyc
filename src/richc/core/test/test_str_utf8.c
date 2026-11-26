// test_str_utf8.c
//
// Tests for walking UTF-8 through an rc_str. Byte strings are written as escapes, split wherever a
// letter follows one, as "\xA2" "b", so the letter is not read as another hex digit.

#include "richc/str.h"

#include "richc/arena.h"
#include "richc/mstr.h"
#include "richc/random.h"
#include "richc/test.h"

enum {
    MAX_STEPS = 32
};

// A codepoint, and its UTF-8.
typedef struct encoded {
    rc_str bytes;
    uint32_t codepoint;
} encoded;

// Bytes that start malformed, and where the first step from them lands.
typedef struct malformed {
    rc_str bytes;
    uint32_t cursor;
} malformed;


// The steps forward through s, from the start, into steps (MAX_STEPS of them). Returns how many.
static uint32_t walk_forward(rc_str s, rc_str_utf8_char *steps)
{
    uint32_t num = 0;

    for (uint32_t cursor = 0; cursor < s.len && num < MAX_STEPS; num++) {
        steps[num] = rc_str_utf8_next(s, cursor);
        cursor = steps[num].cursor;
    }
    return num;
}


// The steps back through s, from the end, into steps (MAX_STEPS of them). Returns how many.
static uint32_t walk_back(rc_str s, rc_str_utf8_char *steps)
{
    uint32_t num = 0;

    for (uint32_t cursor = s.len; cursor > 0 && num < MAX_STEPS; num++) {
        steps[num] = rc_str_utf8_prev(s, cursor);
        cursor = steps[num].cursor;
    }
    return num;
}


// Check that walking s forward takes exactly the num steps in expected.
static void check_walk_forward(rc_str s, const rc_str_utf8_char *expected, uint32_t num)
{
    rc_str_utf8_char steps[MAX_STEPS] = {0};

    RC_CHECK(walk_forward(s, steps), ==, num);
    for (uint32_t i = 0; i < num; i++) {
        RC_CHECK(steps[i].codepoint, ==, expected[i].codepoint);
        RC_CHECK(steps[i].cursor, ==, expected[i].cursor);
    }
}


// Check that walking s back takes exactly the num steps in expected.
static void check_walk_back(rc_str s, const rc_str_utf8_char *expected, uint32_t num)
{
    rc_str_utf8_char steps[MAX_STEPS] = {0};

    RC_CHECK(walk_back(s, steps), ==, num);
    for (uint32_t i = 0; i < num; i++) {
        RC_CHECK(steps[i].codepoint, ==, expected[i].codepoint);
        RC_CHECK(steps[i].cursor, ==, expected[i].cursor);
    }
}

// ---- stepping forward ----

RC_TEST(str_utf8, next_ascii)
{
    rc_str s = RC_STR("ab");
    rc_str_utf8_char expected[] = {{'a', 1}, {'b', 2}};

    check_walk_forward(s, expected, RC_COUNT(expected));
}


RC_TEST(str_utf8, next_each_length)
{
    // $, cent, euro, Gothic hwair: one to four bytes
    rc_str s = RC_STR("$\xC2\xA2\xE2\x82\xAC\xF0\x90\x8D\x88");
    rc_str_utf8_char expected[] = {{0x24, 1}, {0xA2, 3}, {0x20AC, 6}, {0x10348, 10}};

    check_walk_forward(s, expected, RC_COUNT(expected));
}


RC_TEST(str_utf8, next_limits)
{
    // The first and last codepoints of each length, and either side of the surrogates
    encoded limits[] = {
        {RC_STR("\x00"), 0x0},
        {RC_STR("\x7F"), 0x7F},
        {RC_STR("\xC2\x80"), 0x80},
        {RC_STR("\xDF\xBF"), 0x7FF},
        {RC_STR("\xE0\xA0\x80"), 0x800},
        {RC_STR("\xED\x9F\xBF"), 0xD7FF},
        {RC_STR("\xEE\x80\x80"), 0xE000},
        {RC_STR("\xEF\xBF\xBF"), 0xFFFF},
        {RC_STR("\xF0\x90\x80\x80"), 0x10000},
        {RC_STR("\xF4\x8F\xBF\xBF"), 0x10FFFF}
    };

    for (uint32_t i = 0; i < RC_COUNT(limits); i++) {
        rc_str_utf8_char c = rc_str_utf8_next(limits[i].bytes, 0);

        RC_CHECK(c.codepoint, ==, limits[i].codepoint);
        RC_CHECK(c.cursor, ==, limits[i].bytes.len);
    }
}


RC_TEST(str_utf8, next_malformed)
{
    malformed cases[] = {
        // Bytes that lead nothing
        {RC_STR("\x80"), 1},
        {RC_STR("\xBF"), 1},
        {RC_STR("\xC0\x80"), 1},
        {RC_STR("\xC1\xBF"), 1},
        {RC_STR("\xF5\x80\x80\x80"), 1},
        {RC_STR("\xFF"), 1},

        // Overlong, surrogate or past U+10FFFF, which the second byte shows
        {RC_STR("\xE0\x80\x80"), 1},
        {RC_STR("\xE0\x9F\xBF"), 1},
        {RC_STR("\xED\xA0\x80"), 1},
        {RC_STR("\xED\xBF\xBF"), 1},
        {RC_STR("\xF0\x80\x80\x80"), 1},
        {RC_STR("\xF0\x8F\xBF\xBF"), 1},
        {RC_STR("\xF4\x90\x80\x80"), 1},

        // Cut short, by the end or by a byte that cannot follow: the step covers the start
        {RC_STR("\xC2"), 1},
        {RC_STR("\xE2\x82"), 2},
        {RC_STR("\xF0\x90\x8D"), 3},
        {RC_STR("\xE2\x82" "A"), 2},
        {RC_STR("\xF0\x90\xC2\xA2"), 2}
    };

    for (uint32_t i = 0; i < RC_COUNT(cases); i++) {
        rc_str_utf8_char c = rc_str_utf8_next(cases[i].bytes, 0);

        RC_CHECK(c.codepoint, ==, RC_CODEPOINT_NONE);
        RC_CHECK(c.cursor, ==, cases[i].cursor);
    }
}


RC_TEST(str_utf8, next_from_the_middle_of_a_character)
{
    // Inside the euro sign, each continuation byte is malformed on its own
    rc_str s = RC_STR("\xE2\x82\xAC" "b");
    rc_str_utf8_char second = rc_str_utf8_next(s, 1);
    rc_str_utf8_char third = rc_str_utf8_next(s, 2);
    rc_str_utf8_char after = rc_str_utf8_next(s, 3);

    RC_CHECK(second.codepoint, ==, RC_CODEPOINT_NONE);
    RC_CHECK(second.cursor, ==, 2u);
    RC_CHECK(third.codepoint, ==, RC_CODEPOINT_NONE);
    RC_CHECK(third.cursor, ==, 3u);
    RC_CHECK(after.codepoint, ==, (uint32_t)'b');
    RC_CHECK(after.cursor, ==, 4u);
}


RC_TEST(str_utf8, next_at_or_past_the_end)
{
    rc_str s = RC_STR("ab");
    rc_str_utf8_char at = rc_str_utf8_next(s, 2);
    rc_str_utf8_char past = rc_str_utf8_next(s, 9);
    rc_str_utf8_char none = rc_str_utf8_next((rc_str) {0}, 0);

    RC_CHECK(at.codepoint, ==, RC_CODEPOINT_NONE);
    RC_CHECK(at.cursor, ==, 2u);
    RC_CHECK(past.codepoint, ==, RC_CODEPOINT_NONE);
    RC_CHECK(past.cursor, ==, 2u);
    RC_CHECK(none.codepoint, ==, RC_CODEPOINT_NONE);
    RC_CHECK(none.cursor, ==, 0u);
}

// ---- stepping back ----

RC_TEST(str_utf8, prev_each_length)
{
    rc_str s = RC_STR("$\xC2\xA2\xE2\x82\xAC\xF0\x90\x8D\x88");
    rc_str_utf8_char expected[] = {{0x10348, 6}, {0x20AC, 3}, {0xA2, 1}, {0x24, 0}};

    check_walk_back(s, expected, RC_COUNT(expected));
}


RC_TEST(str_utf8, prev_from_the_middle_of_a_character)
{
    // Only the bytes before the cursor count, so the euro sign is cut short
    rc_str s = RC_STR("a\xE2\x82\xAC");
    rc_str_utf8_char two = rc_str_utf8_prev(s, 3);
    rc_str_utf8_char one = rc_str_utf8_prev(s, 2);

    RC_CHECK(two.codepoint, ==, RC_CODEPOINT_NONE);
    RC_CHECK(two.cursor, ==, 1u);
    RC_CHECK(one.codepoint, ==, RC_CODEPOINT_NONE);
    RC_CHECK(one.cursor, ==, 1u);
}


RC_TEST(str_utf8, prev_at_the_start_or_past_the_end)
{
    rc_str s = RC_STR("ab");
    rc_str_utf8_char start = rc_str_utf8_prev(s, 0);
    rc_str_utf8_char past = rc_str_utf8_prev(s, 9);
    rc_str_utf8_char none = rc_str_utf8_prev((rc_str) {0}, 3);

    RC_CHECK(start.codepoint, ==, RC_CODEPOINT_NONE);
    RC_CHECK(start.cursor, ==, 0u);
    RC_CHECK(past.codepoint, ==, (uint32_t)'b');
    RC_CHECK(past.cursor, ==, 1u);
    RC_CHECK(none.codepoint, ==, RC_CODEPOINT_NONE);
    RC_CHECK(none.cursor, ==, 0u);
}


RC_TEST(str_utf8, unicode_standard_example)
{
    // Table 3-8 of the Unicode Standard: a, then F1 80 80, E1 80 and C2 as three maximal subparts,
    // b, a lone 80, c, 80 and BF apart, then d. Back, the same steps, the other way.
    rc_str s = RC_STR("\x61\xF1\x80\x80\xE1\x80\xC2\x62\x80\x63\x80\xBF\x64");
    uint32_t none = RC_CODEPOINT_NONE;
    rc_str_utf8_char forward[] = {
        {'a', 1}, {none, 4}, {none, 6}, {none, 7}, {'b', 8}, {none, 9}, {'c', 10}, {none, 11}, {none, 12}, {'d', 13}
    };
    rc_str_utf8_char back[] = {
        {'d', 12}, {none, 11}, {none, 10}, {'c', 9}, {none, 8}, {'b', 7}, {none, 6}, {none, 4}, {none, 1}, {'a', 0}
    };

    check_walk_forward(s, forward, RC_COUNT(forward));
    check_walk_back(s, back, RC_COUNT(back));
    RC_CHECK(rc_str_utf8_count(s), ==, 10u);
    RC_CHECK_FALSE(rc_str_utf8_is_valid(s));
}

// ---- boundaries ----

RC_TEST(str_utf8, floor_and_ceil)
{
    // a, a three-byte euro sign, b
    rc_str s = RC_STR("a\xE2\x82\xAC" "b");
    uint32_t floors[] = {0, 1, 1, 1, 4, 5, 5};
    uint32_t ceils[] = {0, 1, 4, 4, 4, 5, 5};
    bool boundaries[] = {true, true, false, false, true, true, false};

    for (uint32_t i = 0; i < RC_COUNT(floors); i++) {
        RC_CHECK(rc_str_utf8_floor(s, i), ==, floors[i]);
        RC_CHECK(rc_str_utf8_ceil(s, i), ==, ceils[i]);
        RC_CHECK(rc_str_utf8_is_boundary(s, i), ==, boundaries[i]);
    }
}


RC_TEST(str_utf8, floor_in_continuation_bytes)
{
    // A four-byte character covers three continuation bytes, but not a fourth, and lone ones are
    // characters of their own
    rc_str four = RC_STR("\xF0\x9F\x98\x80\x80");
    rc_str lone = RC_STR("\x80\x80\x80\x80\x80");

    RC_CHECK(rc_str_utf8_floor(four, 3), ==, 0u);
    RC_CHECK(rc_str_utf8_floor(four, 4), ==, 4u);
    RC_CHECK(rc_str_utf8_ceil(four, 1), ==, 4u);
    RC_CHECK(rc_str_utf8_floor(lone, 4), ==, 4u);
    RC_CHECK_TRUE(rc_str_utf8_is_boundary(lone, 2));
}


RC_TEST(str_utf8, floor_does_not_reach_a_lead_too_far_back)
{
    // A three-byte lead with three continuation bytes after it: the third stands alone
    rc_str s = RC_STR("\xE2\x82\xAC\xAC");

    RC_CHECK(rc_str_utf8_floor(s, 2), ==, 0u);
    RC_CHECK(rc_str_utf8_floor(s, 3), ==, 3u);
}

// ---- whole strings ----

RC_TEST(str_utf8, is_valid_and_count)
{
    rc_str text = RC_STR("a\xE2\x82\xAC" "b");

    RC_CHECK_TRUE(rc_str_utf8_is_valid(text));
    RC_CHECK(rc_str_utf8_count(text), ==, 3u);
    RC_CHECK_TRUE(rc_str_utf8_is_valid(RC_STR("")));
    RC_CHECK(rc_str_utf8_count(RC_STR("")), ==, 0u);
    RC_CHECK_TRUE(rc_str_utf8_is_valid((rc_str) {0}));
    RC_CHECK(rc_str_utf8_count((rc_str) {0}), ==, 0u);
    RC_CHECK_FALSE(rc_str_utf8_is_valid(RC_STR("a\xE2\x82")));
}

// ---- every codepoint ----

// The scalar after codepoint, which is one; 0x110000 after the last.
static uint32_t next_scalar(uint32_t codepoint)
{
    return codepoint == 0xD7FF ? 0xE000 : codepoint + 1;
}


// How many scalars, all written one after another with rc_mstr_append_utf8, do not step back
// as themselves.
static uint32_t count_scalars_not_round_tripping(rc_arena *arena)
{
    // Room for all of them: 128 of one byte, 1,920 of two, 61,440 of three and 1,048,576 of four
    rc_mstr s = rc_mstr_make(4382592, arena);
    uint32_t failures = 0;
    uint32_t expected = 0;

    for (uint32_t codepoint = 0; codepoint <= 0x10FFFF; codepoint = next_scalar(codepoint)) {
        rc_mstr_append_utf8(&s, codepoint, arena);
    }

    for (uint32_t cursor = 0; cursor < s.len; expected = next_scalar(expected)) {
        rc_str_utf8_char c = rc_str_utf8_next(s.view, cursor);

        failures += c.codepoint != expected ? 1u : 0u;
        cursor = c.cursor;
    }

    return failures + (expected != 0x110000 ? 1u : 0u);
}


RC_TEST(str_utf8, every_scalar_round_trips)
{
    rc_arena arena = rc_arena_make_default();

    RC_CHECK(count_scalars_not_round_tripping(&arena), ==, 0u);
    rc_arena_release(&arena);
}


// codepoint, a scalar past ASCII, in UTF-8 the long way, into bytes (four of them). Returns how
// many it takes.
static uint32_t encode(uint32_t codepoint, uint8_t *bytes)
{
    if (codepoint < 0x800) {
        bytes[0] = (uint8_t)(0xC0 | codepoint >> 6);
        bytes[1] = (uint8_t)(0x80 | (codepoint & 0x3F));
        return 2;
    }

    if (codepoint < 0x10000) {
        bytes[0] = (uint8_t)(0xE0 | codepoint >> 12);
        bytes[1] = (uint8_t)(0x80 | (codepoint >> 6 & 0x3F));
        bytes[2] = (uint8_t)(0x80 | (codepoint & 0x3F));
        return 3;
    }

    bytes[0] = (uint8_t)(0xF0 | codepoint >> 18);
    bytes[1] = (uint8_t)(0x80 | (codepoint >> 12 & 0x3F));
    bytes[2] = (uint8_t)(0x80 | (codepoint >> 6 & 0x3F));
    bytes[3] = (uint8_t)(0x80 | (codepoint & 0x3F));
    return 4;
}


// Note in steps, the first step through each pair of bytes (b0 << 8 | b1) followed by tail
// twice, what codepoint's UTF-8 says: the step is that codepoint, if the bytes are all of it, or
// else, if no codepoint is, it covers at least the bytes the two share, the start of a character.
static void reference_add(rc_str_utf8_char *steps, uint32_t tail, uint32_t codepoint)
{
    uint8_t bytes[4] = {0};
    uint32_t len = encode(codepoint, bytes);
    rc_str_utf8_char *step = &steps[(uint32_t)bytes[0] << 8 | bytes[1]];
    uint32_t same = 2;

    while (same < len && bytes[same] == tail) {
        same++;
    }

    if (same == len) {
        *step = (rc_str_utf8_char) {codepoint, len};
    }
    else if (step->codepoint == RC_CODEPOINT_NONE && step->cursor < same) {
        step->cursor = same;
    }
}


// The first step through each pair of bytes (b0 << 8 | b1) followed by tail twice, worked out
// from every scalar's UTF-8 rather than by decoding.
static rc_str_utf8_char *reference_make(uint32_t tail, rc_arena *arena)
{
    rc_str_utf8_char *steps = rc_arena_alloc(arena, 65536 * sizeof(rc_str_utf8_char));

    // ASCII is itself, and anything else no character starts is malformed on its own
    for (uint32_t key = 0; key < 65536; key++) {
        uint32_t b0 = key >> 8;

        steps[key] = (rc_str_utf8_char) {b0 < 0x80 ? b0 : RC_CODEPOINT_NONE, 1};
    }

    for (uint32_t codepoint = 0x80; codepoint <= 0x10FFFF; codepoint = next_scalar(codepoint)) {
        reference_add(steps, tail, codepoint);
    }
    return steps;
}


// How many pairs of first bytes, followed by tail twice, step other than as the reference does.
static uint32_t count_differences(uint32_t tail, rc_arena *arena)
{
    const rc_str_utf8_char *expected = reference_make(tail, arena);
    uint32_t differences = 0;

    for (uint32_t key = 0; key < 65536; key++) {
        char bytes[4] = {(char)(key >> 8), (char)(key & 0xFF), (char)tail, (char)tail};
        rc_str_utf8_char c = rc_str_utf8_next(rc_str_from_ptr(bytes, 4), 0);

        differences += c.codepoint != expected[key].codepoint || c.cursor != expected[key].cursor;
    }
    return differences;
}


RC_TEST(str_utf8, every_first_two_bytes_match_the_reference)
{
    // The first two bytes decide whether a character is well formed; after them, the tail either
    // can follow or cannot
    rc_arena arena = rc_arena_make_default();

    RC_CHECK(count_differences(0x80, &arena), ==, 0u);
    RC_CHECK(count_differences('A', &arena), ==, 0u);
    rc_arena_release(&arena);
}

// ---- random text ----

enum {
    RANDOM_STRINGS = 2000,
    RANDOM_MAX_LEN = 24
};

// Bytes that make interesting UTF-8: ASCII, leads of each length and their edge cases, and
// continuation bytes at the edges of the second-byte ranges.
static const uint8_t random_bytes[] = {
    'a', 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xC2, 0xDF, 0xE0, 0xE1, 0xED, 0xEF, 0xF0, 0xF1, 0xF4, 0xF5, 0xFF
};


// What the forward walk says of each index of s: whether a step starts or ends there, and the
// steps' codepoints. Returns how many steps there were, or RC_INDEX_NONE if one did not move on.
static uint32_t random_walk(rc_str s, bool *is_boundary, rc_str_utf8_char *steps)
{
    uint32_t num = 0;

    for (uint32_t i = 0; i <= s.len; i++) {
        is_boundary[i] = false;
    }

    for (uint32_t cursor = 0; cursor < s.len; num++) {
        is_boundary[cursor] = true;
        steps[num] = rc_str_utf8_next(s, cursor);
        if (steps[num].cursor <= cursor) {
            return RC_INDEX_NONE;
        }
        cursor = steps[num].cursor;
    }

    is_boundary[s.len] = true;
    return num;
}


// True if s's boundaries, by floor, ceil and is_boundary, are where the forward walk put them.
static bool random_boundaries_match(rc_str s, const bool *is_boundary)
{
    uint32_t floor = 0;

    for (uint32_t i = 0; i <= s.len; i++) {
        uint32_t ceil = i;

        floor = is_boundary[i] ? i : floor;
        while (!is_boundary[ceil]) {
            ceil++;
        }

        if (rc_str_utf8_floor(s, i) != floor || rc_str_utf8_ceil(s, i) != ceil) {
            return false;
        }

        if (rc_str_utf8_is_boundary(s, i) != is_boundary[i]) {
            return false;
        }
    }
    return true;
}


// True if walking s back takes the num steps forward in steps, in reverse.
static bool random_back_matches(rc_str s, const rc_str_utf8_char *steps, uint32_t num)
{
    uint32_t cursor = s.len;

    for (uint32_t i = num; i > 0; i--) {
        rc_str_utf8_char c = rc_str_utf8_prev(s, cursor);
        uint32_t start = i > 1 ? steps[i - 2].cursor : 0;

        if (c.codepoint != steps[i - 1].codepoint || c.cursor != start) {
            return false;
        }
        cursor = c.cursor;
    }
    return cursor == 0;
}


// True if s's count and validity agree with the num steps in steps.
static bool random_whole_matches(rc_str s, const rc_str_utf8_char *steps, uint32_t num)
{
    bool valid = true;

    for (uint32_t i = 0; i < num; i++) {
        valid = valid && steps[i].codepoint != RC_CODEPOINT_NONE;
    }

    if (rc_str_utf8_count(s) != num) {
        return false;
    }
    return rc_str_utf8_is_valid(s) == valid;
}


// True if everything agrees with the forward walk through s.
static bool random_string_agrees(rc_str s)
{
    bool is_boundary[RANDOM_MAX_LEN + 1] = {0};
    rc_str_utf8_char steps[RANDOM_MAX_LEN] = {0};
    uint32_t num = random_walk(s, is_boundary, steps);

    if (num == RC_INDEX_NONE || !random_boundaries_match(s, is_boundary)) {
        return false;
    }

    if (!random_back_matches(s, steps, num)) {
        return false;
    }
    return random_whole_matches(s, steps, num);
}


RC_TEST(str_utf8, random_text_agrees_with_the_forward_walk)
{
    // Stepping back retraces stepping forward, and floor, ceil, is_boundary, count and is_valid
    // all agree with where it stepped
    rc_random random = rc_random_make(2024);
    uint32_t failures = 0;

    for (uint32_t n = 0; n < RANDOM_STRINGS; n++) {
        char bytes[RANDOM_MAX_LEN] = {0};
        uint32_t len = rc_random_next(&random) % (RANDOM_MAX_LEN + 1);

        for (uint32_t i = 0; i < len; i++) {
            bytes[i] = (char)random_bytes[rc_random_next(&random) % RC_COUNT(random_bytes)];
        }
        failures += random_string_agrees(rc_str_from_ptr(bytes, len)) ? 0u : 1u;
    }

    RC_CHECK(failures, ==, 0u);
}
