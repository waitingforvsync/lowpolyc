// app.h
//
// The application: one window (a canvas, on wasm), its input, and the frame loop.
//
// The host owns the loop. rc_app_run takes an rc_app_desc holding the window settings and the
// callbacks. Natively it runs the loop and returns once the window closes; on wasm it returns at
// once, and the page calls back in for each frame and event. Either way the app's state has to
// outlive the call to rc_app_run, so it lives in static storage or an arena, never on the stack.

#ifndef RC_APP_APP_H_
#define RC_APP_APP_H_

#include "richc/app/keys.h"
#include "richc/str.h"

// ---- input ----

typedef enum rc_app_event_type {
    RC_APP_EVENT_NONE,

    // A key went down or came up: once each per press, however long the key is held. For keys as
    // controls (by scancode) and hotkeys (by keycode and modifiers).
    RC_APP_EVENT_KEY_DOWN,
    RC_APP_EVENT_KEY_UP,

    // A held key, at the system's auto-repeat rate, between its down and its up. For the few
    // hotkeys that should repeat, such as undo; everything else can ignore it.
    RC_APP_EVENT_KEY_REPEAT,

    // Typing, for text fields: a character, after the layout, shift, caps lock, AltGr, dead keys
    // and compose have had their say; or an editing key (an arrow, home, end, page up or down,
    // insert, delete, backspace, tab, enter or escape), as its rc_key. Repeats while the key is
    // held. Ctrl or super with a character key is a hotkey rather than typing, so gives no char.
    RC_APP_EVENT_CHAR
} rc_app_event_type;

// Something that happened to the window, passed to the event callback.
typedef struct rc_app_event {
    uint8_t type;        // rc_app_event_type
    uint8_t modifiers;   // rc_modifier bits, for key and char events, including the event's own key
    uint16_t scancode;   // rc_scancode, for key events
    uint32_t key;        // rc_key, for key events
    uint32_t codepoint;  // for char events: the character, or the editing key's rc_key
} rc_app_event;

// ---- running ----

// The window and the callbacks. Zero fields mean the default, and any callback may be NULL.
typedef struct rc_app_desc {
    rc_str title;
    uint32_t width;   // 0: 1280; ignored on wasm, where the page sizes the canvas
    uint32_t height;  // 0: 720
    void *user;       // passed to every callback

    // Once, before the first frame
    void (*init)(void *user);

    // For each event, between frames
    void (*event)(void *user, const rc_app_event *event);

    // Once per displayed frame
    void (*frame)(void *user);

    // Once, after the last frame (natively; a page is simply closed)
    void (*deinit)(void *user);
} rc_app_desc;

// Open the window and run the app. Natively, returns 0 once the window closes, or non-zero if it
// could not be opened. On wasm, returns 0 at once. desc is copied, so it need not outlive the call.
int32_t rc_app_run(const rc_app_desc *desc);

#endif // RC_APP_APP_H_
