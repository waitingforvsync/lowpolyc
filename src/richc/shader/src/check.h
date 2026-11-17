// check.h
//
// Private to richc_shader: checking a syntax tree against WGSL's rules, and recording what the
// writers need to know: about each node, and about each entry point, everything it reaches.
//
// The checker resolves every name, works out every expression's type and checks it, and settles
// each abstract number on the concrete type it is used as. The one exception is a const of
// abstract type: it keeps its type, and the writers write its value out in place at each use,
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
typedef enum rc_wgsl_builtin_value {
    RC_WGSL_BUILTIN_VALUE_NONE = 0,
    RC_WGSL_BUILTIN_VALUE_POSITION,
    RC_WGSL_BUILTIN_VALUE_VERTEX_INDEX,
    RC_WGSL_BUILTIN_VALUE_INSTANCE_INDEX,
    RC_WGSL_BUILTIN_VALUE_FRONT_FACING,
    RC_WGSL_BUILTIN_VALUE_FRAG_DEPTH,
    RC_WGSL_BUILTIN_VALUE_SAMPLE_INDEX,
    RC_WGSL_BUILTIN_VALUE_SAMPLE_MASK
} rc_wgsl_builtin_value;

// The attributes WGSL defines.
typedef enum rc_wgsl_attribute {
    RC_WGSL_ATTRIBUTE_NONE = 0,
    RC_WGSL_ATTRIBUTE_ALIGN,
    RC_WGSL_ATTRIBUTE_BINDING,
    RC_WGSL_ATTRIBUTE_BLEND_SRC,
    RC_WGSL_ATTRIBUTE_BUILTIN,
    RC_WGSL_ATTRIBUTE_COMPUTE,
    RC_WGSL_ATTRIBUTE_CONST,
    RC_WGSL_ATTRIBUTE_DIAGNOSTIC,
    RC_WGSL_ATTRIBUTE_FRAGMENT,
    RC_WGSL_ATTRIBUTE_GROUP,
    RC_WGSL_ATTRIBUTE_ID,
    RC_WGSL_ATTRIBUTE_INTERPOLATE,
    RC_WGSL_ATTRIBUTE_INVARIANT,
    RC_WGSL_ATTRIBUTE_LOCATION,
    RC_WGSL_ATTRIBUTE_MUST_USE,
    RC_WGSL_ATTRIBUTE_SIZE,
    RC_WGSL_ATTRIBUTE_VERTEX,
    RC_WGSL_ATTRIBUTE_WORKGROUP_SIZE
} rc_wgsl_attribute;

// The bit for an attribute in a set of them, as RC_WGSL_ATTRIBUTE_BIT(ALIGN).
#define RC_WGSL_ATTRIBUTE_BIT(name) (1u << RC_WGSL_ATTRIBUTE_##name)

// WebGPU's limits on the numbers attributes give, which every device meets.
enum {
    RC_WGSL_MAX_GROUPS = 4,
    RC_WGSL_MAX_BINDINGS = 1000,
    RC_WGSL_MAX_LOCATIONS = 16
};

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

// The flags of an rc_wgsl_sema.
enum {
    // An expression naming memory, which can be assigned to and have its address taken
    RC_WGSL_SEMA_REF = 1,

    // An expression whose value is known before the shader runs
    RC_WGSL_SEMA_CONST = 2,

    // An integer or bool const-expression whose value is in value
    RC_WGSL_SEMA_VALUE = 4
};

// What the checker learnt about one node. Which fields mean anything depends on the node:
//
//   expressions     type, flags, and a reference's space and access. A const-expression's value.
//   IDENT           target: the declaration named, or 0 for a predeclared name
//   CALL            target (a function's FN node), call, builtin, and type (the result)
//   declarations    type: a struct's or alias's type, a variable's or parameter's type, a
//                   function's return type
//   VAR             space and access
//   ATTRIBUTE       attribute
//   attribute args  value: an integer, or a built-in value, interpolation or sampling by name
//   FN              stage, for an entry point; fragment_only_at, the first thing it does that
//                   only a fragment shader may (a node), or 0
typedef struct rc_wgsl_sema {
    int64_t value;
    uint32_t type;
    uint32_t target;
    uint32_t fragment_only_at;
    uint8_t flags;
    uint8_t space;      // rc_wgsl_space
    uint8_t access;     // rc_wgsl_access
    uint8_t call;       // rc_wgsl_call
    uint8_t builtin;    // rc_wgsl_builtin
    uint8_t stage;      // rc_shader_stage
    uint8_t attribute;  // rc_wgsl_attribute
} rc_wgsl_sema;

#define RC_ARRAY_TYPE rc_wgsl_sema
#define RC_ARRAY_NAME wgsl_sema
#include "richc/array.template.h"

// An entry point input or output's attributes, decoded.
typedef struct rc_wgsl_io {
    uint32_t location;        // RC_INDEX_NONE when there is none
    uint8_t builtin_value;    // rc_wgsl_builtin_value
    uint8_t interpolation;    // rc_wgsl_interpolate
    uint8_t sampling;         // rc_wgsl_sampling
    bool invariant;
} rc_wgsl_io;

// True for the address spaces of resources, which are bound with @group and @binding.
static inline bool rc_wgsl_is_resource_space(rc_wgsl_space space)
{
    return space == RC_WGSL_SPACE_UNIFORM || space == RC_WGSL_SPACE_STORAGE || space == RC_WGSL_SPACE_HANDLE;
}

// Which way an input or output goes, at which stage.
typedef struct rc_wgsl_io_place {
    uint8_t stage;  // rc_shader_stage
    bool input;
} rc_wgsl_io_place;

// True if an input or output at place passes between stages, and so is interpolated.
static inline bool rc_wgsl_is_inter_stage(rc_wgsl_io_place place)
{
    return place.input == (place.stage == RC_SHADER_STAGE_FRAGMENT);
}

// One input or output of an entry point: a parameter or the result, or a member of the struct one
// of those is.
typedef struct rc_wgsl_io_slot {
    rc_wgsl_io io;
    uint32_t node;   // the PARAM, FIELD, or return type, to name it and blame it
    uint32_t type;
    uint32_t param;  // the parameter it is or is in, or RC_INDEX_NONE for the result
    uint32_t field;  // its FIELD within that parameter's or the result's struct, or 0
} rc_wgsl_io_slot;

#define RC_ARRAY_TYPE rc_wgsl_io_slot
#define RC_ARRAY_NAME wgsl_io_slot
#include "richc/array.template.h"

// A texture and a sampler used together, as module-scope VAR nodes. sampler is 0 for a texture
// read without one, by textureLoad and the queries.
typedef struct rc_wgsl_texture_pair {
    uint32_t texture;
    uint32_t sampler;
} rc_wgsl_texture_pair;

#define RC_ARRAY_TYPE rc_wgsl_texture_pair
#define RC_ARRAY_NAME wgsl_texture_pair
#include "richc/array.template.h"

// An entry point, and everything it reaches: what a program for it holds. Each list of
// declarations puts what something depends on before it.
typedef struct rc_wgsl_entry {
    uint32_t fn;
    rc_view_wgsl_index structs;
    rc_view_wgsl_index globals;         // module-scope vars and consts
    rc_view_wgsl_index functions;       // each after those it calls; the entry point last
    rc_view_wgsl_index resources;       // the uniform, storage and handle vars among globals
    rc_view_wgsl_texture_pair pairs;
    rc_view_wgsl_io_slot inputs;
    rc_view_wgsl_io_slot outputs;
    uint8_t stage;                      // rc_shader_stage
} rc_wgsl_entry;

#define RC_ARRAY_TYPE rc_wgsl_entry
#define RC_ARRAY_NAME wgsl_entry
#include "richc/array.template.h"

// A checked module: the tree, the types, one rc_wgsl_sema per node, and the entry points.
typedef struct rc_wgsl_module {
    rc_wgsl_ast ast;
    rc_view_wgsl_typeinfo types;
    rc_view_wgsl_sema sema;
    rc_view_wgsl_entry entries;
} rc_wgsl_module;

typedef struct rc_wgsl_check_result {
    rc_wgsl_module module;
    rc_wgsl_error error;
} rc_wgsl_check_result;

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

// True if the checked list attributes holds attribute.
bool rc_wgsl_has_attribute(rc_wgsl_ast ast, rc_view_wgsl_sema sema, uint32_t attributes, rc_wgsl_attribute attribute);

// The value of the first argument of attribute in the checked list attributes, or fallback if the
// list doesn't hold it.
int64_t rc_wgsl_attribute_value(rc_wgsl_ast ast, rc_view_wgsl_sema sema, uint32_t attributes,
    rc_wgsl_attribute attribute, int64_t fallback);

#endif // RC_SHADER_CHECK_H_
