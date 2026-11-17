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

enum {
    // How deeply types may nest: every pass over a type follows it down, a level at a time
    RC_WGSL_MAX_TYPE_DEPTH = 32
};

// A name in scope, and the node that declares it.
typedef struct rc_wgsl_name {
    rc_str text;
    uint32_t node;
} rc_wgsl_name;

#define RC_ARRAY_TYPE rc_wgsl_name
#define RC_ARRAY_NAME wgsl_name
#include "richc/array.template.h"

// One module-scope declaration naming another: a function calling one, or anything using a
// struct, var, const or alias. node is where it does.
typedef struct rc_wgsl_reference {
    uint32_t user;
    uint32_t used;
    uint32_t node;
} rc_wgsl_reference;

#define RC_ARRAY_TYPE rc_wgsl_reference
#define RC_ARRAY_NAME wgsl_reference
#include "richc/array.template.h"

// A texture function's use of a texture and sampler, in function.
typedef struct rc_wgsl_texture_use {
    rc_wgsl_texture_pair pair;
    uint32_t function;
} rc_wgsl_texture_use;

#define RC_ARRAY_TYPE rc_wgsl_texture_use
#define RC_ARRAY_NAME wgsl_texture_use
#include "richc/array.template.h"

// The references, grouped by user: those of node n are from starts[n] up to starts[n + 1].
typedef struct rc_wgsl_graph {
    rc_view_wgsl_index starts;
    rc_view_wgsl_reference references;
} rc_wgsl_graph;

// How far a module-scope declaration's check has got.
typedef enum rc_wgsl_decl_state {
    RC_WGSL_DECL_STATE_NONE = 0,
    RC_WGSL_DECL_STATE_CHECKING,
    RC_WGSL_DECL_STATE_CHECKED
} rc_wgsl_decl_state;

#define RC_ARRAY_TYPE uint8_t
#define RC_ARRAY_NAME wgsl_state
#include "richc/array.template.h"

typedef struct rc_wgsl_checker {
    rc_wgsl_ast ast;
    rc_wgsl_types types;
    rc_span_wgsl_sema sema;
    rc_span_wgsl_state states;    // an rc_wgsl_decl_state for each node
    rc_array_wgsl_name globals;
    rc_hash_index global_index;   // into globals, by name
    rc_array_wgsl_name locals;
    rc_array_wgsl_reference references;
    rc_array_wgsl_texture_use texture_uses;
    rc_array_wgsl_index pointer_calls;  // calls to the shader's functions passing a pointer
    rc_array_wgsl_index entry_points;
    rc_array_wgsl_entry entries;
    rc_wgsl_graph graph;
    uint32_t user;                // the declaration being checked, or the FN whose body is, or 0
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

// Record message as the error at node, unless there is one already. Each of these returns {0},
// what a failed check gives.
rc_wgsl_sema rc_wgsl_check_fail(rc_wgsl_checker *c, uint32_t node, rc_str message);

// Record an error at node made of before, the name of type, and after.
rc_wgsl_sema rc_wgsl_check_fail_type(rc_wgsl_checker *c, uint32_t node, rc_str before, uint32_t type, rc_str after, rc_arena *arena);

// Record an error at node made of before, two types' names, and their joining words.
rc_wgsl_sema rc_wgsl_check_fail_types(rc_wgsl_checker *c, uint32_t node, rc_str before,
    uint32_t a, rc_str between, uint32_t b, rc_arena *arena);

// Record an error at node made of before, the node's own name, and after.
rc_wgsl_sema rc_wgsl_check_fail_name(rc_wgsl_checker *c, uint32_t node, rc_str before, rc_str after, rc_arena *arena);

// What has been learnt about node so far.
rc_wgsl_sema rc_wgsl_check_sema(const rc_wgsl_checker *c, uint32_t node);

// Replace what is known about node.
void rc_wgsl_check_set(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema sema);

// Change just node's type.
void rc_wgsl_check_set_type(rc_wgsl_checker *c, uint32_t node, uint32_t type);

// The module as checked so far.
rc_wgsl_module rc_wgsl_check_module(const rc_wgsl_checker *c);

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

// Check the module-scope declaration decl now, if it hasn't been already, noting that what is
// being checked uses it at node.
void rc_wgsl_check_global(rc_wgsl_checker *c, uint32_t decl, uint32_t node, rc_arena *arena);

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

// The value of the integer const-expression node, which must be from minimum to maximum.
int64_t rc_wgsl_check_const_int(rc_wgsl_checker *c, uint32_t node, int64_t minimum, int64_t maximum, rc_arena *arena);

// Check every attribute in the list attributes and its arguments, failing for any not in allowed,
// a set of RC_WGSL_ATTRIBUTE_BIT.
void rc_wgsl_check_attributes(rc_wgsl_checker *c, uint32_t attributes, uint32_t allowed, rc_arena *arena);

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
uint32_t rc_wgsl_check_operation_type(rc_wgsl_checker *c, rc_wgsl_token_type op, uint32_t left, uint32_t right, rc_arena *arena);

// ---- check_call.c: calls and constructors ----

// The type of argument i.
uint32_t rc_wgsl_check_arg_type(const rc_wgsl_checker *c, rc_view_wgsl_index args, uint32_t i);

// The flags a call keeps from its arguments: const if they all are.
uint8_t rc_wgsl_check_args_flags(const rc_wgsl_checker *c, rc_view_wgsl_index args);

// The type arguments [first, last) all convert to, or that their scalars do if scalars; or 0 if
// there is none.
uint32_t rc_wgsl_check_common_arg_type(const rc_wgsl_checker *c, rc_view_wgsl_index args, uint32_t first, uint32_t last, bool scalars);

// Convert arguments [first, last) to type.
void rc_wgsl_check_convert_args(rc_wgsl_checker *c, rc_view_wgsl_index args, uint32_t first, uint32_t last, uint32_t type, rc_arena *arena);

// Check the CALL node.
rc_wgsl_sema rc_wgsl_check_call(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// ---- check_builtin.c: built-in functions ----

// Check a CALL node calling builtin, whose arguments are already checked.
rc_wgsl_sema rc_wgsl_check_builtin_call(rc_wgsl_checker *c, uint32_t node, rc_wgsl_builtin builtin,
    rc_view_wgsl_index args, rc_arena *arena);

// ---- check_stmt.c: statements and function bodies ----

// Check the body of the function node.
void rc_wgsl_check_body(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// ---- check_graph.c: what uses what ----

// Group the references by user, for the walks below.
void rc_wgsl_check_graph(rc_wgsl_checker *c, rc_arena *arena);

// Fail if any function calls itself, directly or through others: WGSL has no recursion.
void rc_wgsl_check_recursion(rc_wgsl_checker *c, rc_arena *arena);

// Fail at a call passing a pointer to memory the call can reach another way: through another of
// its arguments, or because the function called uses it itself. WGSL forbids that whenever either
// way writes the memory; we forbid it outright.
void rc_wgsl_check_aliasing(rc_wgsl_checker *c, rc_arena *arena);

// The entry point fn, with its inputs and outputs and everything it reaches.
rc_wgsl_entry rc_wgsl_check_reach(rc_wgsl_checker *c, uint32_t fn, rc_view_wgsl_io_slot inputs,
    rc_view_wgsl_io_slot outputs, rc_arena *arena);

// ---- check_entry.c: entry points ----

// Check an entry point's inputs and outputs, and that what it reaches suits its stage and binds
// each resource once, returning it whole.
rc_wgsl_entry rc_wgsl_check_entry_point(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

#endif // RC_SHADER_CHECKER_H_
