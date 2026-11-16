// check_call.c
//
// Calls: to the shader's own functions, and to type names, which construct or convert values.
// Built-in functions have their own file, check_builtin.c.

#include "checker.h"

#include "richc/mstr.h"

// ---- arguments ----

// The type of argument i.
static uint32_t rc_wgsl_arg_type(const rc_wgsl_checker *c, rc_view_wgsl_index args, uint32_t i)
{
    return rc_wgsl_check_sema(c, rc_view_wgsl_index_get(args, i)).type;
}


// The flags a call keeps from its arguments: const if they all are.
static uint8_t rc_wgsl_args_flags(const rc_wgsl_checker *c, rc_view_wgsl_index args)
{
    uint8_t flags = RC_WGSL_SEMA_CONST;

    for (uint32_t i = 0; i < args.num; i++) {
        flags &= rc_wgsl_check_sema(c, rc_view_wgsl_index_get(args, i)).flags;
    }
    return flags;
}


// Fail at node unless there are expected arguments, naming what takes them.
static bool rc_wgsl_check_arg_count(rc_wgsl_checker *c, uint32_t node, rc_view_wgsl_index args,
    uint32_t expected, rc_arena *arena)
{
    rc_mstr message;

    if (args.num == expected) {
        return true;
    }
    message = rc_mstr_from_str(RC_STR("'"), 64, arena);
    rc_mstr_append(&message, rc_wgsl_ast_text(c->ast, node), arena);
    rc_mstr_append(&message, RC_STR("' takes "), arena);
    rc_mstr_append_u32(&message, expected, arena);
    rc_mstr_append(&message, expected == 1 ? RC_STR(" argument, not ") : RC_STR(" arguments, not "),
        arena);
    rc_mstr_append_u32(&message, args.num, arena);
    rc_wgsl_check_fail(c, node, message.view);
    return false;
}


// Convert each argument from first on to type.
static void rc_wgsl_convert_args(rc_wgsl_checker *c, rc_view_wgsl_index args, uint32_t first,
    uint32_t type, rc_arena *arena)
{
    for (uint32_t i = first; i < args.num; i++) {
        rc_wgsl_check_convert(c, rc_view_wgsl_index_get(args, i), type, arena);
    }
}

// ---- the shader's own functions ----

// A call to the function fn.
static rc_wgsl_sema rc_wgsl_check_function_call(rc_wgsl_checker *c, uint32_t node, uint32_t fn,
    rc_view_wgsl_index args, rc_arena *arena)
{
    rc_view_wgsl_index params;

    rc_wgsl_check_global(c, fn, arena);
    params = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_fn_parts(c->ast, fn).params);
    if (!rc_wgsl_check_arg_count(c, node, args, params.num, arena)) {
        return (rc_wgsl_sema) {0};
    }
    for (uint32_t i = 0; i < args.num; i++) {
        uint32_t param = rc_view_wgsl_index_get(params, i);

        rc_wgsl_check_convert(c, rc_view_wgsl_index_get(args, i), rc_wgsl_check_sema(c, param).type,
            arena);
    }
    if (rc_wgsl_check_sema(c, fn).stage != RC_WGSL_STAGE_NONE) {
        rc_wgsl_check_fail(c, node, RC_STR("an entry point can't be called"));
    }
    if (c->function != 0) {
        rc_wgsl_edge edge = {.caller = c->function, .callee = fn, .call = node};

        rc_array_wgsl_edge_push(&c->calls, edge, arena);
    }

    return (rc_wgsl_sema) {
        .type = rc_wgsl_check_sema(c, fn).type,
        .target = fn,
        .call = RC_WGSL_CALL_FUNCTION
    };
}

// ---- constructing values ----

// What a call to a constructor gives: a value of type, const if its arguments are.
static rc_wgsl_sema rc_wgsl_constructed(const rc_wgsl_checker *c, uint32_t type,
    rc_wgsl_call call, rc_view_wgsl_index args)
{
    return (rc_wgsl_sema) {
        .type = type,
        .flags = rc_wgsl_args_flags(c, args),
        .call = (uint8_t)call
    };
}


// The zero value of type, as T().
static rc_wgsl_sema rc_wgsl_check_zero_value(rc_wgsl_checker *c, uint32_t node, uint32_t type,
    rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_sema result = rc_wgsl_constructed(c, type, RC_WGSL_CALL_CONSTRUCT, args);

    if (!rc_wgsl_type_is_constructible(c->types.view, type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("can't construct "), type, RC_STR(""), arena);
    }
    if (rc_wgsl_type_is_scalar(c->types.view, type) && type != RC_WGSL_TYPE_ID_F32) {
        result.flags |= RC_WGSL_SEMA_VALUE;
    }
    return result;
}


// The integer or bool value v of type from, converted to the integer or bool type to.
static int64_t rc_wgsl_convert_value(rc_wgsl_typeinfo_type from, rc_wgsl_typeinfo_type to, int64_t v)
{
    if (to == RC_WGSL_TYPEINFO_TYPE_BOOL) {
        return v != 0;
    }
    if (to == RC_WGSL_TYPEINFO_TYPE_I32 && from == RC_WGSL_TYPEINFO_TYPE_U32) {
        return (int32_t)(uint32_t)v;
    }
    if (to == RC_WGSL_TYPEINFO_TYPE_U32 && from == RC_WGSL_TYPEINFO_TYPE_I32) {
        return (uint32_t)(int32_t)v;
    }
    return v;
}


// A scalar from one scalar: the same value, or a conversion from another scalar type.
static rc_wgsl_sema rc_wgsl_check_scalar_construct(rc_wgsl_checker *c, uint32_t node,
    uint32_t type, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t arg = rc_view_wgsl_index_get(args, 0);
    uint32_t from = rc_wgsl_arg_type(c, args, 0);
    rc_wgsl_sema result = rc_wgsl_constructed(c, type, RC_WGSL_CALL_CONSTRUCT, args);
    rc_wgsl_sema value;

    if (!rc_wgsl_check_arg_count(c, node, args, 1, arena)) {
        return result;
    }
    if (!rc_wgsl_type_is_scalar(c->types.view, from)) {
        rc_wgsl_check_fail_types(c, arg, RC_STR("can't make "), type, RC_STR(" from "), from, arena);
        return result;
    }
    if (rc_wgsl_type_converts(c->types.view, from, type)) {
        rc_wgsl_check_convert(c, arg, type, arena);
    }
    else {
        rc_wgsl_check_concretise_default(c, arg, arena);
        result.call = RC_WGSL_CALL_CONVERT;
    }

    // Integers and bools fold; floats are left for the shader compiler
    value = rc_wgsl_check_sema(c, arg);
    if ((value.flags & RC_WGSL_SEMA_VALUE) && type != RC_WGSL_TYPE_ID_F32) {
        result.flags |= RC_WGSL_SEMA_VALUE;
        result.value = rc_wgsl_convert_value(rc_wgsl_check_typeinfo_type(c, value.type),
            rc_wgsl_check_typeinfo_type(c, type), value.value);
    }
    return result;
}


// The number of scalars a scalar or vector holds, or 0 for anything else.
static uint32_t rc_wgsl_components(const rc_wgsl_checker *c, uint32_t type)
{
    if (!rc_wgsl_type_is_scalar_or_vector(c->types.view, type)) {
        return 0;
    }
    return rc_wgsl_type_width_of(c->types.view, type);
}


// A vector from its parts, each a scalar or vector whose scalars convert to element; fails at
// node unless they add up to width.
static void rc_wgsl_check_vector_parts(rc_wgsl_checker *c, uint32_t node, uint32_t element,
    uint32_t width, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t total = 0;

    for (uint32_t i = 0; i < args.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t arg = rc_view_wgsl_index_get(args, i);
        uint32_t type = rc_wgsl_arg_type(c, args, i);
        uint32_t components = rc_wgsl_components(c, type);

        if (components == 0) {
            rc_wgsl_check_fail_type(c, arg, RC_STR("a vector can't be made from "), type, RC_STR(""),
                arena);
            return;
        }
        rc_wgsl_check_convert(c, arg, rc_wgsl_type_with_scalar(&c->types, type, element, arena),
            arena);
        total += components;
    }
    if (total != width && !rc_wgsl_check_failed(c)) {
        rc_wgsl_check_fail(c, node, RC_STR("these don't add up to the vector's size"));
    }
}


// A vector: a splat of one scalar, a conversion of another vector, or its parts.
static rc_wgsl_sema rc_wgsl_check_vector_construct(rc_wgsl_checker *c, uint32_t node,
    uint32_t type, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_typeinfo vector = rc_wgsl_check_typeinfo(c, type);
    uint32_t from = rc_wgsl_arg_type(c, args, 0);
    rc_wgsl_sema result = rc_wgsl_constructed(c, type, RC_WGSL_CALL_CONSTRUCT, args);

    if (args.num == 1 && rc_wgsl_type_is_scalar(c->types.view, from)) {
        rc_wgsl_check_convert(c, rc_view_wgsl_index_get(args, 0), vector.element, arena);
    }
    else if (args.num == 1 && rc_wgsl_check_typeinfo_type(c, from) == RC_WGSL_TYPEINFO_TYPE_VECTOR
        && rc_wgsl_type_width_of(c->types.view, from) == vector.width
        && !rc_wgsl_type_converts(c->types.view, from, type)) {
        rc_wgsl_check_concretise_default(c, rc_view_wgsl_index_get(args, 0), arena);
        result.call = RC_WGSL_CALL_CONVERT;
    }
    else {
        rc_wgsl_check_vector_parts(c, node, vector.element, vector.width, args, arena);
    }
    return result;
}


// A matrix: from another matrix, its columns, or all its numbers column by column.
static rc_wgsl_sema rc_wgsl_check_matrix_construct(rc_wgsl_checker *c, uint32_t node,
    uint32_t type, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_typeinfo matrix = rc_wgsl_check_typeinfo(c, type);

    if (args.num == 1) {
        rc_wgsl_check_convert(c, rc_view_wgsl_index_get(args, 0), type, arena);
    }
    else if (args.num == matrix.columns) {
        rc_wgsl_convert_args(c, args, 0, rc_wgsl_type_column(&c->types, type, arena), arena);
    }
    else if (args.num == (uint32_t)matrix.columns * matrix.rows) {
        rc_wgsl_convert_args(c, args, 0, matrix.element, arena);
    }
    else {
        rc_wgsl_check_fail_type(c, node, RC_STR(""), type,
            RC_STR(" takes its columns, or all its numbers"), arena);
    }
    return rc_wgsl_constructed(c, type, RC_WGSL_CALL_CONSTRUCT, args);
}


// An array from its elements.
static rc_wgsl_sema rc_wgsl_check_array_construct(rc_wgsl_checker *c, uint32_t node,
    uint32_t type, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_typeinfo array = rc_wgsl_check_typeinfo(c, type);

    if (array.count == 0) {
        rc_wgsl_check_fail(c, node, RC_STR("an array sized at run time can't be constructed"));
    }
    else if (rc_wgsl_check_arg_count(c, node, args, array.count, arena)) {
        rc_wgsl_convert_args(c, args, 0, array.element, arena);
    }
    return rc_wgsl_constructed(c, type, RC_WGSL_CALL_CONSTRUCT, args);
}


// A struct from its members.
static rc_wgsl_sema rc_wgsl_check_struct_construct(rc_wgsl_checker *c, uint32_t node,
    uint32_t type, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t decl = rc_wgsl_check_typeinfo(c, type).decl;
    rc_view_wgsl_index fields = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, decl).lhs);

    if (!rc_wgsl_type_is_constructible(c->types.view, type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("can't construct "), type, RC_STR(""), arena);
    }
    else if (rc_wgsl_check_arg_count(c, node, args, fields.num, arena)) {
        for (uint32_t i = 0; i < args.num; i++) {
            uint32_t field = rc_view_wgsl_index_get(fields, i);

            rc_wgsl_check_convert(c, rc_view_wgsl_index_get(args, i),
                rc_wgsl_check_sema(c, field).type, arena);
        }
    }
    return rc_wgsl_constructed(c, type, RC_WGSL_CALL_CONSTRUCT, args);
}


// A value of type made from args.
static rc_wgsl_sema rc_wgsl_check_construct(rc_wgsl_checker *c, uint32_t node, uint32_t type,
    rc_view_wgsl_index args, rc_arena *arena)
{
    if (args.num == 0) {
        return rc_wgsl_check_zero_value(c, node, type, args, arena);
    }
    switch (rc_wgsl_check_typeinfo_type(c, type)) {
    case RC_WGSL_TYPEINFO_TYPE_BOOL:
    case RC_WGSL_TYPEINFO_TYPE_I32:
    case RC_WGSL_TYPEINFO_TYPE_U32:
    case RC_WGSL_TYPEINFO_TYPE_F32:
        return rc_wgsl_check_scalar_construct(c, node, type, args, arena);
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
        return rc_wgsl_check_vector_construct(c, node, type, args, arena);
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
        return rc_wgsl_check_matrix_construct(c, node, type, args, arena);
    case RC_WGSL_TYPEINFO_TYPE_ARRAY:
        return rc_wgsl_check_array_construct(c, node, type, args, arena);
    case RC_WGSL_TYPEINFO_TYPE_STRUCT:
        return rc_wgsl_check_struct_construct(c, node, type, args, arena);
    default:
        rc_wgsl_check_fail_type(c, node, RC_STR("can't construct "), type, RC_STR(""), arena);
        return (rc_wgsl_sema) {0};
    }
}

// ---- types worked out from the arguments ----

// The type every argument converts to, or of their scalars if scalars, or 0 if there is none.
static uint32_t rc_wgsl_common_arg_type(rc_wgsl_checker *c, rc_view_wgsl_index args, bool scalars)
{
    uint32_t common = 0;

    for (uint32_t i = 0; i < args.num; i++) {
        uint32_t type = rc_wgsl_arg_type(c, args, i);

        if (scalars) {
            type = rc_wgsl_type_scalar_of(c->types.view, type);
        }
        if (i == 0) {
            common = type;
        }
        else if (rc_wgsl_type_converts(c->types.view, common, type)) {
            common = type;
        }
        else if (!rc_wgsl_type_converts(c->types.view, type, common)) {
            return 0;
        }
    }
    return common;
}


// The type a generic name such as vec3, mat2x2 or array makes from args.
static uint32_t rc_wgsl_infer_type(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_predeclared predeclared, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_generator generator = (rc_wgsl_generator)predeclared.generator;
    uint32_t scalar = rc_wgsl_common_arg_type(c, args, generator != RC_WGSL_GENERATOR_ARRAY);

    if (args.num == 0) {
        rc_wgsl_check_fail_name(c, node, RC_STR("'"),
            RC_STR("' needs its element type when it has no arguments"), arena);
        return 0;
    }
    if (scalar == 0) {
        rc_wgsl_check_fail_name(c, node, RC_STR("the arguments to '"),
            RC_STR("' have no type in common"), arena);
        return 0;
    }
    if (generator == RC_WGSL_GENERATOR_ARRAY) {
        return rc_wgsl_type_array(&c->types, scalar, args.num, arena);
    }
    if (generator == RC_WGSL_GENERATOR_VECTOR) {
        return rc_wgsl_type_vector(&c->types, scalar, predeclared.width, arena);
    }

    // A matrix of integers is a matrix of numbers
    if (scalar == RC_WGSL_TYPE_ID_ABSTRACT_INT) {
        scalar = RC_WGSL_TYPE_ID_ABSTRACT_FLOAT;
    }
    if (!rc_wgsl_type_is_float_scalar(c->types.view, scalar)) {
        rc_wgsl_check_fail(c, node, RC_STR("a matrix holds f32"));
        return 0;
    }
    return rc_wgsl_type_matrix(&c->types, scalar, predeclared.columns, predeclared.rows,
        arena);
}


// A call to a predeclared type name: a constructor, its type written out or worked out.
static rc_wgsl_sema rc_wgsl_check_type_call(rc_wgsl_checker *c, uint32_t node, uint32_t callee,
    rc_wgsl_predeclared predeclared, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_generator generator = (rc_wgsl_generator)predeclared.generator;
    uint32_t templates = rc_wgsl_ast_node(c->ast, callee).lhs;
    bool generic = templates == 0 && predeclared.element == 0
        && (generator == RC_WGSL_GENERATOR_VECTOR || generator == RC_WGSL_GENERATOR_MATRIX
            || generator == RC_WGSL_GENERATOR_ARRAY);
    uint32_t type = generic ? rc_wgsl_infer_type(c, callee, predeclared, args, arena)
                            : rc_wgsl_check_generated_type(c, callee, predeclared, templates, arena);

    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }
    rc_wgsl_check_set(c, callee, (rc_wgsl_sema) {.type = type});
    return rc_wgsl_check_construct(c, node, type, args, arena);
}

// ---- calls ----

// A call to the shader's own declaration decl: a function, or a struct or alias to construct.
static rc_wgsl_sema rc_wgsl_check_declared_call(rc_wgsl_checker *c, uint32_t node, uint32_t callee,
    uint32_t decl, rc_view_wgsl_index args, rc_arena *arena)
{
    switch (rc_wgsl_ast_type(c->ast, decl)) {
    case RC_WGSL_NODE_TYPE_FN:
        if (rc_wgsl_ast_node(c->ast, callee).lhs != 0) {
            rc_wgsl_check_fail(c, callee, RC_STR("a function takes no template list"));
            return (rc_wgsl_sema) {0};
        }
        return rc_wgsl_check_function_call(c, node, decl, args, arena);
    case RC_WGSL_NODE_TYPE_STRUCT:
    case RC_WGSL_NODE_TYPE_ALIAS:
        return rc_wgsl_check_construct(c, node, rc_wgsl_check_type_expr(c, callee, arena), args,
            arena);
    default:
        rc_wgsl_check_fail_name(c, callee, RC_STR("'"), RC_STR("' can't be called"), arena);
        return (rc_wgsl_sema) {0};
    }
}


rc_wgsl_sema rc_wgsl_check_call(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_view_wgsl_index args = rc_wgsl_ast_list(c->ast, n.rhs);
    rc_wgsl_found found;

    for (uint32_t i = 0; i < args.num; i++) {
        rc_wgsl_check_value(c, rc_view_wgsl_index_get(args, i), arena);
    }
    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }
    found = rc_wgsl_check_lookup(c, rc_wgsl_ast_text(c->ast, n.lhs));
    if (found.node != 0) {
        return rc_wgsl_check_declared_call(c, node, n.lhs, found.node, args, arena);
    }
    switch ((rc_wgsl_predeclared_type)found.predeclared.type) {
    case RC_WGSL_PREDECLARED_TYPE_GENERATOR:
        return rc_wgsl_check_type_call(c, node, n.lhs, found.predeclared, args, arena);
    case RC_WGSL_PREDECLARED_TYPE_FUNCTION:
        return rc_wgsl_check_builtin_call(c, node, (rc_wgsl_builtin)found.predeclared.builtin, args,
            arena);
    case RC_WGSL_PREDECLARED_TYPE_UNSUPPORTED:
        rc_wgsl_check_fail_name(c, n.lhs, RC_STR("'"), RC_STR("' isn't supported yet"), arena);
        return (rc_wgsl_sema) {0};
    case RC_WGSL_PREDECLARED_TYPE_ENUMERANT:
        rc_wgsl_check_fail_name(c, n.lhs, RC_STR("'"), RC_STR("' can't be called"), arena);
        return (rc_wgsl_sema) {0};
    default:
        rc_wgsl_check_fail_name(c, n.lhs, RC_STR("there is no function called '"), RC_STR("'"),
            arena);
        return (rc_wgsl_sema) {0};
    }
}
