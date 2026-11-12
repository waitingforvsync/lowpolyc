// gl.h
//
// Private to richc_app: the OpenGL ES 3.0 types, constants and functions the GL backend uses,
// declared by hand so we need no GL headers or loader library. Each is added when the backend
// first needs it.
//
// Every function is called through rc_<name> (rc_glClear for glClear). The prefix keeps our names
// apart from any glClear the GL driver exports. Natively each is a pointer that rc_gl_load fills
// from the context.

#ifndef RC_APP_GL_H_
#define RC_APP_GL_H_

#include "gl_context.h"

// ---- types ----

typedef uint32_t GLenum;
typedef uint32_t GLuint;
typedef int32_t GLint;
typedef int32_t GLsizei;
typedef uint8_t GLboolean;
typedef float GLfloat;

// ---- constants ----

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NO_ERROR 0

#define GL_SCISSOR_TEST 0x0C11
#define GL_COLOR 0x1800
#define GL_DRAW_FRAMEBUFFER 0x8CA9

// ---- functions ----

// Each function as X(return type, name, parameters).
#define RC_GL_FUNCTIONS(X) \
    X(void, glBindFramebuffer, (GLenum target, GLuint framebuffer)) \
    X(void, glClearBufferfv, (GLenum buffer, GLint drawbuffer, const GLfloat *value)) \
    X(void, glColorMask, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)) \
    X(void, glDisable, (GLenum cap)) \
    X(GLenum, glGetError, (void)) \
    X(void, glInvalidateFramebuffer, (GLenum target, GLsizei num_attachments, const GLenum *attachments))

#define RC_GL_DECLARE_(ret, name, params) extern ret (*rc_##name) params;
RC_GL_FUNCTIONS(RC_GL_DECLARE_)
#undef RC_GL_DECLARE_

// Look up every function in context. Returns the name of the first one it lacks, or NULL once
// they are all found.
const char *rc_gl_load(const rc_gl_context *context);

#endif // RC_APP_GL_H_
