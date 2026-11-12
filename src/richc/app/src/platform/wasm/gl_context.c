// gl_context.c
//
// The GL context on wasm: gl.js holds the WebGL2 context on the page's canvas, made before the
// module starts, and its GL functions are imports. So there is little to do here. The context and
// the window are ids standing for gl.js's objects.

#include "gl_context.h"

#include "log.h"

// The id of gl.js's WebGL2 context, or 0 if the browser could not make one.
__attribute__((import_module("richc"), import_name("gl_context_id"))) uint32_t rc_wasm_gl_context_id(void);

enum {
    // The canvas, the one window a page has
    RC_WASM_CANVAS_ID = 1
};

rc_gl_context rc_gl_context_make(void)
{
    uint32_t id = rc_wasm_gl_context_id();

    if (id == 0) {
        rc_app_log(RC_STR("rc_gl_context_make: the browser has no WebGL2"));
        return (rc_gl_context) {0};
    }
    return (rc_gl_context) {.context_ = (void *)(uintptr_t)id};
}


void rc_gl_context_release(rc_gl_context *context)
{
    // The context belongs to the page, which keeps it
    *context = (rc_gl_context) {0};
}


rc_gl_proc rc_gl_context_proc(const rc_gl_context *context, const char *name)
{
    // GL functions are imports on wasm, so nothing looks them up
    (void)context;
    (void)name;
    return NULL;
}


rc_gl_window rc_gl_window_make(const rc_gl_context *context)
{
    RC_ASSERT(context->context_);
    return (rc_gl_window) {.surface_ = (void *)(uintptr_t)RC_WASM_CANVAS_ID};
}


void rc_gl_window_release(rc_gl_window *window, const rc_gl_context *context)
{
    (void)context;
    *window = (rc_gl_window) {0};
}


void rc_gl_window_present(rc_gl_window window, const rc_gl_context *context)
{
    // The browser shows the canvas when the animation frame callback returns, at the display's
    // rate, so there is nothing to do or wait for
    (void)context;
    RC_ASSERT(window.surface_);
}
