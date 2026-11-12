// gl_context.c
//
// The GL context on X11, through EGL: an OpenGL ES 3.0 context, and a surface on the app window.

#include "gl_context.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>

#include "platform/x11/x11.h"

enum {
    // How many of EGL's matching configs we look through for one that suits the window
    RC_EGL_MAX_CONFIGS = 64
};

// Report a failed EGL call, with EGL's error code.
static void rc_egl_report(const char *what)
{
    fprintf(stderr, "%s (EGL error 0x%04x)\n", what, (unsigned)eglGetError());
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
    EGLConfig configs[RC_EGL_MAX_CONFIGS];
    EGLint count = 0;

    if (!eglChooseConfig(display, attributes, configs, RC_EGL_MAX_CONFIGS, &count) || count == 0) {
        return NULL;
    }

    XWindowAttributes window_attributes;
    XGetWindowAttributes(w->display, w->window, &window_attributes);

    VisualID visual = XVisualIDFromVisual(window_attributes.visual);
    for (EGLint i = 0; i < count; i++) {
        EGLint config_visual = 0;

        if (eglGetConfigAttrib(display, configs[i], EGL_NATIVE_VISUAL_ID, &config_visual)
            && (VisualID)config_visual == visual) {
            return configs[i];
        }
    }
    return configs[0];
}


rc_gl_context rc_gl_context_make(void)
{
    const rc_x11_window *w = rc_x11_app_window();
    RC_ASSERT(w);

    EGLDisplay display = rc_egl_display(w->display);
    if (display == EGL_NO_DISPLAY) {
        rc_egl_report("rc_gl_context_make: EGL has no display for X11");
        return (rc_gl_context) {0};
    }
    if (!eglInitialize(display, NULL, NULL)) {
        rc_egl_report("rc_gl_context_make: cannot initialise EGL");
        return (rc_gl_context) {0};
    }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        rc_egl_report("rc_gl_context_make: EGL has no OpenGL ES");
        eglTerminate(display);
        return (rc_gl_context) {0};
    }

    EGLConfig config = rc_egl_config(display, w);
    if (!config) {
        rc_egl_report("rc_gl_context_make: EGL has no config for an OpenGL ES 3 window");
        eglTerminate(display);
        return (rc_gl_context) {0};
    }

    const EGLint attributes[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 0,
        EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, attributes);
    if (context == EGL_NO_CONTEXT) {
        rc_egl_report("rc_gl_context_make: cannot make an OpenGL ES 3.0 context");
        eglTerminate(display);
        return (rc_gl_context) {0};
    }

    // Current with no surface (EGL_KHR_surfaceless_context), so GL works before there is a window
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


rc_gl_window rc_gl_window_make(const rc_gl_context *context)
{
    const rc_x11_window *w = rc_x11_app_window();
    RC_ASSERT(w && context->context_);

    EGLSurface surface = eglCreateWindowSurface(context->display_, context->config_, (EGLNativeWindowType)w->window, NULL);
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
