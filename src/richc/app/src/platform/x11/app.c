// app.c
//
// X11, through Xlib: one window, its events, and the native frame loop.

#include "richc/app/app.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <poll.h>
#include <stdio.h>

#include "richc/mem.h"

enum {
    RC_X11_DEFAULT_WIDTH = 1280,
    RC_X11_DEFAULT_HEIGHT = 720,

    // X keycodes run from 8 to 255
    RC_X11_KEYCODES = 256,

    // How long the loop waits for an event before the next frame
    RC_X11_FRAME_WAIT_MS = 16
};

typedef struct rc_x11_window {
    Display *display;
    Window window;
    Atom wm_delete_window;
    bool close_requested;

    // Which keys are down, by keycode, so a held key's repeats are not taken for new presses
    bool key_down[RC_X11_KEYCODES];
} rc_x11_window;

// The key at an X keycode. Every current X server (Xorg with evdev or libinput, and XWayland) uses
// the Linux evdev code plus 8, which names a physical position whatever the layout.
static rc_key rc_x11_key(uint32_t keycode)
{
    switch (keycode) {
        case 65:
            return RC_KEY_SPACE;
        default:
            return RC_KEY_UNKNOWN;
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
        .event_mask = KeyPressMask | KeyReleaseMask | FocusChangeMask
    };

    w->window = XCreateWindow(w->display, RootWindow(w->display, screen), 0, 0, width, height, 0, CopyFromParent,
                              InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &attributes);
    if (!rc_str_is_empty(desc->title)) {
        rc_x11_set_title(w, desc->title);
    }

    // Ask to be told when the close button is pressed, rather than have the connection cut
    w->wm_delete_window = XInternAtom(w->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(w->display, w->window, &w->wm_delete_window, 1);

    // A held key then repeats as presses alone, with no releases in between
    XkbSetDetectableAutoRepeat(w->display, True, NULL);

    XMapWindow(w->display, w->window);
    XFlush(w->display);
    return true;
}


// Destroy the window and close the display.
static void rc_x11_close(rc_x11_window *w)
{
    XDestroyWindow(w->display, w->window);
    XCloseDisplay(w->display);
}


// A key press or release. Repeats of a held key are dropped.
static void rc_x11_handle_key(rc_x11_window *w, const XKeyEvent *xkey, const rc_app_desc *desc)
{
    uint32_t keycode = xkey->keycode;
    bool down = xkey->type == KeyPress;

    if (keycode >= RC_X11_KEYCODES || (down && w->key_down[keycode])) {
        return;
    }
    w->key_down[keycode] = down;
    if (desc->event) {
        rc_app_event event = {
            .type = down ? RC_APP_EVENT_KEY_DOWN : RC_APP_EVENT_KEY_UP,
            .key = (uint16_t)rc_x11_key(keycode)
        };

        desc->event(desc->user, &event);
    }
}


// Handle every event already queued, without waiting for more.
static void rc_x11_handle_events(rc_x11_window *w, const rc_app_desc *desc)
{
    while (XPending(w->display)) {
        XEvent xevent;

        XNextEvent(w->display, &xevent);
        switch (xevent.type) {
            case KeyPress:
            case KeyRelease:
                rc_x11_handle_key(w, &xevent.xkey, desc);
                break;

            case FocusOut:
                // Keys released while we cannot see them would otherwise stay down
                rc_memset(w->key_down, 0, sizeof w->key_down);
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


// Wait until an event arrives, or for at most timeout_ms.
static void rc_x11_wait(rc_x11_window *w, int timeout_ms)
{
    struct pollfd connection = {.fd = ConnectionNumber(w->display), .events = POLLIN};

    if (!XPending(w->display)) {
        poll(&connection, 1, timeout_ms);
    }
}


int32_t rc_app_run(const rc_app_desc *desc)
{
    rc_x11_window w = {0};

    RC_ASSERT(desc);
    if (!rc_x11_open(&w, desc)) {
        fprintf(stderr, "rc_app_run: cannot open the X display\n");
        return 1;
    }

    if (desc->init) {
        desc->init(desc->user);
    }
    while (true) {
        rc_x11_handle_events(&w, desc);
        if (w.close_requested) {
            break;
        }
        if (desc->frame) {
            desc->frame(desc->user);
        }

        // Nothing paces the frames yet, so we wait for input rather than spin
        rc_x11_wait(&w, RC_X11_FRAME_WAIT_MS);
    }
    if (desc->deinit) {
        desc->deinit(desc->user);
    }

    rc_x11_close(&w);
    return 0;
}
