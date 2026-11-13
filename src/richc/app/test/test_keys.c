// test_keys.c
//
// Tests for the keycode rules every platform shares (src/keys.c).

#include "keys.h"

#include "richc/test.h"


RC_TEST(keys, named_keys_ignore_the_layout)
{
    RC_CHECK(rc_app_keycode(RC_SCANCODE_LEFT, 0), ==, (uint32_t)RC_KEY_LEFT);
    RC_CHECK(rc_app_keycode(RC_SCANCODE_ESCAPE, 'x'), ==, (uint32_t)RC_KEY_ESCAPE);
    RC_CHECK(rc_app_keycode(RC_SCANCODE_ENTER, 0), ==, (uint32_t)RC_KEY_ENTER);
    RC_CHECK(rc_app_keycode(RC_SCANCODE_RIGHT_SHIFT, 0), ==, (uint32_t)RC_KEY_RIGHT_SHIFT);

    // The keypad keeps its own keycodes whatever num lock makes it type
    RC_CHECK(rc_app_keycode(RC_SCANCODE_KP_4, '4'), ==, (uint32_t)RC_KEY_KP_4);
}


RC_TEST(keys, character_keys_follow_the_layout)
{
    // US
    RC_CHECK(rc_app_keycode(RC_SCANCODE_Z, 'z'), ==, (uint32_t)'z');

    // French AZERTY: Ctrl+Z is the key that types z, at the US W
    RC_CHECK(rc_app_keycode(RC_SCANCODE_W, 'z'), ==, (uint32_t)'z');
    RC_CHECK(rc_app_keycode(RC_SCANCODE_Q, 'a'), ==, (uint32_t)'a');

    // German QWERTZ
    RC_CHECK(rc_app_keycode(RC_SCANCODE_Y, 'z'), ==, (uint32_t)'z');

    // Spanish: the key right of L types n with a tilde (U+00F1), which is Latin
    RC_CHECK(rc_app_keycode(RC_SCANCODE_SEMICOLON, 0xF1), ==, 0xF1u);
}


RC_TEST(keys, keycodes_are_lower_case)
{
    RC_CHECK(rc_app_keycode(RC_SCANCODE_A, 'A'), ==, (uint32_t)'a');

    // German A with a diaeresis, U+00C4 to U+00E4
    RC_CHECK(rc_app_keycode(RC_SCANCODE_APOSTROPHE, 0xC4), ==, 0xE4u);
}


RC_TEST(keys, number_row_gives_digits)
{
    // French layouts type & there unshifted
    RC_CHECK(rc_app_keycode(RC_SCANCODE_1, '&'), ==, (uint32_t)'1');
    RC_CHECK(rc_app_keycode(RC_SCANCODE_0, 0xE0), ==, (uint32_t)'0');
    RC_CHECK(rc_app_keycode(RC_SCANCODE_5, 0), ==, (uint32_t)'5');
}


RC_TEST(keys, non_latin_layouts_fall_back_to_us)
{
    // Russian: the Z key types U+044F, so Ctrl+Z still undoes
    RC_CHECK(rc_app_keycode(RC_SCANCODE_Z, 0x44F), ==, (uint32_t)'z');
    RC_CHECK(rc_app_keycode(RC_SCANCODE_SEMICOLON, 0x436), ==, (uint32_t)';');

    // Greek
    RC_CHECK(rc_app_keycode(RC_SCANCODE_A, 0x3B1), ==, (uint32_t)'a');
}


RC_TEST(keys, unknown_layout_characters_fall_back_to_us)
{
    // A dead key, or a platform that cannot tell
    RC_CHECK(rc_app_keycode(RC_SCANCODE_LEFT_BRACKET, 0), ==, (uint32_t)'[');
    RC_CHECK(rc_app_keycode(RC_SCANCODE_M, 0), ==, (uint32_t)'m');
}


RC_TEST(keys, unmapped_scancodes)
{
    // A key with no known position goes by what it types
    RC_CHECK(rc_app_keycode(RC_SCANCODE_UNKNOWN, 'x'), ==, (uint32_t)'x');
    RC_CHECK(rc_app_keycode(RC_SCANCODE_UNKNOWN, 0x44F), ==, (uint32_t)RC_KEY_UNKNOWN);
    RC_CHECK(rc_app_keycode(RC_SCANCODE_UNKNOWN, 0), ==, (uint32_t)RC_KEY_UNKNOWN);
    RC_CHECK(rc_app_keycode((rc_scancode)500, 'x'), ==, (uint32_t)RC_KEY_UNKNOWN);
}


RC_TEST(keys, is_text)
{
    RC_CHECK_TRUE(rc_app_is_text('a'));
    RC_CHECK_TRUE(rc_app_is_text(' '));
    RC_CHECK_TRUE(rc_app_is_text(0xE9));
    RC_CHECK_TRUE(rc_app_is_text(0x1F600));
    RC_CHECK_FALSE(rc_app_is_text(0x08));
    RC_CHECK_FALSE(rc_app_is_text(0x0D));
    RC_CHECK_FALSE(rc_app_is_text(0x7F));
    RC_CHECK_FALSE(rc_app_is_text(0x85));
    RC_CHECK_FALSE(rc_app_is_text(0xD800));
    RC_CHECK_FALSE(rc_app_is_text(0x110000));
}
