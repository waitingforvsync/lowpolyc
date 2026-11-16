// checker.h
//
// Private to the checker's own files (check*.c): its state, and the functions they share.
//
// Module-scope declarations may be used before they are declared, so each is checked when first
// needed, and marked while it is being checked to catch declarations that depend on themselves.
// Function bodies are checked last, once every declaration's type is known. Names in functions are
// kept on a stack, innermost last, and popped as their blocks end.

#ifndef RC_SHADER_CHECKER_H_
#define RC_SHADER_CHECKER_H_

#include "check.h"
#include "richc/hash_index.h"

// A name in scope, and the node that declares it.
typedef struct rc_wgsl_name {
    rc_str text;
    uint32_t node;
} rc_wgsl_name;

#define RC_ARRAY_TYPE rc_wgsl_name
#define RC_ARRAY_NAME wgsl_name
#include "richc/array.template.h"

// One function calling another, by the CALL node call.
typedef struct rc_wgsl_edge {
    uint32_t caller;
    uint32_t callee;
    uint32_t call;
} rc_wgsl_edge;

#define RC_ARRAY_TYPE rc_wgsl_edge
#define RC_ARRAY_NAME wgsl_edge
#include "richc/array.template.h"

typedef struct rc_wgsl_checker {
    rc_wgsl_ast ast;
    rc_array_wgsl_typeinfo types;
    rc_span_wgsl_sema sema;
    rc_array_wgsl_name globals;
    rc_hash_index global_index;   // into globals, by name
    rc_array_wgsl_name locals;
    rc_array_wgsl_edge calls;
    rc_array_wgsl_index entry_points;
    uint32_t function;            // the FN whose body is being checked, or 0
    uint32_t scope;               // where the innermost block's names start in locals
    uint32_t loops;               // loops around the statement being checked
    uint32_t breakables;          // loops and switches around it
    bool in_continuing;           // inside a continuing block, and no loop inside that
    bool broke;                   // a break has reached the innermost loop or switch
    uint32_t depth;               // declarations being checked on demand, nested
    rc_wgsl_error error;
} rc_wgsl_checker;

// A name looked up: the declaration found in the shader, and whether it is at module scope, or
// else the predeclared name.
typedef struct rc_wgsl_found {
    uint32_t node;
    bool global;
    rc_wgsl_predeclared predeclared;
} rc_wgsl_found;

// ---- check.c: state, errors and names ----

// True once there is an error.
bool rc_wgsl_check_failed(const rc_wgsl_checker *c);

// Record message as the error at node, unless there is one already.
void rc_wgsl_check_fail(rc_wgsl_checker *c, uint32_t node, rc_str message);

// Record an error at node made of before, the name of type, and after.
void rc_wgsl_check_fail_type(rc_wgsl_checker *c, uint32_t node, rc_str before, uint32_t type,
    rc_str after, rc_arena *arena);

// Record an error at node made of before, two types' names, and their joining words.
void rc_wgsl_check_fail_types(rc_wgsl_checker *c, uint32_t node, rc_str before, uint32_t a,
    rc_str between, uint32_t b, rc_arena *arena);

// Record an error at node made of before, the node's own name, and after.
void rc_wgsl_check_fail_name(rc_wgsl_checker *c, uint32_t node, rc_str before, rc_str after,
    rc_arena *arena);

// What has been learnt about node so far.
rc_wgsl_sema rc_wgsl_check_sema(const rc_wgsl_checker *c, uint32_t node);

// Replace what is known about node.
void rc_wgsl_check_set(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema sema);

// Change just node's type.
void rc_wgsl_check_set_type(rc_wgsl_checker *c, uint32_t node, uint32_t type);

// The type at index id.
rc_wgsl_typeinfo rc_wgsl_check_typeinfo(const rc_wgsl_checker *c, uint32_t id);

// Whether the type at index id is a vector, a struct and so on.
rc_wgsl_typeinfo_type rc_wgsl_check_typeinfo_type(const rc_wgsl_checker *c, uint32_t id);

// What name means where it is used: in the function's scopes first, then the module's, then
// WGSL's own.
rc_wgsl_found rc_wgsl_check_lookup(const rc_wgsl_checker *c, rc_str name);

// Make a function-scope declaration visible to what follows, failing if its block already has one
// of that name.
void rc_wgsl_check_declare_local(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// Note that the function being checked does something only fragment shaders may, at node.
void rc_wgsl_check_note_fragment_only(rc_wgsl_checker *c, uint32_t node);

// Check the module-scope declaration node now, if it hasn't been already.
void rc_wgsl_check_global(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// ---- check_decl.c: declarations ----

// Check the module-scope declaration node, by its node type.
void rc_wgsl_check_declaration(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// Check a let or const, in a function or (for const) the module.
void rc_wgsl_check_value_decl(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// ---- check_type.c: types and attributes ----

// The type the type expression node names.
uint32_t rc_wgsl_check_type_expr(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// The type a predeclared type name makes from the template list templates, with node for errors.
uint32_t rc_wgsl_check_generated_type(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_predeclared predeclared, uint32_t templates, rc_arena *arena);

// The enumerant the template argument node names, which must be from set.
uint32_t rc_wgsl_check_enumerant(rc_wgsl_checker *c, uint32_t node, rc_wgsl_enumerant set);

// The value of the integer const-expression node, which must be at least minimum.
int64_t rc_wgsl_check_const_int(rc_wgsl_checker *c, uint32_t node, int64_t minimum,
    rc_arena *arena);

// Check every attribute in the list attributes and its arguments, failing for any not in the
// allowed names, separated by spaces.
void rc_wgsl_check_attributes(rc_wgsl_checker *c, uint32_t attributes, rc_str allowed,
    rc_arena *arena);

// ---- check_expr.c: expressions ----

// Check the expression node, returning what was learnt.
rc_wgsl_sema rc_wgsl_check_expr(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// Check the expression node, which must give a value (not a type or function), and return its
// value's type, after any load.
uint32_t rc_wgsl_check_value(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// Settle an abstract expression node on type, which it must convert to, and its parts with it.
void rc_wgsl_check_concretise(rc_wgsl_checker *c, uint32_t node, uint32_t type, rc_arena *arena);

// Settle an abstract expression on the type it becomes when nothing says otherwise.
void rc_wgsl_check_concretise_default(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// Check that the checked expression node converts to type, and settle it on it.
void rc_wgsl_check_convert(rc_wgsl_checker *c, uint32_t node, uint32_t type, rc_arena *arena);

// The type of left op right for an arithmetic, bitwise or shift operator, or 0 if it can't take
// them. Nothing settles.
uint32_t rc_wgsl_check_operation_type(rc_wgsl_checker *c, rc_wgsl_token_type op, uint32_t left,
    uint32_t right, rc_arena *arena);

// ---- check_call.c: calls and constructors ----

// Check the CALL node.
rc_wgsl_sema rc_wgsl_check_call(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// ---- check_builtin.c: built-in functions ----

// Check a CALL node calling builtin, whose arguments are already checked.
rc_wgsl_sema rc_wgsl_check_builtin_call(rc_wgsl_checker *c, uint32_t node, rc_wgsl_builtin builtin,
    rc_view_wgsl_index args, rc_arena *arena);

// ---- check_stmt.c: statements and function bodies ----

// Check the body of the function node.
void rc_wgsl_check_body(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// ---- check_entry.c: entry points ----

// Check an entry point's inputs and outputs, and that it calls nothing its stage can't.
void rc_wgsl_check_entry_point(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

#endif // RC_SHADER_CHECKER_H_
