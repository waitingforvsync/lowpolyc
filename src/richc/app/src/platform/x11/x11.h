// x11.h
//
// Private to richc_app's X11 platform: the window rc_app_run opens, for the code that draws to it.

#ifndef RC_APP_X11_H_
#define RC_APP_X11_H_

#include <X11/Xlib.h>

#include "richc/macros.h"

enum {
    // X keycodes run from 8 to 255
    RC_X11_KEYCODES = 256
};

typedef struct rc_x11_window {
    Display *display;
    Window window;
    Atom wm_delete_window;
    bool close_requested;

    // Whether the window is on screen. Presenting an unmapped window may not wait for the display,
    // so the loop draws nothing until it is mapped.
    bool mapped;

    // Which keys are down, by keycode, so a held key's repeats are not taken for new presses
    bool key_down[RC_X11_KEYCODES];
} rc_x11_window;

// The window rc_app_run has open, or NULL outside it.
const rc_x11_window *rc_x11_app_window(void);

#endif // RC_APP_X11_H_
