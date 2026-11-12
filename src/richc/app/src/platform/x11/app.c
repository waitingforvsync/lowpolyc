// app.c
//
// X11, through Xlib: one window, its events, and the native frame loop.

#include "richc/app/app.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdio.h>

#include "log.h"
#include "platform/x11/x11.h"
#include "richc/mem.h"

enum {
    RC_X11_DEFAULT_WIDTH = 1280,
    RC_X11_DEFAULT_HEIGHT = 720
};

// The open window, while rc_app_run is running.
static const rc_x11_window *rc_x11_open_window;


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

            case MapNotify:
                w->mapped = true;
                break;

            case UnmapNotify:
                w->mapped = false;
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
