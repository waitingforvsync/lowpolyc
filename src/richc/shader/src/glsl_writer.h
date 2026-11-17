// glsl_writer.h
//
// Private to the GLSL writer's own files (glsl*.c): the functions they share. The checker has
// found everything the entry point reaches, in the order GLSL needs it (rc_wgsl_entry), so the
// writer just writes it out, in the three parts of an rc_writer.

#ifndef RC_SHADER_GLSL_WRITER_H_
#define RC_SHADER_GLSL_WRITER_H_

#include "glsl.h"

// The polyfills: functions GLSL lacks, or has with different edge cases, written once per type.
typedef enum rc_glsl_polyfill {
    RC_GLSL_POLYFILL_NONE = 0,
    RC_GLSL_POLYFILL_FMOD,
    RC_GLSL_POLYFILL_DIV,
    RC_GLSL_POLYFILL_MOD,
    RC_GLSL_POLYFILL_F2I,
    RC_GLSL_POLYFILL_F2U,
    RC_GLSL_POLYFILL_AND,
    RC_GLSL_POLYFILL_OR,
    RC_GLSL_POLYFILL_SELECT,
    RC_GLSL_POLYFILL_SELECT_VECTOR,
    RC_GLSL_POLYFILL_DOT
} rc_glsl_polyfill;

// ---- glsl.c ----

// Append the name of the combined sampler pair, as rc_t0_1 or rc_t0_1_s0_2.
void rc_glsl_append_sampler_name(rc_mstr *out, const rc_writer *w, rc_wgsl_texture_pair pair, rc_arena *arena);

// ---- glsl_names.c ----

// Append a WGSL name as GLSL can take it (rc_writer_append_name).
void rc_glsl_append_name(rc_mstr *out, rc_str name, rc_arena *arena);

// ---- glsl_expr.c ----

// Append the GLSL for type, as float, vec3 or float[4][2].
void rc_glsl_write_type(rc_writer *w, rc_mstr *out, uint32_t type, uint32_t node, rc_arena *arena);

// Append the declaration of a variable called name of type, as float[4] a.
void rc_glsl_write_declaration(rc_writer *w, rc_mstr *out, uint32_t type, rc_str name, uint32_t node, rc_arena *arena);

// Append the zero value of type.
void rc_glsl_write_zero(rc_writer *w, rc_mstr *out, uint32_t type, uint32_t node, rc_arena *arena);

// Write the expression node. scalar, if not 0, is the scalar an abstract expression stands for
// where an abstract const is written out in place.
void rc_glsl_write_expr(rc_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena);

// Write left op right, of type, for any binary operator.
void rc_glsl_write_operation(rc_writer *w, rc_wgsl_token_type op, uint32_t left,
    uint32_t right, uint32_t type, uint32_t scalar, rc_arena *arena);

// ---- glsl_builtin.c ----

// Write a call to a built-in function.
void rc_glsl_write_builtin(rc_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena);

// Write a call to polyfill for operands of type, writing the polyfill first if it is new. The
// arguments follow, written by the caller, then a ")".
void rc_glsl_write_polyfill_call(rc_writer *w, rc_glsl_polyfill polyfill, uint32_t type, uint32_t node, rc_arena *arena);

// ---- glsl_stmt.c ----

// Write the function node.
void rc_glsl_write_function(rc_writer *w, uint32_t node, rc_arena *arena);

#endif // RC_SHADER_GLSL_WRITER_H_
