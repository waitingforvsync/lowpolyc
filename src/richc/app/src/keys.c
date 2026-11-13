// keys.c
//
// The keycode rules, shared by every platform.

#include "keys.h"

// The character each character key types on a US layout, by scancode; 0 for other keys.
static const uint8_t rc_app_us_chars[RC_SCANCODE_COUNT] = {
    [RC_SCANCODE_A] = 'a',
    [RC_SCANCODE_B] = 'b',
    [RC_SCANCODE_C] = 'c',
    [RC_SCANCODE_D] = 'd',
    [RC_SCANCODE_E] = 'e',
    [RC_SCANCODE_F] = 'f',
    [RC_SCANCODE_G] = 'g',
    [RC_SCANCODE_H] = 'h',
    [RC_SCANCODE_I] = 'i',
    [RC_SCANCODE_J] = 'j',
    [RC_SCANCODE_K] = 'k',
    [RC_SCANCODE_L] = 'l',
    [RC_SCANCODE_M] = 'm',
    [RC_SCANCODE_N] = 'n',
    [RC_SCANCODE_O] = 'o',
    [RC_SCANCODE_P] = 'p',
    [RC_SCANCODE_Q] = 'q',
    [RC_SCANCODE_R] = 'r',
    [RC_SCANCODE_S] = 's',
    [RC_SCANCODE_T] = 't',
    [RC_SCANCODE_U] = 'u',
    [RC_SCANCODE_V] = 'v',
    [RC_SCANCODE_W] = 'w',
    [RC_SCANCODE_X] = 'x',
    [RC_SCANCODE_Y] = 'y',
    [RC_SCANCODE_Z] = 'z',
    [RC_SCANCODE_1] = '1',
    [RC_SCANCODE_2] = '2',
    [RC_SCANCODE_3] = '3',
    [RC_SCANCODE_4] = '4',
    [RC_SCANCODE_5] = '5',
    [RC_SCANCODE_6] = '6',
    [RC_SCANCODE_7] = '7',
    [RC_SCANCODE_8] = '8',
    [RC_SCANCODE_9] = '9',
    [RC_SCANCODE_0] = '0',
    [RC_SCANCODE_SPACE] = ' ',
    [RC_SCANCODE_MINUS] = '-',
    [RC_SCANCODE_EQUALS] = '=',
    [RC_SCANCODE_LEFT_BRACKET] = '[',
    [RC_SCANCODE_RIGHT_BRACKET] = ']',
    [RC_SCANCODE_BACKSLASH] = '\\',
    [RC_SCANCODE_SEMICOLON] = ';',
    [RC_SCANCODE_APOSTROPHE] = '\'',
    [RC_SCANCODE_GRAVE] = '`',
    [RC_SCANCODE_COMMA] = ',',
    [RC_SCANCODE_PERIOD] = '.',
    [RC_SCANCODE_SLASH] = '/',
    [RC_SCANCODE_NON_US_BACKSLASH] = '\\'
};

// The named keycode of every other key, by scancode; 0 for character keys.
static const uint16_t rc_app_named_keys[RC_SCANCODE_COUNT] = {
    [RC_SCANCODE_ENTER] = RC_KEY_ENTER,
    [RC_SCANCODE_ESCAPE] = RC_KEY_ESCAPE,
    [RC_SCANCODE_BACKSPACE] = RC_KEY_BACKSPACE,
    [RC_SCANCODE_TAB] = RC_KEY_TAB,
    [RC_SCANCODE_CAPS_LOCK] = RC_KEY_CAPS_LOCK,
    [RC_SCANCODE_F1] = RC_KEY_F1,
    [RC_SCANCODE_F2] = RC_KEY_F2,
    [RC_SCANCODE_F3] = RC_KEY_F3,
    [RC_SCANCODE_F4] = RC_KEY_F4,
    [RC_SCANCODE_F5] = RC_KEY_F5,
    [RC_SCANCODE_F6] = RC_KEY_F6,
    [RC_SCANCODE_F7] = RC_KEY_F7,
    [RC_SCANCODE_F8] = RC_KEY_F8,
    [RC_SCANCODE_F9] = RC_KEY_F9,
    [RC_SCANCODE_F10] = RC_KEY_F10,
    [RC_SCANCODE_F11] = RC_KEY_F11,
    [RC_SCANCODE_F12] = RC_KEY_F12,
    [RC_SCANCODE_PRINT_SCREEN] = RC_KEY_PRINT_SCREEN,
    [RC_SCANCODE_SCROLL_LOCK] = RC_KEY_SCROLL_LOCK,
    [RC_SCANCODE_PAUSE] = RC_KEY_PAUSE,
    [RC_SCANCODE_INSERT] = RC_KEY_INSERT,
    [RC_SCANCODE_HOME] = RC_KEY_HOME,
    [RC_SCANCODE_PAGE_UP] = RC_KEY_PAGE_UP,
    [RC_SCANCODE_DELETE] = RC_KEY_DELETE,
    [RC_SCANCODE_END] = RC_KEY_END,
    [RC_SCANCODE_PAGE_DOWN] = RC_KEY_PAGE_DOWN,
    [RC_SCANCODE_RIGHT] = RC_KEY_RIGHT,
    [RC_SCANCODE_LEFT] = RC_KEY_LEFT,
    [RC_SCANCODE_DOWN] = RC_KEY_DOWN,
    [RC_SCANCODE_UP] = RC_KEY_UP,
    [RC_SCANCODE_NUM_LOCK] = RC_KEY_NUM_LOCK,
    [RC_SCANCODE_KP_DIVIDE] = RC_KEY_KP_DIVIDE,
    [RC_SCANCODE_KP_MULTIPLY] = RC_KEY_KP_MULTIPLY,
    [RC_SCANCODE_KP_MINUS] = RC_KEY_KP_MINUS,
    [RC_SCANCODE_KP_PLUS] = RC_KEY_KP_PLUS,
    [RC_SCANCODE_KP_ENTER] = RC_KEY_KP_ENTER,
    [RC_SCANCODE_KP_1] = RC_KEY_KP_1,
    [RC_SCANCODE_KP_2] = RC_KEY_KP_2,
    [RC_SCANCODE_KP_3] = RC_KEY_KP_3,
    [RC_SCANCODE_KP_4] = RC_KEY_KP_4,
    [RC_SCANCODE_KP_5] = RC_KEY_KP_5,
    [RC_SCANCODE_KP_6] = RC_KEY_KP_6,
    [RC_SCANCODE_KP_7] = RC_KEY_KP_7,
    [RC_SCANCODE_KP_8] = RC_KEY_KP_8,
    [RC_SCANCODE_KP_9] = RC_KEY_KP_9,
    [RC_SCANCODE_KP_0] = RC_KEY_KP_0,
    [RC_SCANCODE_KP_PERIOD] = RC_KEY_KP_PERIOD,
    [RC_SCANCODE_MENU] = RC_KEY_MENU,
    [RC_SCANCODE_KP_EQUALS] = RC_KEY_KP_EQUALS,
    [RC_SCANCODE_LEFT_CTRL] = RC_KEY_LEFT_CTRL,
    [RC_SCANCODE_LEFT_SHIFT] = RC_KEY_LEFT_SHIFT,
    [RC_SCANCODE_LEFT_ALT] = RC_KEY_LEFT_ALT,
    [RC_SCANCODE_LEFT_SUPER] = RC_KEY_LEFT_SUPER,
    [RC_SCANCODE_RIGHT_CTRL] = RC_KEY_RIGHT_CTRL,
    [RC_SCANCODE_RIGHT_SHIFT] = RC_KEY_RIGHT_SHIFT,
    [RC_SCANCODE_RIGHT_ALT] = RC_KEY_RIGHT_ALT,
    [RC_SCANCODE_RIGHT_SUPER] = RC_KEY_RIGHT_SUPER
};

// True for a printable character from the Latin blocks (Basic Latin to Latin Extended-B).
static bool rc_app_is_latin(uint32_t codepoint)
{
    return rc_app_is_text(codepoint) && codepoint < 0x250;
}


// codepoint in lower case, for the letters of Basic Latin and Latin-1.
static uint32_t rc_app_lower(uint32_t codepoint)
{
    bool ascii_upper = codepoint >= 'A' && codepoint <= 'Z';
    bool latin1_upper = codepoint >= 0xC0 && codepoint <= 0xDE && codepoint != 0xD7;

    return ascii_upper || latin1_upper ? codepoint + 0x20 : codepoint;
}


uint32_t rc_app_keycode(rc_scancode scancode, uint32_t layout_char)
{
    if ((uint32_t)scancode >= RC_SCANCODE_COUNT) {
        return RC_KEY_UNKNOWN;
    }

    uint32_t named = rc_app_named_keys[scancode];
    if (named != 0) {
        return named;
    }

    // A key we know no position for keys by whatever it types
    uint32_t us_char = rc_app_us_chars[scancode];
    if (us_char == 0) {
        return rc_app_is_latin(layout_char) ? rc_app_lower(layout_char) : RC_KEY_UNKNOWN;
    }

    // The number row gives digits whatever the layout types there, and a key whose character is not
    // Latin (or is not known, or is a dead key) gives the US one
    if (scancode >= RC_SCANCODE_1 && scancode <= RC_SCANCODE_0) {
        return us_char;
    }
    return rc_app_is_latin(layout_char) ? rc_app_lower(layout_char) : us_char;
}


bool rc_app_is_text(uint32_t codepoint)
{
    bool control = codepoint < 0x20 || (codepoint >= 0x7F && codepoint < 0xA0);
    bool surrogate = codepoint >= 0xD800 && codepoint < 0xE000;

    return !control && !surrogate && codepoint <= 0x10FFFF;
}
