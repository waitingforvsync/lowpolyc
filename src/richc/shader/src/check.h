// check.h
//
// Private to richc_shader: checking a syntax tree against WGSL's rules, and recording what the
// emitters need to know about each node.
//
// The checker resolves every name, works out every expression's type and checks it, and settles
// each abstract number on the concrete type it is used as. The one exception is a const of
// abstract type: it keeps its type, and the emitters write its value out in place at each use,
// in whatever type that use wants.
//
// Integer and bool const-expressions are evaluated, since array sizes, attributes and switch cases
// need their values. Floating-point ones are left for the shader compiler.

#ifndef RC_SHADER_CHECK_H_
#define RC_SHADER_CHECK_H_

#include "builtins.h"
#include "types.h"

// What kind of thing a CALL is.
typedef enum rc_wgsl_call {
    RC_WGSL_CALL_NONE = 0,
    RC_WGSL_CALL_FUNCTION,
    RC_WGSL_CALL_BUILTIN,
    RC_WGSL_CALL_CONSTRUCT,
    RC_WGSL_CALL_CONVERT,
    RC_WGSL_CALL_BITCAST
} rc_wgsl_call;

// The built-in values an entry point can take or give.
typedef enum rc_wgsl_value {
    RC_WGSL_VALUE_NONE = 0,
    RC_WGSL_VALUE_POSITION,
    RC_WGSL_VALUE_VERTEX_INDEX,
    RC_WGSL_VALUE_INSTANCE_INDEX,
    RC_WGSL_VALUE_FRONT_FACING,
    RC_WGSL_VALUE_FRAG_DEPTH,
    RC_WGSL_VALUE_SAMPLE_INDEX,
    RC_WGSL_VALUE_SAMPLE_MASK
} rc_wgsl_value;

typedef enum rc_wgsl_interpolate {
    RC_WGSL_INTERPOLATE_NONE = 0,
    RC_WGSL_INTERPOLATE_PERSPECTIVE,
    RC_WGSL_INTERPOLATE_LINEAR,
    RC_WGSL_INTERPOLATE_FLAT
} rc_wgsl_interpolate;

typedef enum rc_wgsl_sampling {
    RC_WGSL_SAMPLING_NONE = 0,
    RC_WGSL_SAMPLING_CENTER,
    RC_WGSL_SAMPLING_CENTROID,
    RC_WGSL_SAMPLING_SAMPLE,
    RC_WGSL_SAMPLING_FIRST,
    RC_WGSL_SAMPLING_EITHER
} rc_wgsl_sampling;

typedef enum rc_wgsl_stage {
    RC_WGSL_STAGE_NONE = 0,
    RC_WGSL_STAGE_VERTEX,
    RC_WGSL_STAGE_FRAGMENT
} rc_wgsl_stage;

// The flags of an rc_wgsl_sema.
enum {
    // An expression naming memory, which can be assigned to and have its address taken
    RC_WGSL_SEMA_REF = 1,

    // An expression whose value is known before the shader runs
    RC_WGSL_SEMA_CONST = 2,

    // An integer or bool const-expression whose value is in value
    RC_WGSL_SEMA_VALUE = 4,

    // A module-scope declaration being checked, and one already checked
    RC_WGSL_SEMA_CHECKING = 8,
    RC_WGSL_SEMA_CHECKED = 16
};

// What the checker learnt about one node. Which fields mean anything depends on the node:
//
//   expressions     type, flags, and a reference's space and access. A const-expression's value.
//   IDENT           target: the declaration named, or 0 for a predeclared name
//   CALL            target (a function's FN node), call, builtin, and type (the result)
//   declarations    type: a struct's or alias's type, a variable's or parameter's type, a
//                   function's return type
//   VAR             space and access
//   attribute args  value: an integer, or a built-in value, interpolation or sampling by name
//   FN              stage, for an entry point; fragment_only_at, the first thing it does that
//                   only a fragment shader may (a node), or 0
typedef struct rc_wgsl_sema {
    int64_t value;
    uint32_t type;
    uint32_t target;
    uint32_t fragment_only_at;
    uint8_t flags;
    uint8_t space;    // rc_wgsl_space
    uint8_t access;   // rc_wgsl_access
    uint8_t call;     // rc_wgsl_call
    uint8_t builtin;  // rc_wgsl_builtin
    uint8_t stage;    // rc_wgsl_stage
} rc_wgsl_sema;

#define RC_ARRAY_TYPE rc_wgsl_sema
#define RC_ARRAY_NAME wgsl_sema
#include "richc/array.template.h"

// A checked module: the tree, the types, and one rc_wgsl_sema per node.
typedef struct rc_wgsl_module {
    rc_wgsl_ast ast;
    rc_view_wgsl_typeinfo types;
    rc_view_wgsl_sema sema;
    rc_view_wgsl_index entry_points;  // FN nodes
} rc_wgsl_module;

typedef struct rc_wgsl_check_result {
    rc_wgsl_module module;
    rc_wgsl_error error;
} rc_wgsl_check_result;

// An entry point input or output's attributes, decoded.
typedef struct rc_wgsl_io {
    uint32_t location;     // RC_INDEX_NONE when there is none
    uint8_t builtin;       // rc_wgsl_value
    uint8_t interpolation; // rc_wgsl_interpolate
    uint8_t sampling;      // rc_wgsl_sampling
    bool invariant;
} rc_wgsl_io;

// Check ast, allocating what it learns from arena; or the first error.
rc_wgsl_check_result rc_wgsl_check(rc_wgsl_ast ast, rc_arena *arena);

// What the checker learnt about node index.
static inline rc_wgsl_sema rc_wgsl_module_sema(rc_wgsl_module module, uint32_t index)
{
    return rc_view_wgsl_sema_get(module.sema, index);
}

// The input or output attributes in the list attributes, checked already.
rc_wgsl_io rc_wgsl_io_of(rc_wgsl_ast ast, rc_view_wgsl_sema sema, uint32_t attributes);

// The @group and @binding of a resource.
typedef struct rc_wgsl_binding {
    uint32_t group;
    uint32_t binding;
} rc_wgsl_binding;

// The @group and @binding of the checked module-scope var.
rc_wgsl_binding rc_wgsl_binding_of(rc_wgsl_ast ast, rc_view_wgsl_sema sema, uint32_t var);

// The attribute called name in the list attributes, or 0.
uint32_t rc_wgsl_attribute_find(rc_wgsl_ast ast, uint32_t attributes, rc_str name);

#endif // RC_SHADER_CHECK_H_
