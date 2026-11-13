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
#include <stdio.h>

#include "keys.h"
#include "log.h"
#include "platform/x11/x11.h"

enum {
    RC_X11_DEFAULT_WIDTH = 1280,
    RC_X11_DEFAULT_HEIGHT = 720
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
    if (keycode < RC_X11_EVDEV_OFFSET || keycode - RC_X11_EVDEV_OFFSET >= RC_X11_EVDEV_CODES) {
        return RC_SCANCODE_UNKNOWN;
    }
    return (rc_scancode)rc_x11_scancodes[keycode - RC_X11_EVDEV_OFFSET];
}


// The code point at the start of text, which holds len (non-zero) bytes, setting *size to the bytes
// it takes. Malformed UTF-8 gives U+FFFD for its first byte.
static uint32_t rc_x11_decode_utf8(const unsigned char *text, uint32_t len, uint32_t *size)
{
    uint32_t lead = text[0];
    uint32_t extra = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : 0;
    uint32_t codepoint = extra == 3 ? lead & 0x07 : extra == 2 ? lead & 0x0F : lead & 0x1F;

    *size = 1;
    if (extra == 0) {
        return lead < 0x80 ? lead : 0xFFFD;
    }
    if (lead >= 0xF8 || extra >= len) {
        return 0xFFFD;
    }
    for (uint32_t i = 1; i <= extra; i++) {
        if ((text[i] & 0xC0) != 0x80) {
            return 0xFFFD;
        }
        codepoint = (codepoint << 6) | (text[i] & 0x3F);
    }
    *size = extra + 1;
    return codepoint;
}


// The character a keysym types, or 0 if it is not a character, or is one of the legacy non-Latin
// keysyms without a Unicode form (which keycodes replace with the US character anyway).
static uint32_t rc_x11_keysym_char(KeySym keysym)
{
    if ((keysym >= 0x20 && keysym <= 0x7E) || (keysym >= 0xA0 && keysym <= 0xFF)) {
        return (uint32_t)keysym;
    }
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
        uint32_t mask = 1u << row;

        for (int i = 0; i < map->max_keypermod; i++) {
            KeyCode keycode = map->modifiermap[row * map->max_keypermod + i];
            KeySym keysym = keycode ? XkbKeycodeToKeysym(w->display, keycode, 0, 0) : NoSymbol;

            if (keysym == XK_Alt_L || keysym == XK_Alt_R || keysym == XK_Meta_L || keysym == XK_Meta_R) {
                w->alt_mask |= mask;
            }
            else if (keysym == XK_Super_L || keysym == XK_Super_R) {
                w->super_mask |= mask;
            }
            else if (keysym == XK_Num_Lock) {
                w->num_lock_mask |= mask;
            }
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


// The modifiers after a key event. X gives the state from before it, so a modifier key's own press
// or release is applied here.
static uint8_t rc_x11_modifiers_after(const rc_x11_window *w, const XKeyEvent *xkey, bool down)
{
    uint8_t modifiers = rc_x11_modifiers(w, xkey->state);
    rc_scancode scancode = rc_x11_scancode(xkey->keycode);
    uint8_t held = rc_x11_modifier_held_by(scancode);

    if (held != 0) {
        // Released, it still counts if the key on the other side is down
        modifiers = down ? modifiers | held : (uint8_t)((modifiers & ~held) | (rc_x11_modifiers_held(w) & held));
    }

    // A lock key toggles its lock on its press, as on every other platform. (X turns a lock off on
    // the second release instead, so that release still reports it on: it keeps the press's.)
    uint8_t lock = scancode == RC_SCANCODE_CAPS_LOCK ? RC_MODIFIER_CAPS_LOCK
        : scancode == RC_SCANCODE_NUM_LOCK ? RC_MODIFIER_NUM_LOCK
        : 0;
    if (lock != 0 && down) {
        modifiers ^= lock;
    }
    if (lock != 0 && !down) {
        modifiers = (uint8_t)((modifiers & ~lock) | (w->modifiers & lock));
    }
    return modifiers;
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


// Open the display and show the window. False if there is no display to open.
static bool rc_x11_open(rc_x11_window *w, const rc_app_desc *desc)
{
    w->display = XOpenDisplay(NULL);
    if (!w->display) {
        return false;
    }

    int screen = DefaultScreen(w->display);
    uint32_t width = desc->width ? desc->width : RC_X11_DEFAULT_WIDTH;
    uint32_t height = desc->height ? desc->height : RC_X11_DEFAULT_HEIGHT;
    XSetWindowAttributes attributes = {
        .background_pixel = BlackPixel(w->display, screen),
        .event_mask = KeyPressMask | KeyReleaseMask | FocusChangeMask | StructureNotifyMask
    };

    w->window = XCreateWindow(w->display, RootWindow(w->display, screen), 0, 0, width, height, 0, CopyFromParent,
                              InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &attributes);
    if (!rc_str_is_empty(desc->title)) {
        rc_x11_set_title(w, desc->title);
    }

    // Ask to be told when the close button is pressed, rather than have the connection cut
    w->wm_delete_window = XInternAtom(w->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(w->display, w->window, &w->wm_delete_window, 1);

    // A held key then repeats as presses alone, with no releases in between. Where the server
    // cannot do that, rc_x11_handle_key_release spots the pairs instead.
    XkbSetDetectableAutoRepeat(w->display, True, NULL);

    // Text comes through an input method, which applies the layout, dead keys and compose. It works
    // in the locale's character set, and a program that never chose one runs in "C", so we take the
    // user's. Without an input method, text falls back to Latin-1.
    if (rc_str_is_equal(rc_str_from_cstr(setlocale(LC_CTYPE, NULL)), RC_STR("C"))) {
        setlocale(LC_CTYPE, "");
    }
    XSetLocaleModifiers("");
    w->im = XOpenIM(w->display, NULL, NULL, NULL);
    if (w->im) {
        w->ic = XCreateIC(w->im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, w->window,
                          XNFocusWindow, w->window, NULL);
    }
    rc_x11_read_modifier_masks(w);
    if (w->ic) {
        // The input method may need events of its own
        long im_events = 0;

        XGetICValues(w->ic, XNFilterEvents, &im_events, NULL);
        XSelectInput(w->display, w->window, attributes.event_mask | im_events);
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


// Send the char events a key press types: an editing key, or text. Text with ctrl or super held is
// a hotkey, not typing, so gives none.
static void rc_x11_send_typing(rc_x11_window *w, XKeyEvent *xkey, uint8_t modifiers, const rc_app_desc *desc)
{
    char text[64];
    KeySym keysym = NoSymbol;
    Status status = XLookupNone;
    int len = 0;

    if (w->ic) {
        len = Xutf8LookupString(w->ic, xkey, text, (int)sizeof text, &keysym, &status);
    }
    else {
        // No input method: Latin-1 text, whose bytes are their own code points
        len = XLookupString(xkey, text, (int)sizeof text, &keysym, NULL);
        status = len > 0 ? XLookupBoth : XLookupKeySym;
    }

    bool has_keysym = status == XLookupKeySym || status == XLookupBoth;
    uint32_t editing_key = has_keysym ? rc_x11_editing_key(keysym) : 0;
    if (editing_key != 0) {
        rc_x11_send(desc, (rc_app_event) {.type = RC_APP_EVENT_CHAR, .modifiers = modifiers, .codepoint = editing_key});
        return;
    }
    if ((modifiers & (RC_MODIFIER_CTRL | RC_MODIFIER_SUPER)) != 0) {
        return;
    }
    if (status != XLookupChars && status != XLookupBoth) {
        return;
    }

    // Compose can type more than one character at once
    const unsigned char *bytes = (const unsigned char *)text;
    uint32_t i = 0;
    while (i < (uint32_t)len) {
        uint32_t size = 1;
        uint32_t codepoint = w->ic ? rc_x11_decode_utf8(bytes + i, (uint32_t)len - i, &size) : bytes[i];

        if (rc_app_is_text(codepoint)) {
            rc_x11_send(desc, (rc_app_event) {.type = RC_APP_EVENT_CHAR, .modifiers = modifiers, .codepoint = codepoint});
        }
        i += size;
    }
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

    if (keycode < RC_X11_KEYCODES) {
        w->key_down[keycode] = true;
    }

    uint8_t modifiers = rc_x11_modifiers_after(w, xkey, true);
    rc_x11_send_key(w, xkey, repeat ? RC_APP_EVENT_KEY_REPEAT : RC_APP_EVENT_KEY_DOWN, modifiers, desc);
    if (!filtered) {
        rc_x11_send_typing(w, xkey, modifiers, desc);
    }
}


// A key release: a key up if the key was down.
static void rc_x11_handle_key_release(rc_x11_window *w, const XKeyEvent *xkey, const rc_app_desc *desc)
{
    uint32_t keycode = xkey->keycode;

    // Without detectable auto-repeat, each repeat is a release then a press at the same time.
    // The release is dropped, and the press counts as a repeat, since the key is still down.
    if (XEventsQueued(w->display, QueuedAfterReading) > 0) {
        XEvent next;

        XPeekEvent(w->display, &next);
        if (next.type == KeyPress && next.xkey.keycode == keycode && next.xkey.time == xkey->time) {
            return;
        }
    }

    if (keycode < RC_X11_KEYCODES && w->key_down[keycode]) {
        w->key_down[keycode] = false;
        rc_x11_send_key(w, xkey, RC_APP_EVENT_KEY_UP, rc_x11_modifiers_after(w, xkey, false), desc);
    }
}


// Losing the keyboard: every key still down gets its key up, since we will not see it released.
static void rc_x11_release_all_keys(rc_x11_window *w, const rc_app_desc *desc)
{
    for (uint32_t keycode = 0; keycode < RC_X11_KEYCODES; keycode++) {
        if (!w->key_down[keycode]) {
            continue;
        }
        w->key_down[keycode] = false;

        // No X event to read the state from: the locks stay as they were, and the held modifiers
        // are the keys still down
        XKeyEvent xkey = {.display = w->display, .keycode = keycode, .state = 0};
        uint8_t locks = w->modifiers & (RC_MODIFIER_CAPS_LOCK | RC_MODIFIER_NUM_LOCK);
        uint8_t modifiers = (uint8_t)(locks | rc_x11_modifiers_held(w));

        rc_x11_send_key(w, &xkey, RC_APP_EVENT_KEY_UP, modifiers, desc);
    }
}


// Handle every event already queued, without waiting for more.
static void rc_x11_handle_events(rc_x11_window *w, const rc_app_desc *desc)
{
    while (XPending(w->display)) {
        XEvent xevent;

        XNextEvent(w->display, &xevent);

        // The input method sees every event first, and may keep some for itself
        bool filtered = XFilterEvent(&xevent, None);
        switch (xevent.type) {
            case KeyPress:
                rc_x11_handle_key_press(w, &xevent.xkey, filtered, desc);
                break;

            case KeyRelease:
                rc_x11_handle_key_release(w, &xevent.xkey, desc);
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
                XRefreshKeyboardMapping(&xevent.xmapping);
                rc_x11_read_modifier_masks(w);
                break;

            case ClientMessage:
                if ((Atom)xevent.xclient.data.l[0] == w->wm_delete_window) {
                    w->close_requested = true;
                }
                break;

            default:
                break;
        }
    }
}


const rc_x11_window *rc_x11_app_window(void)
{
    return rc_x11_open_window;
}


void rc_app_log(rc_str message)
{
    fwrite(message.data, 1, message.len, stderr);
    fputc('\n', stderr);
}


int32_t rc_app_run(const rc_app_desc *desc)
{
    rc_x11_window w = {0};

    RC_ASSERT(desc);
    if (!rc_x11_open(&w, desc)) {
        rc_app_log(RC_STR("rc_app_run: cannot open the X display"));
        return 1;
    }

    rc_x11_open_window = &w;
    if (desc->init) {
        desc->init(desc->user);
    }
    while (true) {
        rc_x11_handle_events(&w, desc);
        if (w.close_requested) {
            break;
        }

        // Unmapped, nothing is drawn, so we sleep until an event comes
        if (!w.mapped) {
            XEvent next;

            XPeekEvent(w.display, &next);
            continue;
        }

        // Presenting waits for the display, which paces the loop
        if (desc->frame) {
            desc->frame(desc->user);
        }
    }
    if (desc->deinit) {
        desc->deinit(desc->user);
    }

    rc_x11_open_window = NULL;
    rc_x11_close(&w);
    return 0;
}
