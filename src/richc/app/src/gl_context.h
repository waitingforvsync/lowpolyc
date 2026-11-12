// gl_context.h
//
// Private to richc_app: an OpenGL ES 3.0 context and the app window it draws to, for the GL
// backend. Each platform implements it: EGL on X11, and WebGL2 on wasm.
//
// The backend holds the context in its device and the window in its surface, and passes them back
// in. The fields are the platform's own handles, and {0} is none. Everything that can fail
// reports why through rc_app_log.

#ifndef RC_APP_GL_CONTEXT_H_
#define RC_APP_GL_CONTEXT_H_

#include "richc/macros.h"

// A GL function, as the platform looks it up; cast to the real type before calling it.
typedef void (*rc_gl_proc)(void);

typedef struct rc_gl_context {
    void *display_;  // EGLDisplay; unused on wasm
    void *config_;   // EGLConfig; unused on wasm
    void *context_;  // EGLContext, or on wasm the id of gl.js's WebGL2 context
} rc_gl_context;

// The app window, as something the context can draw to and present.
typedef struct rc_gl_window {
    void *surface_;  // EGLSurface, or on wasm the id of the canvas
} rc_gl_window;

// A context on the app window's display, current with nothing to draw to yet. None if it cannot
// be made.
rc_gl_context rc_gl_context_make(void);

// Destroy the context and zero it. Any window made from it must already be released.
void rc_gl_context_release(rc_gl_context *context);

// The GL function called name, or NULL if the context has none.
rc_gl_proc rc_gl_context_proc(const rc_gl_context *context, const char *name);

// The app window, made current so drawing goes to it. None if it cannot be made.
rc_gl_window rc_gl_window_make(const rc_gl_context *context);

// Stop drawing to the window and zero it.
void rc_gl_window_release(rc_gl_window *window, const rc_gl_context *context);

// Show what has been drawn, waiting for the display.
void rc_gl_window_present(rc_gl_window window, const rc_gl_context *context);

#endif // RC_APP_GL_CONTEXT_H_
