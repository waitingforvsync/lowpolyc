// keys.h
//
// The keyboard, three ways:
//
//   rc_scancode  where a key is, whatever the layout: for keys as controls (viewport, game)
//   rc_key       what a key means in the current layout: for hotkeys
//   rc_modifier  the modifier keys held and the locks on, at the time of an event
//
// Scancodes are named for the key at that place on a US keyboard: RC_SCANCODE_Q is the top-left
// letter, even where the layout puts A or ' there. Their values are USB HID keyboard usages (usage
// page 0x07), as SDL's are.
//
// A keycode (rc_key) is a Unicode code point for a key that types a character: the character it
// types with no modifiers, lowercased, so Ctrl+Z is 'z' on a French AZERTY keyboard, wherever the
// Z key is. Two adjustments keep hotkeys working everywhere, as SDL's do: the number row always
// gives the digits '0' to '9' (French layouts type & and the like there unshifted), and a key whose
// layout character is not Latin (Russian, Greek, ...) gives the US character at its position.
// Every other key has a named keycode below.
//
// The named keycodes are also how char events carry the editing keys (arrows, backspace and so
// on), so a text field reads one stream. The ones macOS has use its function-key code points from
// Unicode's private use area, and the rest follow on from them.
//
// Every platform gives the same values, so they can be compared with the constants, stored in a
// config file or written into tests.

#ifndef RC_APP_KEYS_H_
#define RC_APP_KEYS_H_

// ---- scancodes ----

typedef enum rc_scancode {
    RC_SCANCODE_UNKNOWN = 0,

    RC_SCANCODE_A = 4,
    RC_SCANCODE_B = 5,
    RC_SCANCODE_C = 6,
    RC_SCANCODE_D = 7,
    RC_SCANCODE_E = 8,
    RC_SCANCODE_F = 9,
    RC_SCANCODE_G = 10,
    RC_SCANCODE_H = 11,
    RC_SCANCODE_I = 12,
    RC_SCANCODE_J = 13,
    RC_SCANCODE_K = 14,
    RC_SCANCODE_L = 15,
    RC_SCANCODE_M = 16,
    RC_SCANCODE_N = 17,
    RC_SCANCODE_O = 18,
    RC_SCANCODE_P = 19,
    RC_SCANCODE_Q = 20,
    RC_SCANCODE_R = 21,
    RC_SCANCODE_S = 22,
    RC_SCANCODE_T = 23,
    RC_SCANCODE_U = 24,
    RC_SCANCODE_V = 25,
    RC_SCANCODE_W = 26,
    RC_SCANCODE_X = 27,
    RC_SCANCODE_Y = 28,
    RC_SCANCODE_Z = 29,

    RC_SCANCODE_1 = 30,
    RC_SCANCODE_2 = 31,
    RC_SCANCODE_3 = 32,
    RC_SCANCODE_4 = 33,
    RC_SCANCODE_5 = 34,
    RC_SCANCODE_6 = 35,
    RC_SCANCODE_7 = 36,
    RC_SCANCODE_8 = 37,
    RC_SCANCODE_9 = 38,
    RC_SCANCODE_0 = 39,

    RC_SCANCODE_ENTER = 40,
    RC_SCANCODE_ESCAPE = 41,
    RC_SCANCODE_BACKSPACE = 42,
    RC_SCANCODE_TAB = 43,
    RC_SCANCODE_SPACE = 44,
    RC_SCANCODE_MINUS = 45,
    RC_SCANCODE_EQUALS = 46,
    RC_SCANCODE_LEFT_BRACKET = 47,
    RC_SCANCODE_RIGHT_BRACKET = 48,
    RC_SCANCODE_BACKSLASH = 49,
    RC_SCANCODE_SEMICOLON = 51,
    RC_SCANCODE_APOSTROPHE = 52,
    RC_SCANCODE_GRAVE = 53,
    RC_SCANCODE_COMMA = 54,
    RC_SCANCODE_PERIOD = 55,
    RC_SCANCODE_SLASH = 56,
    RC_SCANCODE_CAPS_LOCK = 57,

    RC_SCANCODE_F1 = 58,
    RC_SCANCODE_F2 = 59,
    RC_SCANCODE_F3 = 60,
    RC_SCANCODE_F4 = 61,
    RC_SCANCODE_F5 = 62,
    RC_SCANCODE_F6 = 63,
    RC_SCANCODE_F7 = 64,
    RC_SCANCODE_F8 = 65,
    RC_SCANCODE_F9 = 66,
    RC_SCANCODE_F10 = 67,
    RC_SCANCODE_F11 = 68,
    RC_SCANCODE_F12 = 69,

    RC_SCANCODE_PRINT_SCREEN = 70,
    RC_SCANCODE_SCROLL_LOCK = 71,
    RC_SCANCODE_PAUSE = 72,
    RC_SCANCODE_INSERT = 73,
    RC_SCANCODE_HOME = 74,
    RC_SCANCODE_PAGE_UP = 75,
    RC_SCANCODE_DELETE = 76,
    RC_SCANCODE_END = 77,
    RC_SCANCODE_PAGE_DOWN = 78,
    RC_SCANCODE_RIGHT = 79,
    RC_SCANCODE_LEFT = 80,
    RC_SCANCODE_DOWN = 81,
    RC_SCANCODE_UP = 82,

    RC_SCANCODE_NUM_LOCK = 83,
    RC_SCANCODE_KP_DIVIDE = 84,
    RC_SCANCODE_KP_MULTIPLY = 85,
    RC_SCANCODE_KP_MINUS = 86,
    RC_SCANCODE_KP_PLUS = 87,
    RC_SCANCODE_KP_ENTER = 88,
    RC_SCANCODE_KP_1 = 89,
    RC_SCANCODE_KP_2 = 90,
    RC_SCANCODE_KP_3 = 91,
    RC_SCANCODE_KP_4 = 92,
    RC_SCANCODE_KP_5 = 93,
    RC_SCANCODE_KP_6 = 94,
    RC_SCANCODE_KP_7 = 95,
    RC_SCANCODE_KP_8 = 96,
    RC_SCANCODE_KP_9 = 97,
    RC_SCANCODE_KP_0 = 98,
    RC_SCANCODE_KP_PERIOD = 99,

    // The extra key beside left shift on ISO keyboards
    RC_SCANCODE_NON_US_BACKSLASH = 100,
    RC_SCANCODE_MENU = 101,
    RC_SCANCODE_KP_EQUALS = 103,

    RC_SCANCODE_LEFT_CTRL = 224,
    RC_SCANCODE_LEFT_SHIFT = 225,
    RC_SCANCODE_LEFT_ALT = 226,
    RC_SCANCODE_LEFT_SUPER = 227,
    RC_SCANCODE_RIGHT_CTRL = 228,
    RC_SCANCODE_RIGHT_SHIFT = 229,
    RC_SCANCODE_RIGHT_ALT = 230,
    RC_SCANCODE_RIGHT_SUPER = 231,

    // One past the largest scancode, for tables indexed by them
    RC_SCANCODE_COUNT = 232
} rc_scancode;

// ---- keycodes ----

typedef enum rc_key {
    RC_KEY_UNKNOWN = 0,

    // Keys whose keycodes are ASCII control characters
    RC_KEY_BACKSPACE = 0x08,
    RC_KEY_TAB = 0x09,
    RC_KEY_ENTER = 0x0D,
    RC_KEY_ESCAPE = 0x1B,

    // A character key, named for convenience
    RC_KEY_SPACE = 0x20,

    // macOS's function-key code points
    RC_KEY_UP = 0xF700,
    RC_KEY_DOWN = 0xF701,
    RC_KEY_LEFT = 0xF702,
    RC_KEY_RIGHT = 0xF703,
    RC_KEY_F1 = 0xF704,
    RC_KEY_F2 = 0xF705,
    RC_KEY_F3 = 0xF706,
    RC_KEY_F4 = 0xF707,
    RC_KEY_F5 = 0xF708,
    RC_KEY_F6 = 0xF709,
    RC_KEY_F7 = 0xF70A,
    RC_KEY_F8 = 0xF70B,
    RC_KEY_F9 = 0xF70C,
    RC_KEY_F10 = 0xF70D,
    RC_KEY_F11 = 0xF70E,
    RC_KEY_F12 = 0xF70F,
    RC_KEY_INSERT = 0xF727,
    RC_KEY_DELETE = 0xF728,
    RC_KEY_HOME = 0xF729,
    RC_KEY_END = 0xF72B,
    RC_KEY_PAGE_UP = 0xF72C,
    RC_KEY_PAGE_DOWN = 0xF72D,
    RC_KEY_PRINT_SCREEN = 0xF72E,
    RC_KEY_SCROLL_LOCK = 0xF72F,
    RC_KEY_PAUSE = 0xF730,
    RC_KEY_MENU = 0xF735,

    // Keys macOS has no code point for
    RC_KEY_CAPS_LOCK = 0xF780,
    RC_KEY_NUM_LOCK = 0xF781,
    RC_KEY_LEFT_SHIFT = 0xF782,
    RC_KEY_RIGHT_SHIFT = 0xF783,
    RC_KEY_LEFT_CTRL = 0xF784,
    RC_KEY_RIGHT_CTRL = 0xF785,
    RC_KEY_LEFT_ALT = 0xF786,
    RC_KEY_RIGHT_ALT = 0xF787,
    RC_KEY_LEFT_SUPER = 0xF788,
    RC_KEY_RIGHT_SUPER = 0xF789,

    // The keypad's keys, whatever Num Lock says (char events follow Num Lock instead)
    RC_KEY_KP_0 = 0xF790,
    RC_KEY_KP_1 = 0xF791,
    RC_KEY_KP_2 = 0xF792,
    RC_KEY_KP_3 = 0xF793,
    RC_KEY_KP_4 = 0xF794,
    RC_KEY_KP_5 = 0xF795,
    RC_KEY_KP_6 = 0xF796,
    RC_KEY_KP_7 = 0xF797,
    RC_KEY_KP_8 = 0xF798,
    RC_KEY_KP_9 = 0xF799,
    RC_KEY_KP_PERIOD = 0xF79A,
    RC_KEY_KP_DIVIDE = 0xF79B,
    RC_KEY_KP_MULTIPLY = 0xF79C,
    RC_KEY_KP_MINUS = 0xF79D,
    RC_KEY_KP_PLUS = 0xF79E,
    RC_KEY_KP_ENTER = 0xF79F,
    RC_KEY_KP_EQUALS = 0xF7A0
} rc_key;

// ---- modifiers ----

// The held bits mean either side's key is down: the scancodes tell left from right. Alt is option,
// and super is the Windows or command key.
typedef enum rc_modifier {
    RC_MODIFIER_SHIFT = 1 << 0,
    RC_MODIFIER_CTRL = 1 << 1,
    RC_MODIFIER_ALT = 1 << 2,
    RC_MODIFIER_SUPER = 1 << 3,
    RC_MODIFIER_CAPS_LOCK = 1 << 4,
    RC_MODIFIER_NUM_LOCK = 1 << 5
} rc_modifier;

#endif // RC_APP_KEYS_H_
