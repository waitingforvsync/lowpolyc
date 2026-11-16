// wgsl.h
//
// Translating WGSL, WebGPU's shading language, into the shading languages of richc's other gfx
// backends, with the reflection the backends need to bind resources (richc/shader/shader.h).
//
// We take the subset of WGSL that richc's shaders use: vertex and fragment shaders, uniform and
// storage buffers, textures and samplers, and the built-in functions that go with them. The rest
// is refused with an error rather than translated wrongly, as is anything one of the targets can't
// express. Errors point into the source by byte offset, line and column.

#ifndef RC_SHADER_WGSL_H_
#define RC_SHADER_WGSL_H_

#include "richc/shader/shader.h"

typedef struct rc_mstr rc_mstr;

// What went wrong, and where. The zero value, with no message, is no error at all.
typedef struct rc_wgsl_error {
    rc_str message;
    uint32_t offset;  // bytes into the source
    uint32_t line;    // from 1
    uint32_t column;  // from 1, counted in bytes
} rc_wgsl_error;

// True if error holds one.
static inline bool rc_wgsl_error_is_set(rc_wgsl_error error)
{
    return rc_str_is_valid(error.message);
}

typedef struct rc_wgsl_translate_result {
    rc_shader_package package;
    rc_wgsl_error error;
} rc_wgsl_translate_result;

// The package for the WGSL module source, allocated from arena, or why there can't be one.
rc_wgsl_translate_result rc_wgsl_translate(rc_str source, rc_arena *arena);

// Append package as C: a header defining it as a static const rc_shader_package called name,
// which must be a C identifier.
void rc_shader_package_append_c(rc_mstr *out, rc_shader_package package, rc_str name,
    rc_arena *arena);

#endif // RC_SHADER_WGSL_H_
