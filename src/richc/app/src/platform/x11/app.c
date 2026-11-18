// app.c
//
// X11, through Xlib: one window, its events, and the native frame loop.

#include "richc/app/app.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <locale.h>

#include "keys.h"
#include "platform/x11/x11.h"
#include "richc/print.h"

enum {
    RC_X11_DEFAULT_WIDTH = 1280,
    RC_X11_DEFAULT_HEIGHT = 720,

    // The events we ask the window for. The input method may want more of its own.
    RC_X11_EVENT_MASK = KeyPressMask | KeyReleaseMask | FocusChangeMask | StructureNotifyMask
};

// The open window, while rc_app_run is running.
static const rc_x11_window *rc_x11_open_window;


// Every current X server (Xorg with evdev or libinput, and XWayland) numbers keys by their Linux
// evdev code plus 8, which names a physical position whatever the layout.
enum {
    RC_X11_EVDEV_OFFSET = 8,

    // The evdev codes we map: the keys of a full PC keyboard
    RC_X11_EVDEV_CODES = 128
};

// Scancodes by evdev code.
static const uint8_t rc_x11_scancodes[RC_X11_EVDEV_CODES] = {
    [1] = RC_SCANCODE_ESCAPE,
    [2] = RC_SCANCODE_1,
    [3] = RC_SCANCODE_2,
    [4] = RC_SCANCODE_3,
    [5] = RC_SCANCODE_4,
    [6] = RC_SCANCODE_5,
    [7] = RC_SCANCODE_6,
    [8] = RC_SCANCODE_7,
    [9] = RC_SCANCODE_8,
    [10] = RC_SCANCODE_9,
    [11] = RC_SCANCODE_0,
    [12] = RC_SCANCODE_MINUS,
    [13] = RC_SCANCODE_EQUALS,
    [14] = RC_SCANCODE_BACKSPACE,
    [15] = RC_SCANCODE_TAB,
    [16] = RC_SCANCODE_Q,
    [17] = RC_SCANCODE_W,
    [18] = RC_SCANCODE_E,
    [19] = RC_SCANCODE_R,
    [20] = RC_SCANCODE_T,
    [21] = RC_SCANCODE_Y,
    [22] = RC_SCANCODE_U,
    [23] = RC_SCANCODE_I,
    [24] = RC_SCANCODE_O,
    [25] = RC_SCANCODE_P,
    [26] = RC_SCANCODE_LEFT_BRACKET,
    [27] = RC_SCANCODE_RIGHT_BRACKET,
    [28] = RC_SCANCODE_ENTER,
    [29] = RC_SCANCODE_LEFT_CTRL,
    [30] = RC_SCANCODE_A,
    [31] = RC_SCANCODE_S,
    [32] = RC_SCANCODE_D,
    [33] = RC_SCANCODE_F,
    [34] = RC_SCANCODE_G,
    [35] = RC_SCANCODE_H,
    [36] = RC_SCANCODE_J,
    [37] = RC_SCANCODE_K,
    [38] = RC_SCANCODE_L,
    [39] = RC_SCANCODE_SEMICOLON,
    [40] = RC_SCANCODE_APOSTROPHE,
    [41] = RC_SCANCODE_GRAVE,
    [42] = RC_SCANCODE_LEFT_SHIFT,
    [43] = RC_SCANCODE_BACKSLASH,
    [44] = RC_SCANCODE_Z,
    [45] = RC_SCANCODE_X,
    [46] = RC_SCANCODE_C,
    [47] = RC_SCANCODE_V,
    [48] = RC_SCANCODE_B,
    [49] = RC_SCANCODE_N,
    [50] = RC_SCANCODE_M,
    [51] = RC_SCANCODE_COMMA,
    [52] = RC_SCANCODE_PERIOD,
    [53] = RC_SCANCODE_SLASH,
    [54] = RC_SCANCODE_RIGHT_SHIFT,
    [55] = RC_SCANCODE_KP_MULTIPLY,
    [56] = RC_SCANCODE_LEFT_ALT,
    [57] = RC_SCANCODE_SPACE,
    [58] = RC_SCANCODE_CAPS_LOCK,
    [59] = RC_SCANCODE_F1,
    [60] = RC_SCANCODE_F2,
    [61] = RC_SCANCODE_F3,
    [62] = RC_SCANCODE_F4,
    [63] = RC_SCANCODE_F5,
    [64] = RC_SCANCODE_F6,
    [65] = RC_SCANCODE_F7,
    [66] = RC_SCANCODE_F8,
    [67] = RC_SCANCODE_F9,
    [68] = RC_SCANCODE_F10,
    [69] = RC_SCANCODE_NUM_LOCK,
    [70] = RC_SCANCODE_SCROLL_LOCK,
    [71] = RC_SCANCODE_KP_7,
    [72] = RC_SCANCODE_KP_8,
    [73] = RC_SCANCODE_KP_9,
    [74] = RC_SCANCODE_KP_MINUS,
    [75] = RC_SCANCODE_KP_4,
    [76] = RC_SCANCODE_KP_5,
    [77] = RC_SCANCODE_KP_6,
    [78] = RC_SCANCODE_KP_PLUS,
    [79] = RC_SCANCODE_KP_1,
    [80] = RC_SCANCODE_KP_2,
    [81] = RC_SCANCODE_KP_3,
    [82] = RC_SCANCODE_KP_0,
    [83] = RC_SCANCODE_KP_PERIOD,
    [86] = RC_SCANCODE_NON_US_BACKSLASH,
    [87] = RC_SCANCODE_F11,
    [88] = RC_SCANCODE_F12,
    [96] = RC_SCANCODE_KP_ENTER,
    [97] = RC_SCANCODE_RIGHT_CTRL,
    [98] = RC_SCANCODE_KP_DIVIDE,
    [99] = RC_SCANCODE_PRINT_SCREEN,
    [100] = RC_SCANCODE_RIGHT_ALT,
    [102] = RC_SCANCODE_HOME,
    [103] = RC_SCANCODE_UP,
    [104] = RC_SCANCODE_PAGE_UP,
    [105] = RC_SCANCODE_LEFT,
    [106] = RC_SCANCODE_RIGHT,
    [107] = RC_SCANCODE_END,
    [108] = RC_SCANCODE_DOWN,
    [109] = RC_SCANCODE_PAGE_DOWN,
    [110] = RC_SCANCODE_INSERT,
    [111] = RC_SCANCODE_DELETE,
    [117] = RC_SCANCODE_KP_EQUALS,
    [119] = RC_SCANCODE_PAUSE,
    [125] = RC_SCANCODE_LEFT_SUPER,
    [126] = RC_SCANCODE_RIGHT_SUPER,
    [127] = RC_SCANCODE_MENU
};

// The scancode of an X keycode.
static rc_scancode rc_x11_scancode(uint32_t keycode)
{
    if (keycode < RC_X11_EVDEV_OFFSET) {
        return RC_SCANCODE_NONE;
    }

    if (keycode - RC_X11_EVDEV_OFFSET >= RC_X11_EVDEV_CODES) {
        return RC_SCANCODE_NONE;
    }

    return (rc_scancode)rc_x11_scancodes[keycode - RC_X11_EVDEV_OFFSET];
}


// A character of typed text, and how many bytes it took.
typedef struct rc_x11_char {
    uint32_t codepoint;
    uint32_t size;
} rc_x11_char;


// How many continuation bytes follow a UTF-8 lead byte.
static uint32_t rc_x11_utf8_continuations(uint32_t lead)
{
    if (lead >= 0xF0) {
        return 3;
    }

    if (lead >= 0xE0) {
        return 2;
    }

    return lead >= 0xC0 ? 1 : 0;
}


// The bits of a code point a UTF-8 lead byte holds: those below its run of high ones and the zero
// after them.
static uint32_t rc_x11_utf8_lead_bits(uint32_t lead, uint32_t extra)
{
    return lead & (0x7Fu >> (extra + 1));
}


// The character at the start of text, which holds len (non-zero) bytes of UTF-8. Malformed UTF-8
// gives U+FFFD for its first byte.
static rc_x11_char rc_x11_decode_utf8(const unsigned char *text, uint32_t len)
{
    uint32_t lead = text[0];
    uint32_t extra = rc_x11_utf8_continuations(lead);
    uint32_t codepoint = rc_x11_utf8_lead_bits(lead, extra);
    rc_x11_char malformed = {.codepoint = 0xFFFD, .size = 1};

    if (extra == 0) {
        return lead < 0x80 ? (rc_x11_char) {.codepoint = lead, .size = 1} : malformed;
    }

    if (lead >= 0xF8) {
        return malformed;
    }

    if (extra >= len) {
        return malformed;
    }

    for (uint32_t i = 1; i <= extra; i++) {
        if ((text[i] & 0xC0) != 0x80) {
            return malformed;
        }

        codepoint = (codepoint << 6) | (text[i] & 0x3F);
    }

    return (rc_x11_char) {.codepoint = codepoint, .size = extra + 1};
}


// True for a keysym of a printable Latin-1 character, whose value is its code point.
static bool rc_x11_keysym_is_latin1(KeySym keysym)
{
    bool ascii = keysym >= 0x20 && keysym <= 0x7E;
    bool upper_half = keysym >= 0xA0 && keysym <= 0xFF;

    return ascii || upper_half;
}


// The character a keysym types, or 0 if it is not a character, or is one of the legacy non-Latin
// keysyms without a Unicode form (which keycodes replace with the US character anyway).
static uint32_t rc_x11_keysym_char(KeySym keysym)
{
    if (rc_x11_keysym_is_latin1(keysym)) {
        return (uint32_t)keysym;
    }

    // Keysyms from 0x01000000 up hold a code point in their low 24 bits
    if ((keysym & 0xFF000000) == 0x01000000) {
        return (uint32_t)(keysym & 0x00FFFFFF);
    }

    return 0;
}


// The keycode of a key event: its scancode, and the character its key types in the layout the
// event was typed in, with no modifiers.
static uint32_t rc_x11_keycode(const rc_x11_window *w, const XKeyEvent *xkey)
{
    uint32_t group = XkbGroupForCoreState(xkey->state);
    KeySym keysym = XkbKeycodeToKeysym(w->display, (KeyCode)xkey->keycode, (int)group, 0);

    return rc_app_keycode(rc_x11_scancode(xkey->keycode), rc_x11_keysym_char(keysym));
}


// The editing key a keysym stands for in char events, or 0. The keypad's count too, when num lock
// makes them move rather than type digits.
static uint32_t rc_x11_editing_key(KeySym keysym)
{
    switch (keysym) {
        case XK_BackSpace:
            return RC_KEY_BACKSPACE;
        case XK_Tab:
        case XK_ISO_Left_Tab:
            return RC_KEY_TAB;
        case XK_Return:
        case XK_KP_Enter:
            return RC_KEY_ENTER;
        case XK_Escape:
            return RC_KEY_ESCAPE;
        case XK_Left:
        case XK_KP_Left:
            return RC_KEY_LEFT;
        case XK_Right:
        case XK_KP_Right:
            return RC_KEY_RIGHT;
        case XK_Up:
        case XK_KP_Up:
            return RC_KEY_UP;
        case XK_Down:
        case XK_KP_Down:
            return RC_KEY_DOWN;
        case XK_Home:
        case XK_KP_Home:
            return RC_KEY_HOME;
        case XK_End:
        case XK_KP_End:
            return RC_KEY_END;
        case XK_Prior:
        case XK_KP_Prior:
            return RC_KEY_PAGE_UP;
        case XK_Next:
        case XK_KP_Next:
            return RC_KEY_PAGE_DOWN;
        case XK_Insert:
        case XK_KP_Insert:
            return RC_KEY_INSERT;
        case XK_Delete:
        case XK_KP_Delete:
            return RC_KEY_DELETE;
        default:
            return 0;
    }
}


// True for the keysym of an alt (or meta) key.
static bool rc_x11_keysym_is_alt(KeySym keysym)
{
    switch (keysym) {
        case XK_Alt_L:
        case XK_Alt_R:
        case XK_Meta_L:
        case XK_Meta_R:
            return true;
        default:
            return false;
    }
}


// If keysym is alt, super or num lock, note that its key sits on the X modifier mask.
static void rc_x11_note_modifier_key(rc_x11_window *w, KeySym keysym, uint32_t mask)
{
    if (rc_x11_keysym_is_alt(keysym)) {
        w->alt_mask |= mask;
    }
    else if (keysym == XK_Super_L || keysym == XK_Super_R) {
        w->super_mask |= mask;
    }
    else if (keysym == XK_Num_Lock) {
        w->num_lock_mask |= mask;
    }
}


// Find which X modifier masks alt, super and num lock are on. The server may move them, and says
// so with a MappingNotify.
static void rc_x11_read_modifier_masks(rc_x11_window *w)
{
    XModifierKeymap *map = XGetModifierMapping(w->display);

    w->alt_mask = 0;
    w->super_mask = 0;
    w->num_lock_mask = 0;

    // Rows 0 to 2 are shift, lock and control; rows 3 to 7 are Mod1 to Mod5
    for (int row = 3; row < 8; row++) {
        for (int i = 0; i < map->max_keypermod; i++) {
            KeyCode keycode = map->modifiermap[row * map->max_keypermod + i];
            KeySym keysym = keycode ? XkbKeycodeToKeysym(w->display, keycode, 0, 0) : NoSymbol;

            rc_x11_note_modifier_key(w, keysym, 1u << row);
        }
    }
    XFreeModifiermap(map);
}


// The rc_modifier bits of an X modifier state.
static uint8_t rc_x11_modifiers(const rc_x11_window *w, uint32_t state)
{
    uint32_t modifiers = 0;

    modifiers |= (state & ShiftMask) ? RC_MODIFIER_SHIFT : 0;
    modifiers |= (state & ControlMask) ? RC_MODIFIER_CTRL : 0;
    modifiers |= (state & w->alt_mask) ? RC_MODIFIER_ALT : 0;
    modifiers |= (state & w->super_mask) ? RC_MODIFIER_SUPER : 0;
    modifiers |= (state & LockMask) ? RC_MODIFIER_CAPS_LOCK : 0;
    modifiers |= (state & w->num_lock_mask) ? RC_MODIFIER_NUM_LOCK : 0;
    return (uint8_t)modifiers;
}


// The rc_modifier bit a modifier key holds, or 0 for any other key.
static uint8_t rc_x11_modifier_held_by(rc_scancode scancode)
{
    switch (scancode) {
        case RC_SCANCODE_LEFT_SHIFT:
        case RC_SCANCODE_RIGHT_SHIFT:
            return RC_MODIFIER_SHIFT;
        case RC_SCANCODE_LEFT_CTRL:
        case RC_SCANCODE_RIGHT_CTRL:
            return RC_MODIFIER_CTRL;
        case RC_SCANCODE_LEFT_ALT:
        case RC_SCANCODE_RIGHT_ALT:
            return RC_MODIFIER_ALT;
        case RC_SCANCODE_LEFT_SUPER:
        case RC_SCANCODE_RIGHT_SUPER:
            return RC_MODIFIER_SUPER;
        default:
            return 0;
    }
}


// The held rc_modifier bits of the keys we have seen go down and not come up.
static uint8_t rc_x11_modifiers_held(const rc_x11_window *w)
{
    uint8_t held = 0;

    for (uint32_t keycode = 0; keycode < RC_X11_KEYCODES; keycode++) {
        if (w->key_down[keycode]) {
            held |= rc_x11_modifier_held_by(rc_x11_scancode(keycode));
        }
    }
    return held;
}


// The rc_modifier bit a lock key toggles, or 0 for any other key.
static uint8_t rc_x11_lock_toggled_by(rc_scancode scancode)
{
    switch (scancode) {
        case RC_SCANCODE_CAPS_LOCK:
            return RC_MODIFIER_CAPS_LOCK;
        case RC_SCANCODE_NUM_LOCK:
            return RC_MODIFIER_NUM_LOCK;
        default:
            return 0;
    }
}


// modifiers with a modifier key's own press or release applied. Released, its bit still counts if
// the key on the other side is down.
static uint8_t rc_x11_apply_modifier_key(const rc_x11_window *w, uint8_t modifiers, rc_scancode scancode, bool down)
{
    uint8_t held = rc_x11_modifier_held_by(scancode);

    if (held == 0) {
        return modifiers;
    }

    if (down) {
        return (uint8_t)(modifiers | held);
    }

    return (uint8_t)((modifiers & ~held) | (rc_x11_modifiers_held(w) & held));
}


// modifiers with a lock key's own press or release applied. A lock toggles on its press, as on
// every other platform. (X turns a lock off on the second release instead, so that release still
// reports it on: it keeps the press's.)
static uint8_t rc_x11_apply_lock_key(const rc_x11_window *w, uint8_t modifiers, rc_scancode scancode, bool down)
{
    uint8_t lock = rc_x11_lock_toggled_by(scancode);

    if (lock == 0) {
        return modifiers;
    }

    if (down) {
        return (uint8_t)(modifiers ^ lock);
    }

    return (uint8_t)((modifiers & ~lock) | (w->modifiers & lock));
}


// The modifiers after a key event. X gives the state from before it, so a modifier key's own press
// or release is applied here.
static uint8_t rc_x11_modifiers_after(const rc_x11_window *w, const XKeyEvent *xkey, bool down)
{
    rc_scancode scancode = rc_x11_scancode(xkey->keycode);
    uint8_t modifiers = rc_x11_apply_modifier_key(w, rc_x11_modifiers(w, xkey->state), scancode, down);

    return rc_x11_apply_lock_key(w, modifiers, scancode, down);
}


// Pass event to the app's event callback, if it has one.
static void rc_x11_send(const rc_app_desc *desc, rc_app_event event)
{
    if (desc->event) {
        desc->event(desc->user, &event);
    }
}


// Give the window a UTF-8 title.
static void rc_x11_set_title(rc_x11_window *w, rc_str title)
{
    Atom net_wm_name = XInternAtom(w->display, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(w->display, "UTF8_STRING", False);
    const unsigned char *text = (const unsigned char *)title.data;

    // _NET_WM_NAME is what window managers show; WM_NAME is the old fallback
    XChangeProperty(w->display, w->window, net_wm_name, utf8_string, 8, PropModeReplace, text, (int)title.len);
    XChangeProperty(w->display, w->window, XA_WM_NAME, XA_STRING, 8, PropModeReplace, text, (int)title.len);
}


// Make the window on the open display, with its title, asking for the events we handle.
static void rc_x11_create_window(rc_x11_window *w, const rc_app_desc *desc)
{
    int screen = DefaultScreen(w->display);
    uint32_t width = desc->width ? desc->width : RC_X11_DEFAULT_WIDTH;
    uint32_t height = desc->height ? desc->height : RC_X11_DEFAULT_HEIGHT;
    XSetWindowAttributes attributes = {
        .background_pixel = BlackPixel(w->display, screen),
        .event_mask = RC_X11_EVENT_MASK
    };

    w->window = XCreateWindow(w->display, RootWindow(w->display, screen), 0, 0, width, height, 0, CopyFromParent,
                              InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &attributes);
    if (!rc_str_is_empty(desc->title)) {
        rc_x11_set_title(w, desc->title);
    }

    // Ask to be told when the close button is pressed, rather than have the connection cut
    w->wm_delete_window = XInternAtom(w->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(w->display, w->window, &w->wm_delete_window, 1);
}


// Open an input method and our context in it, where the display has one. It applies the layout,
// dead keys and compose to make text, in the locale's character set; a program that never chose
// one runs in "C", so we take the user's. Without an input method, text falls back to Latin-1.
static void rc_x11_open_input_method(rc_x11_window *w)
{
    if (rc_str_is_equal(rc_str_from_cstr(setlocale(LC_CTYPE, NULL)), RC_STR("C"))) {
        setlocale(LC_CTYPE, "");
    }

    XSetLocaleModifiers("");
    w->im = XOpenIM(w->display, NULL, NULL, NULL);
    if (w->im) {
        w->ic = XCreateIC(w->im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, w->window,
                          XNFocusWindow, w->window, NULL);
    }
}


// Ask for the events the input method needs as well as ours.
static void rc_x11_select_input_method_events(rc_x11_window *w)
{
    long im_events = 0;

    XGetICValues(w->ic, XNFilterEvents, &im_events, NULL);
    XSelectInput(w->display, w->window, RC_X11_EVENT_MASK | im_events);
}


// Open the display and show the window. False if there is no display to open.
static bool rc_x11_open(rc_x11_window *w, const rc_app_desc *desc)
{
    w->display = XOpenDisplay(NULL);
    if (!w->display) {
        return false;
    }

    rc_x11_create_window(w, desc);

    // A held key then repeats as presses alone, with no releases in between. Where the server
    // cannot do that, rc_x11_handle_key_release spots the pairs instead.
    XkbSetDetectableAutoRepeat(w->display, True, NULL);
    rc_x11_open_input_method(w);
    rc_x11_read_modifier_masks(w);
    if (w->ic) {
        rc_x11_select_input_method_events(w);
    }

    XMapWindow(w->display, w->window);
    XFlush(w->display);
    return true;
}


// Destroy the window and close the display.
static void rc_x11_close(rc_x11_window *w)
{
    if (w->ic) {
        XDestroyIC(w->ic);
    }

    if (w->im) {
        XCloseIM(w->im);
    }

    XDestroyWindow(w->display, w->window);
    XCloseDisplay(w->display);
}


// What a key press types, as XLookupString or Xutf8LookupString look it up.
typedef struct rc_x11_typed {
    char text[64];
    int len;
    KeySym keysym;
    Status status;
} rc_x11_typed;


// What a key press types through the input method, as UTF-8.
static rc_x11_typed rc_x11_lookup_utf8(XIC ic, XKeyEvent *xkey)
{
    rc_x11_typed typed = {.keysym = NoSymbol, .status = XLookupNone};

    typed.len = Xutf8LookupString(ic, xkey, typed.text, (int)sizeof typed.text, &typed.keysym, &typed.status);
    return typed;
}


// What a key press types with no input method: Latin-1 text, whose bytes are their own code points.
static rc_x11_typed rc_x11_lookup_latin1(XKeyEvent *xkey)
{
    rc_x11_typed typed = {.keysym = NoSymbol, .status = XLookupNone};

    typed.len = XLookupString(xkey, typed.text, (int)sizeof typed.text, &typed.keysym, NULL);
    typed.status = typed.len > 0 ? XLookupBoth : XLookupKeySym;
    return typed;
}


// The editing key a lookup found, or 0.
static uint32_t rc_x11_typed_editing_key(const rc_x11_typed *typed)
{
    if (typed->status != XLookupKeySym && typed->status != XLookupBoth) {
        return 0;
    }

    return rc_x11_editing_key(typed->keysym);
}


// The character at byte i of a lookup's text, which is UTF-8, or Latin-1 if not utf8.
static rc_x11_char rc_x11_typed_char(const rc_x11_typed *typed, uint32_t i, bool utf8)
{
    const unsigned char *bytes = (const unsigned char *)typed->text;

    if (!utf8) {
        return (rc_x11_char) {.codepoint = bytes[i], .size = 1};
    }

    return rc_x11_decode_utf8(bytes + i, (uint32_t)typed->len - i);
}


// Send a char event for each character of a lookup's text: compose can type more than one at once.
static void rc_x11_send_text(const rc_x11_typed *typed, bool utf8, uint8_t modifiers, const rc_app_desc *desc)
{
    uint32_t i = 0;

    while (i < (uint32_t)typed->len) {
        rc_x11_char c = rc_x11_typed_char(typed, i, utf8);

        if (rc_app_is_text(c.codepoint)) {
            rc_x11_send(desc, (rc_app_event) {.type = RC_APP_EVENT_TYPE_CHAR, .modifiers = modifiers, .codepoint = c.codepoint});
        }

        i += c.size;
    }
}


// Send the char events a key press types: an editing key, or text. Text with ctrl or super held is
// a hotkey, not typing, so gives none.
static void rc_x11_send_typing(rc_x11_window *w, XKeyEvent *xkey, uint8_t modifiers, const rc_app_desc *desc)
{
    rc_x11_typed typed = w->ic ? rc_x11_lookup_utf8(w->ic, xkey) : rc_x11_lookup_latin1(xkey);
    uint32_t editing_key = rc_x11_typed_editing_key(&typed);

    if (editing_key != 0) {
        rc_x11_send(desc, (rc_app_event) {.type = RC_APP_EVENT_TYPE_CHAR, .modifiers = modifiers, .codepoint = editing_key});
        return;
    }

    if ((modifiers & (RC_MODIFIER_CTRL | RC_MODIFIER_SUPER)) != 0) {
        return;
    }

    if (typed.status != XLookupChars && typed.status != XLookupBoth) {
        return;
    }

    rc_x11_send_text(&typed, w->ic != NULL, modifiers, desc);
}


// Send a key event for xkey.
static void rc_x11_send_key(rc_x11_window *w, const XKeyEvent *xkey, uint8_t type, uint8_t modifiers, const rc_app_desc *desc)
{
    w->modifiers = modifiers;
    rc_x11_send(desc, (rc_app_event) {
        .type = type,
        .modifiers = modifiers,
        .scancode = (uint16_t)rc_x11_scancode(xkey->keycode),
        .key = rc_x11_keycode(w, xkey)
    });
}


// A key press: a key down if the key was up and a key repeat if it was already down, then what it
// types. filtered says the input method kept the press for itself (a dead key, say), so it types
// nothing yet.
static void rc_x11_handle_key_press(rc_x11_window *w, XKeyEvent *xkey, bool filtered, const rc_app_desc *desc)
{
    uint32_t keycode = xkey->keycode;
    bool repeat = keycode < RC_X11_KEYCODES && w->key_down[keycode];
    uint8_t modifiers = rc_x11_modifiers_after(w, xkey, true);

    if (keycode < RC_X11_KEYCODES) {
        w->key_down[keycode] = true;
    }

    rc_x11_send_key(w, xkey, repeat ? RC_APP_EVENT_TYPE_KEY_REPEAT : RC_APP_EVENT_TYPE_KEY_DOWN, modifiers, desc);
    if (!filtered) {
        rc_x11_send_typing(w, xkey, modifiers, desc);
    }
}


// True if a key release is half of a repeat: without detectable auto-repeat, each repeat is a
// release then a press of the same key at the same time.
static bool rc_x11_release_is_repeat(const rc_x11_window *w, const XKeyEvent *xkey)
{
    XEvent next = {0};

    if (XEventsQueued(w->display, QueuedAfterReading) <= 0) {
        return false;
    }

    XPeekEvent(w->display, &next);
    if (next.type != KeyPress) {
        return false;
    }

    return next.xkey.keycode == xkey->keycode && next.xkey.time == xkey->time;
}


// A key release: a key up if the key was down.
static void rc_x11_handle_key_release(rc_x11_window *w, const XKeyEvent *xkey, const rc_app_desc *desc)
{
    uint32_t keycode = xkey->keycode;

    // A repeat's release is dropped, and its press counts as a repeat: the key is still down
    if (rc_x11_release_is_repeat(w, xkey)) {
        return;
    }

    if (keycode < RC_X11_KEYCODES && w->key_down[keycode]) {
        w->key_down[keycode] = false;
        rc_x11_send_key(w, xkey, RC_APP_EVENT_TYPE_KEY_UP, rc_x11_modifiers_after(w, xkey, false), desc);
    }
}


// Send the key up of a key already marked up, which we will not see released. With no X event to
// read the state from, the locks stay as they were, and the held modifiers are the keys still down.
static void rc_x11_send_lost_key_up(rc_x11_window *w, uint32_t keycode, const rc_app_desc *desc)
{
    XKeyEvent xkey = {.display = w->display, .keycode = keycode, .state = 0};
    uint8_t locks = w->modifiers & (RC_MODIFIER_CAPS_LOCK | RC_MODIFIER_NUM_LOCK);
    uint8_t modifiers = (uint8_t)(locks | rc_x11_modifiers_held(w));

    rc_x11_send_key(w, &xkey, RC_APP_EVENT_TYPE_KEY_UP, modifiers, desc);
}


// Losing the keyboard: every key still down gets its key up, since we will not see it released.
static void rc_x11_release_all_keys(rc_x11_window *w, const rc_app_desc *desc)
{
    for (uint32_t keycode = 0; keycode < RC_X11_KEYCODES; keycode++) {
        if (!w->key_down[keycode]) {
            continue;
        }

        w->key_down[keycode] = false;
        rc_x11_send_lost_key_up(w, keycode, desc);
    }
}


// Handle an event. filtered says the input method kept it for itself.
static void rc_x11_handle_event(rc_x11_window *w, XEvent *xevent, bool filtered, const rc_app_desc *desc)
{
    switch (xevent->type) {
        case KeyPress:
            rc_x11_handle_key_press(w, &xevent->xkey, filtered, desc);
            break;

        case KeyRelease:
            rc_x11_handle_key_release(w, &xevent->xkey, desc);
            break;

        case MapNotify:
            w->mapped = true;
            break;

        case UnmapNotify:
            w->mapped = false;
            break;

        case FocusIn:
            if (w->ic) {
                XSetICFocus(w->ic);
            }

            break;

        case FocusOut:
            if (w->ic) {
                XUnsetICFocus(w->ic);
            }

            rc_x11_release_all_keys(w, desc);
            break;

        case MappingNotify:
            // The keyboard's mapping changed: Xlib's copy, and where the modifiers are
            XRefreshKeyboardMapping(&xevent->xmapping);
            rc_x11_read_modifier_masks(w);
            break;

        case ClientMessage:
            if ((Atom)xevent->xclient.data.l[0] == w->wm_delete_window) {
                w->close_requested = true;
            }

            break;

        default:
            break;
    }
}


// Take the next event off the queue, waiting for one if there is none.
static XEvent rc_x11_next_event(Display *display)
{
    XEvent xevent = {0};

    XNextEvent(display, &xevent);
    return xevent;
}


// Handle every event already queued, without waiting for more. The input method sees every event
// first, and may keep some for itself.
static void rc_x11_handle_events(rc_x11_window *w, const rc_app_desc *desc)
{
    while (XPending(w->display)) {
        XEvent xevent = rc_x11_next_event(w->display);
        bool filtered = XFilterEvent(&xevent, None);

        rc_x11_handle_event(w, &xevent, filtered, desc);
    }
}


// Sleep until an event comes, leaving it queued.
static void rc_x11_wait_for_event(Display *display)
{
    XEvent next = {0};

    XPeekEvent(display, &next);
}


// Run the frame loop until the window is closed.
static void rc_x11_run_frames(rc_x11_window *w, const rc_app_desc *desc)
{
    while (true) {
        rc_x11_handle_events(w, desc);
        if (w->close_requested) {
            break;
        }

        // Unmapped, nothing is drawn, so we sleep until an event comes
        if (!w->mapped) {
            rc_x11_wait_for_event(w->display);
            continue;
        }

        // Presenting waits for the display, which paces the loop
        if (desc->frame) {
            desc->frame(desc->user);
        }
    }
}


const rc_x11_window *rc_x11_app_window(void)
{
    return rc_x11_open_window;
}


int32_t rc_app_run(const rc_app_desc *desc)
{
    rc_x11_window w = {0};

    RC_ASSERT(desc);
    if (!rc_x11_open(&w, desc)) {
        rc_print_err(RC_STR("rc_app_run: cannot open the X display\n"));
        return 1;
    }

    rc_x11_open_window = &w;
    if (desc->init) {
        desc->init(desc->user);
    }

    rc_x11_run_frames(&w, desc);
    if (desc->deinit) {
        desc->deinit(desc->user);
    }

    rc_x11_open_window = NULL;
    rc_x11_close(&w);
    return 0;
}
