// glsl_writer.h
//
// Private to the GLSL writer's own files (glsl*.c): its state, and the functions they share.
//
// Writing happens in two passes. The first walks out from the entry point and gathers everything
// it reaches, in the order GLSL needs: structs before their users, consts before the code that
// uses them, and each function after the functions it calls. The second writes it all out. The
// polyfills the code needs are found as it is written, so they gather in a buffer of their own and
// go in ahead of the functions at the end.

#ifndef RC_SHADER_GLSL_WRITER_H_
#define RC_SHADER_GLSL_WRITER_H_

#include "glsl.h"
#include "richc/mstr.h"

// What the first pass knows of each node.
enum {
    // A module-scope declaration
    RC_GLSL_MARK_GLOBAL = 1,

    // Reached from the entry point, and being or done being gathered
    RC_GLSL_MARK_VISITING = 2,
    RC_GLSL_MARK_GATHERED = 4
};

#define RC_ARRAY_TYPE uint8_t
#define RC_ARRAY_NAME glsl_mark
#include "richc/array.template.h"

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

// A polyfill already written, and the type of its operands: GLSL overloads each polyfill once
// per type it takes.
typedef struct rc_glsl_polyfill_written {
    uint32_t operand_type;
    uint8_t polyfill;  // rc_glsl_polyfill
} rc_glsl_polyfill_written;

#define RC_ARRAY_TYPE rc_glsl_polyfill_written
#define RC_ARRAY_NAME glsl_polyfill_written
#include "richc/array.template.h"

typedef struct rc_glsl_writer {
    rc_wgsl_module module;
    rc_array_wgsl_typeinfo types;      // the module's, with more added for abstract consts' uses
    rc_glsl_dialect dialect;
    uint32_t entry;
    rc_span_glsl_mark marks;
    rc_array_wgsl_index structs;   // reached, dependencies first
    rc_array_wgsl_index globals;   // module-scope vars and consts reached, dependencies first
    rc_array_wgsl_index functions; // reached, each after those it calls
    rc_array_glsl_pair samplers;
    rc_array_glsl_polyfill_written polyfills_written;
    rc_mstr polyfills;
    rc_mstr code;
    uint32_t indent;
    rc_wgsl_error error;
} rc_glsl_writer;

// ---- glsl.c ----

// True once there is an error.
bool rc_glsl_failed(const rc_glsl_writer *w);

// Record message as the error at node, unless there is one already.
void rc_glsl_fail(rc_glsl_writer *w, uint32_t node, rc_str message);

// Record that the dialect being written can't do what, at node, unless there is an error already.
void rc_glsl_fail_dialect(rc_glsl_writer *w, uint32_t node, rc_str what, rc_arena *arena);

// What the checker learnt about node.
rc_wgsl_sema rc_glsl_sema(const rc_glsl_writer *w, uint32_t node);

// The type at index id.
rc_wgsl_typeinfo rc_glsl_typeinfo(const rc_glsl_writer *w, uint32_t id);

// Append text to the code.
void rc_glsl_put(rc_glsl_writer *w, rc_str text, rc_arena *arena);

// Start a new line of code, indented.
void rc_glsl_line(rc_glsl_writer *w, rc_arena *arena);

// The index of the combined sampler for pair, among those gathered.
uint32_t rc_glsl_pair_index(const rc_glsl_writer *w, rc_glsl_pair pair);

// ---- glsl_names.c ----

// Append a WGSL name as GLSL can take it: unchanged, unless GLSL reserves it or it could clash
// with a name we make (all of which start rc_), when it gains a prefix of rc_u_.
void rc_glsl_append_name(rc_mstr *out, rc_str name, rc_arena *arena);

// ---- glsl_expr.c ----

// Append the GLSL for type, as float, vec3 or float[4][2].
void rc_glsl_write_type(rc_glsl_writer *w, rc_mstr *out, uint32_t type, uint32_t node,
    rc_arena *arena);

// Append the declaration of a variable called name of type, as float[4] a.
void rc_glsl_write_declaration(rc_glsl_writer *w, rc_mstr *out, uint32_t type, rc_str name,
    uint32_t node, rc_arena *arena);

// Append the zero value of type.
void rc_glsl_write_zero(rc_glsl_writer *w, rc_mstr *out, uint32_t type, uint32_t node,
    rc_arena *arena);

// Write the expression node. scalar, if not 0, is the scalar an abstract expression stands for
// where an abstract const is written out in place.
void rc_glsl_write_expr(rc_glsl_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena);

// The type an expression node has where it is written: its own, or for an abstract one, the same
// shape of scalar.
uint32_t rc_glsl_expr_type(rc_glsl_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena);

// Write left op right, of type, for any binary operator.
void rc_glsl_write_operation(rc_glsl_writer *w, rc_wgsl_token_type op, uint32_t left,
    uint32_t right, uint32_t type, uint32_t scalar, rc_arena *arena);

// True if the expression node calls a function of the shader's, which might change something.
bool rc_glsl_has_calls(const rc_glsl_writer *w, uint32_t node);

// ---- glsl_builtin.c ----

// Write a call to a built-in function.
void rc_glsl_write_builtin(rc_glsl_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena);

// Write a call to polyfill for operands of type, writing the polyfill first if it is new. The
// arguments follow, written by the caller, then a ")".
void rc_glsl_write_polyfill_call(rc_glsl_writer *w, rc_glsl_polyfill polyfill, uint32_t type,
    uint32_t node, rc_arena *arena);

// ---- glsl_stmt.c ----

// Write the function node.
void rc_glsl_write_function(rc_glsl_writer *w, uint32_t node, rc_arena *arena);

#endif // RC_SHADER_GLSL_WRITER_H_
