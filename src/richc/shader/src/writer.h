// writer.h
//
// Private to richc_shader: what every writer of a target's programs shares. A writer writes one
// entry point of a checked module, using what the checker found it reaches (rc_wgsl_entry). Its
// program comes in three parts: declarations, then the helper functions the code turned out to
// need, then the code. The helpers are polyfills: functions a target lacks, or has with different
// edge cases, written once for each type of operand they are used on.
//
// A target without pointers writes each pointer out as the memory it points at, wherever it is
// used. rc_writer_is_stable_reference says when that is the same memory every time.

#ifndef RC_SHADER_WRITER_H_
#define RC_SHADER_WRITER_H_

#include "check.h"
#include "richc/mstr.h"

// A polyfill written already, and the type of its operands.
typedef struct rc_writer_polyfill {
    uint32_t operand_type;
    uint8_t polyfill;  // the target's own enumeration of them
} rc_writer_polyfill;

#define RC_ARRAY_TYPE rc_writer_polyfill
#define RC_ARRAY_NAME writer_polyfill
#include "richc/array.template.h"

// Where a program finds a resource, for a target that finds them by name: a buffer var, or a
// texture var and the sampler var it is combined with, if any.
typedef struct rc_writer_slot {
    rc_str name;
    uint32_t var;
    uint32_t sampler;
} rc_writer_slot;

#define RC_ARRAY_TYPE rc_writer_slot
#define RC_ARRAY_NAME writer_slot
#include "richc/array.template.h"

// Words a target reserves, in byte order, and a prefix it reserves too.
#define RC_ARRAY_TYPE rc_str
#define RC_ARRAY_NAME writer_word
#include "richc/array.template.h"

typedef struct rc_writer_reserved {
    rc_view_writer_word words;
    rc_str prefix;
} rc_writer_reserved;

typedef struct rc_writer {
    rc_wgsl_module module;
    rc_wgsl_entry entry;
    rc_wgsl_types types;          // the module's, with more added for abstract consts' uses
    rc_mstr polyfills;
    rc_mstr code;
    rc_array_writer_polyfill polyfills_written;
    rc_array_writer_slot slots;
    uint32_t indent;
    rc_wgsl_error error;
    uint8_t target;               // rc_shader_target
} rc_writer;

// A program, in its three parts, and where it finds its resources; or why it can't be written.
typedef struct rc_writer_result {
    rc_str declarations;
    rc_str polyfills;
    rc_str code;
    rc_view_writer_slot slots;
    rc_wgsl_error error;
} rc_writer_result;

// A writer of entry's program for target, allocating from arena.
rc_writer rc_writer_make(rc_wgsl_module module, rc_wgsl_entry entry, rc_shader_target target, rc_arena *arena);

// The program written, after the declarations.
rc_writer_result rc_writer_finish(const rc_writer *w, rc_str declarations);

// True once there is an error.
bool rc_writer_failed(const rc_writer *w);

// Record message as the error at node, unless there is one already.
void rc_writer_fail(rc_writer *w, uint32_t node, rc_str message);

// Record that the target can't write what, at node, unless there is an error already.
void rc_writer_fail_target(rc_writer *w, uint32_t node, rc_str what, rc_arena *arena);

// What the checker learnt about node.
rc_wgsl_sema rc_writer_sema(const rc_writer *w, uint32_t node);

// The type at index id.
rc_wgsl_typeinfo rc_writer_typeinfo(const rc_writer *w, uint32_t id);

// Append text to the code.
void rc_writer_put(rc_writer *w, rc_str text, rc_arena *arena);

// Start a new line of code, indented.
void rc_writer_line(rc_writer *w, rc_arena *arena);

// The type an expression node has where it is written: its own, or for an abstract one, the same
// shape of scalar (or the concrete type it would become, if scalar is 0).
uint32_t rc_writer_expr_type(rc_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena);

// Note that polyfill is needed for operand_type, returning true if it wasn't before.
bool rc_writer_note_polyfill(rc_writer *w, uint8_t polyfill, uint32_t operand_type, rc_arena *arena);

// Note that the program finds a resource as name.
void rc_writer_add_slot(rc_writer *w, rc_writer_slot slot, rc_arena *arena);

// True if the expression node calls a function of the shader's, which might change something.
bool rc_writer_has_calls(const rc_writer *w, uint32_t node);

// True if every index in the reference expression node stays the same once evaluated, so
// writing it out again at each use means the same memory.
bool rc_writer_is_stable_reference(const rc_writer *w, uint32_t node);

// Append a WGSL name as a target takes it: unchanged, unless the target reserves it, it holds a
// double underscore, or it could clash with a name we make (all of which start rc_), when it gains
// a prefix of rc_u_. That keeps every name distinct: a name that changes starts rc_, and one that
// doesn't, doesn't.
void rc_writer_append_name(rc_mstr *out, rc_str name, rc_writer_reserved reserved, rc_arena *arena);

// Append a WGSL float literal as decimal C does: no suffix, a point or an exponent always, and hex
// floats worked out.
void rc_writer_append_float(rc_mstr *out, rc_str literal, rc_arena *arena);

#endif // RC_SHADER_WRITER_H_
