// check_call.c
//
// Calls: to the shader's own functions, and to type names, which construct or convert values.
// Built-in functions have their own file, check_builtin.c.

#include "checker.h"

#include "richc/mstr.h"

// ---- arguments ----

// The type of argument i of args.
uint32_t rc_wgsl_check_arg_type(const rc_wgsl_checker *c, rc_view_wgsl_index args, uint32_t i)
{
    return rc_wgsl_check_sema(c, rc_view_wgsl_index_get(args, i)).type;
}


// The flags every one of args has.
uint8_t rc_wgsl_check_args_flags(const rc_wgsl_checker *c, rc_view_wgsl_index args)
{
    uint8_t flags = RC_WGSL_SEMA_CONST;

    for (uint32_t i = 0; i < args.num; i++) {
        flags &= rc_wgsl_check_sema(c, rc_view_wgsl_index_get(args, i)).flags;
    }
    return flags;
}


// Fail at node, which takes expected arguments but was given given, naming what takes them.
static void rc_wgsl_fail_arg_count(rc_wgsl_checker *c, uint32_t node, uint32_t given, uint32_t expected, rc_arena *arena)
{
    rc_mstr message = rc_mstr_from_str(RC_STR("'"), 64, arena);

    rc_mstr_append(&message, rc_wgsl_ast_text(c->ast, node), arena);
    rc_mstr_append(&message, RC_STR("' takes "), arena);
    rc_mstr_append_u32(&message, expected, arena);
    rc_mstr_append(&message, expected == 1 ? RC_STR(" argument, not ") : RC_STR(" arguments, not "), arena);
    rc_mstr_append_u32(&message, given, arena);
    rc_wgsl_check_fail(c, node, message.view);
}


// Fail at node unless there are expected arguments, naming what takes them.
static bool rc_wgsl_check_arg_count(rc_wgsl_checker *c, uint32_t node, rc_view_wgsl_index args, uint32_t expected, rc_arena *arena)
{
    if (args.num == expected) {
        return true;
    }

    rc_wgsl_fail_arg_count(c, node, args.num, expected, arena);
    return false;
}


// Convert the arguments from first up to last to type.
void rc_wgsl_check_convert_args(rc_wgsl_checker *c, rc_view_wgsl_index args, uint32_t first, uint32_t last, uint32_t type, rc_arena *arena)
{
    for (uint32_t i = first; i < last; i++) {
        rc_wgsl_check_convert(c, rc_view_wgsl_index_get(args, i), type, arena);
    }
}

// ---- the shader's own functions ----

// True if any of args is a pointer.
static bool rc_wgsl_has_pointer_arg(const rc_wgsl_checker *c, rc_view_wgsl_index args)
{
    for (uint32_t i = 0; i < args.num; i++) {
        if (rc_wgsl_check_typeinfo_type(c, rc_wgsl_check_arg_type(c, args, i)) == RC_WGSL_TYPEINFO_TYPE_POINTER) {
            return true;
        }
    }
    return false;
}


// A call to the function fn.
static rc_wgsl_sema rc_wgsl_check_function_call(rc_wgsl_checker *c, uint32_t node, uint32_t fn, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_view_wgsl_index params = rc_wgsl_ast_params(c->ast, fn);

    rc_wgsl_check_global(c, fn, node, arena);
    if (!rc_wgsl_check_arg_count(c, node, args, params.num, arena)) {
        return (rc_wgsl_sema) {0};
    }

    for (uint32_t i = 0; i < args.num; i++) {
        uint32_t param = rc_view_wgsl_index_get(params, i);

        rc_wgsl_check_convert(c, rc_view_wgsl_index_get(args, i), rc_wgsl_check_sema(c, param).type, arena);
    }
    if (rc_wgsl_check_sema(c, fn).stage != RC_SHADER_STAGE_NONE) {
        rc_wgsl_check_fail(c, node, RC_STR("an entry point can't be called"));
    }

    if (rc_wgsl_has_pointer_arg(c, args)) {
        rc_array_wgsl_index_push(&c->pointer_calls, node, arena);
    }

    return (rc_wgsl_sema) {
        .type = rc_wgsl_check_sema(c, fn).type,
        .target = fn,
        .call = RC_WGSL_CALL_FUNCTION
    };
}

// ---- constructing values ----

// What a call to a constructor gives: a value of type, const if its arguments are.
static rc_wgsl_sema rc_wgsl_constructed(const rc_wgsl_checker *c, uint32_t type, rc_wgsl_call call, rc_view_wgsl_index args)
{
    return (rc_wgsl_sema) {
        .type = type,
        .flags = rc_wgsl_check_args_flags(c, args),
        .call = (uint8_t)call
    };
}


// The zero value of type, as T().
static rc_wgsl_sema rc_wgsl_check_zero_value(rc_wgsl_checker *c, uint32_t node, uint32_t type, rc_view_wgsl_index args, rc_arena *arena)
{
    bool folds = rc_wgsl_type_is_scalar(c->types.infos.view, type) && type != RC_WGSL_TYPE_ID_F32;

    if (!rc_wgsl_type_is_constructible(c->types.infos.view, type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("can't construct "), type, RC_STR(""), arena);
    }

    return (rc_wgsl_sema) {
        .type = type,
        .flags = (uint8_t)(rc_wgsl_check_args_flags(c, args) | (folds ? RC_WGSL_SEMA_VALUE : 0)),
        .call = RC_WGSL_CALL_CONSTRUCT
    };
}


// The integer or bool value v of type from, converted to the integer or bool type to.
static int64_t rc_wgsl_convert_value(const rc_wgsl_checker *c, uint32_t from, uint32_t to, int64_t v)
{
    rc_wgsl_typeinfo_type from_type = rc_wgsl_check_typeinfo_type(c, from);
    rc_wgsl_typeinfo_type to_type = rc_wgsl_check_typeinfo_type(c, to);

    if (to_type == RC_WGSL_TYPEINFO_TYPE_BOOL) {
        return v != 0;
    }

    if (to_type == RC_WGSL_TYPEINFO_TYPE_I32 && from_type == RC_WGSL_TYPEINFO_TYPE_U32) {
        return (int32_t)(uint32_t)v;
    }

    if (to_type == RC_WGSL_TYPEINFO_TYPE_U32 && from_type == RC_WGSL_TYPEINFO_TYPE_I32) {
        return (uint32_t)(int32_t)v;
    }

    return v;
}


// What T(e) gives for the scalar type T, once e is checked: the same value, or a conversion if
// converts is false. Integers and bools fold; floats are left for the shader compiler.
static rc_wgsl_sema rc_wgsl_scalar_constructed(const rc_wgsl_checker *c, uint32_t type, rc_view_wgsl_index args, bool converts)
{
    rc_wgsl_sema value = rc_wgsl_check_sema(c, rc_view_wgsl_index_get(args, 0));
    bool folds = (value.flags & RC_WGSL_SEMA_VALUE) && type != RC_WGSL_TYPE_ID_F32;

    return (rc_wgsl_sema) {
        .value = folds ? rc_wgsl_convert_value(c, value.type, type, value.value) : 0,
        .type = type,
        .flags = (uint8_t)(rc_wgsl_check_args_flags(c, args) | (folds ? RC_WGSL_SEMA_VALUE : 0)),
        .call = converts ? RC_WGSL_CALL_CONSTRUCT : RC_WGSL_CALL_CONVERT
    };
}


// A scalar from one scalar: the same value, or a conversion from another scalar type.
static rc_wgsl_sema rc_wgsl_check_scalar_construct(rc_wgsl_checker *c, uint32_t node,
    uint32_t type, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t arg = rc_view_wgsl_index_get(args, 0);
    uint32_t from = rc_wgsl_check_arg_type(c, args, 0);
    bool converts = rc_wgsl_type_converts(c->types.infos.view, from, type);

    if (!rc_wgsl_check_arg_count(c, node, args, 1, arena)) {
        return (rc_wgsl_sema) {0};
    }

    if (!rc_wgsl_type_is_scalar(c->types.infos.view, from)) {
        return rc_wgsl_check_fail_types(c, arg, RC_STR("can't make "), type, RC_STR(" from "), from, arena);
    }

    if (converts) {
        rc_wgsl_check_convert(c, arg, type, arena);
    }
    else {
        rc_wgsl_check_concretise_default(c, arg, arena);
    }

    return rc_wgsl_scalar_constructed(c, type, args, converts);
}


// The number of scalars a scalar or vector holds, or 0 for anything else.
static uint32_t rc_wgsl_components(const rc_wgsl_checker *c, uint32_t type)
{
    if (!rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, type)) {
        return 0;
    }

    return rc_wgsl_type_width_of(c->types.infos.view, type);
}


// A vector from its parts, each a scalar or vector whose scalars convert to element; fails at
// node unless they add up to width.
static void rc_wgsl_check_vector_parts(rc_wgsl_checker *c, uint32_t node, uint32_t element,
    uint32_t width, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t total = 0;

    for (uint32_t i = 0; i < args.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t arg = rc_view_wgsl_index_get(args, i);
        uint32_t type = rc_wgsl_check_arg_type(c, args, i);
        uint32_t components = rc_wgsl_components(c, type);

        if (components == 0) {
            rc_wgsl_check_fail_type(c, arg, RC_STR("a vector can't be made from "), type, RC_STR(""), arena);
            return;
        }

        rc_wgsl_check_convert(c, arg, rc_wgsl_type_with_scalar(&c->types, type, element, arena), arena);
        total += components;
    }
    if (total != width && !rc_wgsl_check_failed(c)) {
        rc_wgsl_check_fail(c, node, RC_STR("these don't add up to the vector's size"));
    }
}


// True if a vector of type from becomes the vector type by converting each element, rather than
// as it is.
static bool rc_wgsl_is_vector_conversion(const rc_wgsl_checker *c, uint32_t from, uint32_t type)
{
    if (rc_wgsl_check_typeinfo_type(c, from) != RC_WGSL_TYPEINFO_TYPE_VECTOR) {
        return false;
    }

    if (rc_wgsl_type_width_of(c->types.infos.view, from) != rc_wgsl_check_typeinfo(c, type).width) {
        return false;
    }

    return !rc_wgsl_type_converts(c->types.infos.view, from, type);
}


// A vector: a splat of one scalar, a conversion of another vector, or its parts.
static rc_wgsl_sema rc_wgsl_check_vector_construct(rc_wgsl_checker *c, uint32_t node,
    uint32_t type, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_typeinfo vector = rc_wgsl_check_typeinfo(c, type);
    uint32_t from = rc_wgsl_check_arg_type(c, args, 0);

    if (args.num == 1 && rc_wgsl_type_is_scalar(c->types.infos.view, from)) {
        rc_wgsl_check_convert(c, rc_view_wgsl_index_get(args, 0), vector.element, arena);
        return rc_wgsl_constructed(c, type, RC_WGSL_CALL_CONSTRUCT, args);
    }

    if (args.num == 1 && rc_wgsl_is_vector_conversion(c, from, type)) {
        rc_wgsl_check_concretise_default(c, rc_view_wgsl_index_get(args, 0), arena);
        return rc_wgsl_constructed(c, type, RC_WGSL_CALL_CONVERT, args);
    }

    rc_wgsl_check_vector_parts(c, node, vector.element, vector.width, args, arena);
    return rc_wgsl_constructed(c, type, RC_WGSL_CALL_CONSTRUCT, args);
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
        rc_wgsl_check_convert_args(c, args, 0, args.num, rc_wgsl_type_column(&c->types, type, arena), arena);
    }
    else if (args.num == (uint32_t)matrix.columns * matrix.rows) {
        rc_wgsl_check_convert_args(c, args, 0, args.num, matrix.element, arena);
    }
    else {
        rc_wgsl_check_fail_type(c, node, RC_STR(""), type, RC_STR(" takes its columns, or all its numbers"), arena);
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
        rc_wgsl_check_convert_args(c, args, 0, args.num, array.element, arena);
    }

    return rc_wgsl_constructed(c, type, RC_WGSL_CALL_CONSTRUCT, args);
}


// A struct from its members.
static rc_wgsl_sema rc_wgsl_check_struct_construct(rc_wgsl_checker *c, uint32_t node,
    uint32_t type, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t decl = rc_wgsl_check_typeinfo(c, type).decl;
    rc_view_wgsl_index fields = rc_wgsl_ast_struct_fields(c->ast, decl);

    if (!rc_wgsl_type_is_constructible(c->types.infos.view, type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("can't construct "), type, RC_STR(""), arena);
    }
    else if (rc_wgsl_check_arg_count(c, node, args, fields.num, arena)) {
        for (uint32_t i = 0; i < args.num; i++) {
            uint32_t field = rc_view_wgsl_index_get(fields, i);

            rc_wgsl_check_convert(c, rc_view_wgsl_index_get(args, i), rc_wgsl_check_sema(c, field).type, arena);
        }
    }

    return rc_wgsl_constructed(c, type, RC_WGSL_CALL_CONSTRUCT, args);
}


// A value of type made from args.
static rc_wgsl_sema rc_wgsl_check_construct(rc_wgsl_checker *c, uint32_t node, uint32_t type, rc_view_wgsl_index args, rc_arena *arena)
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
        return rc_wgsl_check_fail_type(c, node, RC_STR("can't construct "), type, RC_STR(""), arena);
    }
}

// ---- types worked out from the arguments ----

// The type the arguments from first up to last have in common, or the scalar type if scalars; 0 if
// they have none.
uint32_t rc_wgsl_check_common_arg_type(const rc_wgsl_checker *c, rc_view_wgsl_index args, uint32_t first, uint32_t last, bool scalars)
{
    uint32_t common = 0;

    for (uint32_t i = first; i < last; i++) {
        uint32_t arg = rc_wgsl_check_arg_type(c, args, i);
        uint32_t type = scalars ? rc_wgsl_type_scalar_of(c->types.infos.view, arg) : arg;

        common = i == first ? type : rc_wgsl_type_common(c->types.infos.view, common, type);
        if (common == 0) {
            return 0;
        }
    }
    return common;
}


// The matrix type a generic matrix name such as mat2x2 makes from scalar elements, which hold
// numbers: integers are taken as floats.
static uint32_t rc_wgsl_infer_matrix_type(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_predeclared predeclared, uint32_t scalar, rc_arena *arena)
{
    uint32_t element = scalar == RC_WGSL_TYPE_ID_ABSTRACT_INT ? RC_WGSL_TYPE_ID_ABSTRACT_FLOAT : scalar;

    if (!rc_wgsl_type_is_float_scalar(c->types.infos.view, element)) {
        rc_wgsl_check_fail(c, node, RC_STR("a matrix holds f32"));
        return 0;
    }

    return rc_wgsl_type_matrix(&c->types, element, predeclared.columns, predeclared.rows, arena);
}


// The type a generic name such as vec3, mat2x2 or array makes from args.
static uint32_t rc_wgsl_infer_type(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_predeclared predeclared, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_generator generator = (rc_wgsl_generator)predeclared.generator;
    uint32_t scalar = rc_wgsl_check_common_arg_type(c, args, 0, args.num, generator != RC_WGSL_GENERATOR_ARRAY);

    if (args.num == 0) {
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' needs its element type when it has no arguments"), arena);
        return 0;
    }

    if (scalar == 0) {
        rc_wgsl_check_fail_name(c, node, RC_STR("the arguments to '"), RC_STR("' have no type in common"), arena);
        return 0;
    }

    if (generator == RC_WGSL_GENERATOR_ARRAY) {
        return rc_wgsl_type_array(&c->types, scalar, args.num, arena);
    }

    if (generator == RC_WGSL_GENERATOR_VECTOR) {
        return rc_wgsl_type_vector(&c->types, scalar, predeclared.width, arena);
    }

    return rc_wgsl_infer_matrix_type(c, node, predeclared, scalar, arena);
}


// True if a call to the predeclared type name with the template list templates works its type out
// from its arguments, as vec3(...), mat2x2(...) and array(...) do.
static bool rc_wgsl_infers_type(rc_wgsl_predeclared predeclared, uint32_t templates)
{
    if (templates != 0 || predeclared.element != 0) {
        return false;
    }

    switch ((rc_wgsl_generator)predeclared.generator) {
    case RC_WGSL_GENERATOR_VECTOR:
    case RC_WGSL_GENERATOR_MATRIX:
    case RC_WGSL_GENERATOR_ARRAY:
        return true;
    default:
        return false;
    }
}


// The type a call to the predeclared type name callee constructs, written out or worked out.
static uint32_t rc_wgsl_called_type(rc_wgsl_checker *c, uint32_t callee, rc_wgsl_predeclared predeclared,
    rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t templates = rc_wgsl_ast_node(c->ast, callee).lhs;

    if (rc_wgsl_infers_type(predeclared, templates)) {
        return rc_wgsl_infer_type(c, callee, predeclared, args, arena);
    }

    return rc_wgsl_check_generated_type(c, callee, predeclared, templates, arena);
}


// A call to a predeclared type name: a constructor, its type written out or worked out.
static rc_wgsl_sema rc_wgsl_check_type_call(rc_wgsl_checker *c, uint32_t node, uint32_t callee,
    rc_wgsl_predeclared predeclared, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t type = rc_wgsl_called_type(c, callee, predeclared, args, arena);

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
            return rc_wgsl_check_fail(c, callee, RC_STR("a function takes no template list"));
        }

        return rc_wgsl_check_function_call(c, node, decl, args, arena);
    case RC_WGSL_NODE_TYPE_STRUCT:
    case RC_WGSL_NODE_TYPE_ALIAS:
        return rc_wgsl_check_construct(c, node, rc_wgsl_check_type_expr(c, callee, arena), args, arena);
    default:
        return rc_wgsl_check_fail_name(c, callee, RC_STR("'"), RC_STR("' can't be called"), arena);
    }
}


// A call to whatever the name callee names, with its checked arguments args.
static rc_wgsl_sema rc_wgsl_check_named_call(rc_wgsl_checker *c, uint32_t node, uint32_t callee, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_found found = rc_wgsl_check_lookup(c, rc_wgsl_ast_text(c->ast, callee));

    if (found.node != 0) {
        return rc_wgsl_check_declared_call(c, node, callee, found.node, args, arena);
    }

    switch ((rc_wgsl_predeclared_type)found.predeclared.type) {
    case RC_WGSL_PREDECLARED_TYPE_GENERATOR:
        return rc_wgsl_check_type_call(c, node, callee, found.predeclared, args, arena);
    case RC_WGSL_PREDECLARED_TYPE_FUNCTION:
        return rc_wgsl_check_builtin_call(c, node, (rc_wgsl_builtin)found.predeclared.builtin, args, arena);
    case RC_WGSL_PREDECLARED_TYPE_UNSUPPORTED:
        return rc_wgsl_check_fail_name(c, callee, RC_STR("'"), RC_STR("' isn't supported yet"), arena);
    case RC_WGSL_PREDECLARED_TYPE_ENUMERANT:
        return rc_wgsl_check_fail_name(c, callee, RC_STR("'"), RC_STR("' can't be called"), arena);
    default:
        return rc_wgsl_check_fail_name(c, callee, RC_STR("there is no function called '"), RC_STR("'"), arena);
    }
}


// Check the call node: its arguments, then the call itself.
rc_wgsl_sema rc_wgsl_check_call(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_view_wgsl_index args = rc_wgsl_ast_list(c->ast, n.rhs);

    for (uint32_t i = 0; i < args.num; i++) {
        rc_wgsl_check_value(c, rc_view_wgsl_index_get(args, i), arena);
    }
    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }

    return rc_wgsl_check_named_call(c, node, n.lhs, args, arena);
}
