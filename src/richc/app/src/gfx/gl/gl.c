// gl.c
//
// The GL function pointers, filled from the context by rc_gl_load. Native only: on wasm each
// function is an import instead.

#include "gfx/gl/gl.h"

#define RC_GL_DEFINE_(ret, name, params) ret (*rc_##name) params;
RC_GL_FUNCTIONS(RC_GL_DEFINE_)
#undef RC_GL_DEFINE_


const char *rc_gl_load(const rc_gl_context *context)
{
#define RC_GL_LOAD_(ret, name, params) \
    rc_##name = (ret (*) params)rc_gl_context_proc(context, #name); \
    if (!rc_##name) { \
        return #name; \
    }

    RC_GL_FUNCTIONS(RC_GL_LOAD_)

#undef RC_GL_LOAD_
    return NULL;
}
