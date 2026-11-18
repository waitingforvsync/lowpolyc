// gl_context.c
//
// The GL context on X11, through EGL: an OpenGL ES 3.0 context, and a surface on the app window.

#include "gl_context.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "platform/x11/x11.h"
#include "richc/print.h"

enum {
    // How many of EGL's matching configs we look through for one that suits the window
    RC_EGL_MAX_CONFIGS = 64
};

// The hex digit of value at place, counting from 0 for the lowest.
static char rc_egl_hex_digit(uint32_t value, uint32_t place)
{
    return "0123456789abcdef"[(value >> (4 * place)) & 15u];
}


// Report a failed EGL call, with EGL's error code.
static void rc_egl_report(const char *what)
{
    uint32_t error = (uint32_t)eglGetError();
    char code[4] = {
        rc_egl_hex_digit(error, 3),
        rc_egl_hex_digit(error, 2),
        rc_egl_hex_digit(error, 1),
        rc_egl_hex_digit(error, 0)
    };

    rc_print_err(rc_str_from_cstr(what));
    rc_print_err(RC_STR(" (EGL error 0x"));
    rc_print_err(rc_str_from_ptr(code, 4));
    rc_print_err(RC_STR(")\n"));
}


// The EGL display for the X display. EGL 1.5 can be told it is X11; eglGetDisplay has to guess.
static EGLDisplay rc_egl_display(Display *x_display)
{
    EGLDisplay display = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, x_display, NULL);

    if (display == EGL_NO_DISPLAY) {
        display = eglGetDisplay((EGLNativeDisplayType)x_display);
    }

    return display;
}


// The EGL display for the X display, initialised for OpenGL ES, or EGL_NO_DISPLAY (reported) if
// it can't be.
static EGLDisplay rc_egl_display_open(Display *x_display)
{
    EGLDisplay display = rc_egl_display(x_display);

    if (display == EGL_NO_DISPLAY) {
        rc_egl_report("rc_gl_context_make: EGL has no display for X11");
        return EGL_NO_DISPLAY;
    }

    if (!eglInitialize(display, NULL, NULL)) {
        rc_egl_report("rc_gl_context_make: cannot initialise EGL");
        return EGL_NO_DISPLAY;
    }

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        rc_egl_report("rc_gl_context_make: EGL has no OpenGL ES");
        eglTerminate(display);
        return EGL_NO_DISPLAY;
    }

    return display;
}


// True if config draws with the X visual whose id is visual.
static bool rc_egl_config_has_visual(EGLDisplay display, EGLConfig config, VisualID visual)
{
    EGLint config_visual = 0;

    if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &config_visual)) {
        return false;
    }

    return (VisualID)config_visual == visual;
}


// The id of the X visual the window was made with.
static VisualID rc_egl_window_visual(const rc_x11_window *w)
{
    XWindowAttributes attributes = {0};

    XGetWindowAttributes(w->display, w->window, &attributes);
    return XVisualIDFromVisual(attributes.visual);
}


// The first of count configs with the window's own visual, or the first of all if none has it.
static EGLConfig rc_egl_config_for_window(EGLDisplay display, const EGLConfig *configs, EGLint count, const rc_x11_window *w)
{
    VisualID visual = rc_egl_window_visual(w);

    for (EGLint i = 0; i < count; i++) {
        if (rc_egl_config_has_visual(display, configs[i], visual)) {
            return configs[i];
        }
    }

    return configs[0];
}


// An 8-bit RGB config for OpenGL ES 3 windows, preferring one with the window's own visual: EGL
// lists deeper configs first, and a config whose visual differs from the window's may not draw to
// it. NULL if there is none.
static EGLConfig rc_egl_config(EGLDisplay display, const rc_x11_window *w)
{
    const EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    EGLConfig configs[RC_EGL_MAX_CONFIGS] = {0};
    EGLint count = 0;

    if (!eglChooseConfig(display, attributes, configs, RC_EGL_MAX_CONFIGS, &count)) {
        return NULL;
    }

    if (count == 0) {
        return NULL;
    }

    return rc_egl_config_for_window(display, configs, count, w);
}


// A new OpenGL ES 3.0 context with config, or EGL_NO_CONTEXT.
static EGLContext rc_egl_create_context(EGLDisplay display, EGLConfig config)
{
    const EGLint attributes[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 0,
        EGL_NONE
    };

    return eglCreateContext(display, config, EGL_NO_CONTEXT, attributes);
}


// The made context, once it is current with no surface (EGL_KHR_surfaceless_context), so GL works
// before there is a window. None, with display terminated, if it can't be.
static rc_gl_context rc_egl_context_current(EGLDisplay display, EGLConfig config, EGLContext context)
{
    if (context == EGL_NO_CONTEXT) {
        rc_egl_report("rc_gl_context_make: cannot make an OpenGL ES 3.0 context");
        eglTerminate(display);
        return (rc_gl_context) {0};
    }

    if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) {
        rc_egl_report("rc_gl_context_make: cannot make the context current");
        eglDestroyContext(display, context);
        eglTerminate(display);
        return (rc_gl_context) {0};
    }

    return (rc_gl_context) {
        .display_ = display,
        .config_ = config,
        .context_ = context
    };
}


// A context on display with config. None, with display terminated, if there is no config.
static rc_gl_context rc_egl_context_with_config(EGLDisplay display, EGLConfig config)
{
    if (!config) {
        rc_egl_report("rc_gl_context_make: EGL has no config for an OpenGL ES 3 window");
        eglTerminate(display);
        return (rc_gl_context) {0};
    }

    return rc_egl_context_current(display, config, rc_egl_create_context(display, config));
}


// A context for the window on display; none if there is no display (that is already reported).
static rc_gl_context rc_egl_context_on(EGLDisplay display, const rc_x11_window *w)
{
    if (display == EGL_NO_DISPLAY) {
        return (rc_gl_context) {0};
    }

    return rc_egl_context_with_config(display, rc_egl_config(display, w));
}


rc_gl_context rc_gl_context_make(void)
{
    const rc_x11_window *w = rc_x11_app_window();

    RC_ASSERT(w);
    return rc_egl_context_on(rc_egl_display_open(w->display), w);
}


void rc_gl_context_release(rc_gl_context *context)
{
    if (context->context_) {
        eglMakeCurrent(context->display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(context->display_, context->context_);
        eglTerminate(context->display_);
    }

    *context = (rc_gl_context) {0};
}


rc_gl_proc rc_gl_context_proc(const rc_gl_context *context, const char *name)
{
    // EGL 1.5 looks up core functions as well as extensions, for whichever API is bound
    (void)context;
    return eglGetProcAddress(name);
}


// A new EGL surface on the window, or EGL_NO_SURFACE.
static EGLSurface rc_egl_create_window_surface(const rc_gl_context *context, const rc_x11_window *w)
{
    return eglCreateWindowSurface(context->display_, context->config_, (EGLNativeWindowType)w->window, NULL);
}


// The window, made current so drawing goes to surface. None if there is no surface, or it can't
// be made current.
static rc_gl_window rc_egl_window_current(const rc_gl_context *context, EGLSurface surface)
{
    if (surface == EGL_NO_SURFACE) {
        rc_egl_report("rc_gl_window_make: cannot make a surface on the window");
        return (rc_gl_window) {0};
    }

    if (!eglMakeCurrent(context->display_, surface, surface, context->context_)) {
        rc_egl_report("rc_gl_window_make: cannot draw to the window");
        eglDestroySurface(context->display_, surface);
        return (rc_gl_window) {0};
    }

    // Present once per refresh. If the driver ignores this, nothing paces the frames
    eglSwapInterval(context->display_, 1);
    return (rc_gl_window) {.surface_ = surface};
}


rc_gl_window rc_gl_window_make(const rc_gl_context *context)
{
    const rc_x11_window *w = rc_x11_app_window();

    RC_ASSERT(w && context->context_);
    return rc_egl_window_current(context, rc_egl_create_window_surface(context, w));
}


void rc_gl_window_release(rc_gl_window *window, const rc_gl_context *context)
{
    if (window->surface_) {
        eglMakeCurrent(context->display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context->context_);
        eglDestroySurface(context->display_, window->surface_);
    }

    *window = (rc_gl_window) {0};
}


void rc_gl_window_present(rc_gl_window window, const rc_gl_context *context)
{
    RC_ASSERT(window.surface_);
    if (!eglSwapBuffers(context->display_, window.surface_)) {
        rc_egl_report("rc_gl_window_present: cannot present");
    }
}
