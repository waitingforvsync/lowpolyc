// glsl_expr.c
//
// Types, values and expressions in GLSL. Every operation is written in parentheses, so GLSL's
// precedence never comes into it.
//
// An expression of pointer type is written as the memory it points at: GLSL has no pointers, and
// passes pointer parameters as inout instead. So &x is just x, and so is *p for p = &x.

#include "glsl_writer.h"

// ---- types ----

// True if type is a vector.
static bool rc_glsl_is_vector(const rc_writer *w, uint32_t type)
{
    return rc_writer_typeinfo(w, type).type == RC_WGSL_TYPEINFO_TYPE_VECTOR;
}


// The kind of scalar type is, or holds.
static rc_wgsl_typeinfo_type rc_glsl_scalar_type(const rc_writer *w, uint32_t type)
{
    return rc_writer_typeinfo(w, rc_wgsl_type_scalar_of(w->types.infos.view, type)).type;
}


// The GLSL name of a concrete scalar of type.
static rc_str rc_glsl_scalar_name(rc_wgsl_typeinfo_type type)
{
    switch (type) {
    case RC_WGSL_TYPEINFO_TYPE_BOOL:
        return RC_STR("bool");
    case RC_WGSL_TYPEINFO_TYPE_I32:
    case RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT:
        return RC_STR("int");
    case RC_WGSL_TYPEINFO_TYPE_U32:
        return RC_STR("uint");
    default:
        return RC_STR("float");
    }
}


// The letter GLSL puts before vec for a vector of type.
static rc_str rc_glsl_vector_prefix(rc_wgsl_typeinfo_type type)
{
    switch (type) {
    case RC_WGSL_TYPEINFO_TYPE_BOOL:
        return RC_STR("b");
    case RC_WGSL_TYPEINFO_TYPE_I32:
    case RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT:
        return RC_STR("i");
    case RC_WGSL_TYPEINFO_TYPE_U32:
        return RC_STR("u");
    default:
        return RC_STR("");
    }
}


// Append an array type: its innermost element, then each size from the outside in.
static void rc_glsl_write_array_type(rc_writer *w, rc_mstr *out, uint32_t type, uint32_t node, rc_arena *arena)
{
    uint32_t element = type;
    uint32_t depth = 0;

    while (rc_writer_typeinfo(w, element).type == RC_WGSL_TYPEINFO_TYPE_ARRAY) {
        element = rc_writer_typeinfo(w, element).element;
        depth++;
    }
    if (depth > 1 && w->target == RC_SHADER_TARGET_GLSL_ES300) {
        rc_writer_fail_target(w, node, RC_STR("arrays of arrays"), arena);
    }

    rc_glsl_write_type(w, out, element, node, arena);
    for (element = type; rc_writer_typeinfo(w, element).type == RC_WGSL_TYPEINFO_TYPE_ARRAY;
        element = rc_writer_typeinfo(w, element).element) {
        rc_mstr_append_char(out, '[', arena);
        if (rc_writer_typeinfo(w, element).count != 0) {
            rc_mstr_append_u32(out, rc_writer_typeinfo(w, element).count, arena);
        }

        rc_mstr_append_char(out, ']', arena);
    }
}


void rc_glsl_write_type(rc_writer *w, rc_mstr *out, uint32_t type, uint32_t node, rc_arena *arena)
{
    rc_wgsl_typeinfo t = rc_writer_typeinfo(w, type);

    switch ((rc_wgsl_typeinfo_type)t.type) {
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
        rc_mstr_append(out, rc_glsl_vector_prefix(rc_writer_typeinfo(w, t.element).type), arena);
        rc_mstr_append(out, RC_STR("vec"), arena);
        rc_mstr_append_u32(out, t.width, arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
        rc_mstr_append(out, RC_STR("mat"), arena);
        rc_mstr_append_u32(out, t.columns, arena);
        if (t.columns != t.rows) {
            rc_mstr_append_char(out, 'x', arena);
            rc_mstr_append_u32(out, t.rows, arena);
        }

        break;
    case RC_WGSL_TYPEINFO_TYPE_ARRAY:
        rc_glsl_write_array_type(w, out, type, node, arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_STRUCT:
        rc_glsl_append_name(out, rc_wgsl_ast_text(w->module.ast, t.decl), arena);
        break;
    default:
        rc_mstr_append(out, rc_glsl_scalar_name((rc_wgsl_typeinfo_type)t.type), arena);
        break;
    }
}


void rc_glsl_write_declaration(rc_writer *w, rc_mstr *out, uint32_t type, rc_str name, uint32_t node, rc_arena *arena)
{
    rc_glsl_write_type(w, out, type, node, arena);
    rc_mstr_append_char(out, ' ', arena);
    rc_glsl_append_name(out, name, arena);
}


// Append each field type's zero value of a struct, separated by commas.
static void rc_glsl_write_struct_zeros(rc_writer *w, rc_mstr *out, uint32_t decl, uint32_t node, rc_arena *arena)
{
    rc_view_wgsl_index fields = rc_wgsl_ast_struct_fields(w->module.ast, decl);

    for (uint32_t i = 0; i < fields.num; i++) {
        if (i > 0) {
            rc_mstr_append(out, RC_STR(", "), arena);
        }

        rc_glsl_write_zero(w, out, rc_writer_sema(w, rc_view_wgsl_index_get(fields, i)).type, node, arena);
    }
}


void rc_glsl_write_zero(rc_writer *w, rc_mstr *out, uint32_t type, uint32_t node, rc_arena *arena)
{
    rc_wgsl_typeinfo t = rc_writer_typeinfo(w, type);

    switch ((rc_wgsl_typeinfo_type)t.type) {
    case RC_WGSL_TYPEINFO_TYPE_BOOL:
        rc_mstr_append(out, RC_STR("false"), arena);
        return;
    case RC_WGSL_TYPEINFO_TYPE_I32:
        rc_mstr_append(out, RC_STR("0"), arena);
        return;
    case RC_WGSL_TYPEINFO_TYPE_U32:
        rc_mstr_append(out, RC_STR("0u"), arena);
        return;
    case RC_WGSL_TYPEINFO_TYPE_F32:
        rc_mstr_append(out, RC_STR("0.0"), arena);
        return;
    default:
        break;
    }
    rc_glsl_write_type(w, out, type, node, arena);
    rc_mstr_append_char(out, '(', arena);
    if (t.type == RC_WGSL_TYPEINFO_TYPE_ARRAY) {
        for (uint32_t i = 0; i < t.count; i++) {
            if (i > 0) {
                rc_mstr_append(out, RC_STR(", "), arena);
            }

            rc_glsl_write_zero(w, out, t.element, node, arena);
        }
    }
    else if (t.type == RC_WGSL_TYPEINFO_TYPE_STRUCT) {
        rc_glsl_write_struct_zeros(w, out, t.decl, node, arena);
    }
    else {
        rc_glsl_write_zero(w, out, t.element, node, arena);
    }

    rc_mstr_append_char(out, ')', arena);
}

// ---- literals ----

// Append an integer or bool value as a literal of type.
static void rc_glsl_write_value(rc_mstr *out, int64_t value, rc_wgsl_typeinfo_type type, rc_arena *arena)
{
    switch (type) {
    case RC_WGSL_TYPEINFO_TYPE_BOOL:
        rc_mstr_append(out, value != 0 ? RC_STR("true") : RC_STR("false"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_U32:
        rc_mstr_append_u64(out, (uint64_t)value, arena);
        rc_mstr_append_char(out, 'u', arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_F32:
        rc_mstr_append(out, value < 0 ? RC_STR("(") : RC_STR(""), arena);
        rc_mstr_append_i64(out, value, arena);
        rc_mstr_append(out, value < 0 ? RC_STR(".0)") : RC_STR(".0"), arena);
        break;
    default:
        // The most negative i32 has no literal: its magnitude doesn't fit
        if (value == INT32_MIN) {
            rc_mstr_append(out, RC_STR("(-2147483647 - 1)"), arena);
        }
        else if (value < 0) {
            rc_mstr_append_char(out, '(', arena);
            rc_mstr_append_i64(out, value, arena);
            rc_mstr_append_char(out, ')', arena);
        }
        else {
            rc_mstr_append_i64(out, value, arena);
        }

        break;
    }
}

// ---- expressions ----

// Write a name used as a value. A pointer let is the memory it points at, and an abstract const
// its value, in place.
static void rc_glsl_write_ident(rc_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    uint32_t decl = rc_writer_sema(w, node).target;
    rc_wgsl_node d = rc_wgsl_ast_node(w->module.ast, decl);
    uint32_t type = rc_writer_sema(w, decl).type;

    if (d.type == RC_WGSL_NODE_TYPE_LET && rc_writer_typeinfo(w, type).type == RC_WGSL_TYPEINFO_TYPE_POINTER) {
        rc_glsl_write_expr(w, d.rhs, scalar, arena);
        return;
    }

    if (d.type == RC_WGSL_NODE_TYPE_CONST && rc_wgsl_type_is_abstract(w->types.infos.view, type)) {
        rc_writer_put(w, RC_STR("("), arena);
        rc_glsl_write_expr(w, d.rhs, rc_wgsl_type_leaf_scalar(w->types.infos.view, rc_writer_expr_type(w, node, scalar, arena)), arena);
        rc_writer_put(w, RC_STR(")"), arena);
        return;
    }

    rc_glsl_append_name(&w->code, rc_wgsl_ast_text(w->module.ast, node), arena);
}


// Write the expression node as a value of type, spreading a scalar across a vector if type is one.
static void rc_glsl_write_as(rc_writer *w, uint32_t node, uint32_t scalar, uint32_t type, rc_arena *arena)
{
    uint32_t own = rc_writer_expr_type(w, node, scalar, arena);

    if (rc_glsl_is_vector(w, type) && !rc_glsl_is_vector(w, own)) {
        rc_glsl_write_type(w, &w->code, type, node, arena);
        rc_writer_put(w, RC_STR("("), arena);
        rc_glsl_write_expr(w, node, scalar, arena);
        rc_writer_put(w, RC_STR(")"), arena);
        return;
    }

    rc_glsl_write_expr(w, node, scalar, arena);
}


// Write "(left op right)".
static void rc_glsl_write_infix(rc_writer *w, uint32_t left, rc_str op, uint32_t right, uint32_t scalar, rc_arena *arena)
{
    rc_writer_put(w, RC_STR("("), arena);
    rc_glsl_write_expr(w, left, scalar, arena);
    rc_writer_put(w, RC_STR(" "), arena);
    rc_writer_put(w, op, arena);
    rc_writer_put(w, RC_STR(" "), arena);
    rc_glsl_write_expr(w, right, scalar, arena);
    rc_writer_put(w, RC_STR(")"), arena);
}


// Write "name(left, right)", for the vector comparisons.
static void rc_glsl_write_prefix(rc_writer *w, rc_str name, uint32_t left, uint32_t right, uint32_t scalar, rc_arena *arena)
{
    rc_writer_put(w, name, arena);
    rc_writer_put(w, RC_STR("("), arena);
    rc_glsl_write_expr(w, left, scalar, arena);
    rc_writer_put(w, RC_STR(", "), arena);
    rc_glsl_write_expr(w, right, scalar, arena);
    rc_writer_put(w, RC_STR(")"), arena);
}


// Write a call to polyfill for type, on left and right, each spread to type if a scalar.
static void rc_glsl_write_polyfill_binary(rc_writer *w, rc_glsl_polyfill polyfill, uint32_t left,
    uint32_t right, uint32_t type, uint32_t scalar, rc_arena *arena)
{
    rc_glsl_write_polyfill_call(w, polyfill, type, left, arena);
    rc_glsl_write_as(w, left, scalar, type, arena);
    rc_writer_put(w, RC_STR(", "), arena);
    rc_glsl_write_as(w, right, scalar, type, arena);
    rc_writer_put(w, RC_STR(")"), arena);
}


// The vector comparison function for op, as lessThan for <.
static rc_str rc_glsl_vector_comparison(rc_wgsl_token_type op)
{
    switch (op) {
    case RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL:
        return RC_STR("equal");
    case RC_WGSL_TOKEN_TYPE_BANG_EQUAL:
        return RC_STR("notEqual");
    case RC_WGSL_TOKEN_TYPE_LESS:
        return RC_STR("lessThan");
    case RC_WGSL_TOKEN_TYPE_LESS_EQUAL:
        return RC_STR("lessThanEqual");
    case RC_WGSL_TOKEN_TYPE_GREATER:
        return RC_STR("greaterThan");
    default:
        return RC_STR("greaterThanEqual");
    }
}


// The text of a binary operator, as GLSL writes it too.
static rc_str rc_glsl_operator_text(rc_wgsl_token_type op)
{
    switch (op) {
    case RC_WGSL_TOKEN_TYPE_PLUS:
        return RC_STR("+");
    case RC_WGSL_TOKEN_TYPE_MINUS:
        return RC_STR("-");
    case RC_WGSL_TOKEN_TYPE_STAR:
        return RC_STR("*");
    case RC_WGSL_TOKEN_TYPE_SLASH:
        return RC_STR("/");
    case RC_WGSL_TOKEN_TYPE_PERCENT:
        return RC_STR("%");
    case RC_WGSL_TOKEN_TYPE_AND:
        return RC_STR("&");
    case RC_WGSL_TOKEN_TYPE_OR:
        return RC_STR("|");
    case RC_WGSL_TOKEN_TYPE_XOR:
        return RC_STR("^");
    case RC_WGSL_TOKEN_TYPE_SHIFT_LEFT:
        return RC_STR("<<");
    case RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT:
        return RC_STR(">>");
    case RC_WGSL_TOKEN_TYPE_AND_AND:
        return RC_STR("&&");
    case RC_WGSL_TOKEN_TYPE_OR_OR:
        return RC_STR("||");
    case RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL:
        return RC_STR("==");
    case RC_WGSL_TOKEN_TYPE_BANG_EQUAL:
        return RC_STR("!=");
    case RC_WGSL_TOKEN_TYPE_LESS:
        return RC_STR("<");
    case RC_WGSL_TOKEN_TYPE_LESS_EQUAL:
        return RC_STR("<=");
    case RC_WGSL_TOKEN_TYPE_GREATER:
        return RC_STR(">");
    default:
        return RC_STR(">=");
    }
}


// True if division or remainder by the expression node needs no guard: it is a constant other
// than 0 and -1.
static bool rc_glsl_is_safe_divisor(const rc_writer *w, uint32_t node)
{
    rc_wgsl_sema sema = rc_writer_sema(w, node);

    if (!(sema.flags & RC_WGSL_SEMA_VALUE)) {
        return false;
    }

    return sema.value != 0 && sema.value != -1;
}


// True if integer / or % by the expression right means the same in GLSL as in WGSL: right is a
// constant that needs no guard, and a remainder is unsigned.
static bool rc_glsl_is_plain_division(const rc_writer *w, bool remainder, uint32_t right, rc_wgsl_typeinfo_type scalar_type)
{
    if (!rc_glsl_is_safe_divisor(w, right)) {
        return false;
    }

    return !remainder || scalar_type == RC_WGSL_TYPEINFO_TYPE_U32;
}


// Write a shift: WGSL takes the amount modulo 32, where GLSL leaves larger amounts undefined.
static void rc_glsl_write_shift(rc_writer *w, rc_wgsl_token_type op, uint32_t left, uint32_t right, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_sema amount = rc_writer_sema(w, right);
    uint32_t amount_type = rc_writer_expr_type(w, right, 0, arena);

    if ((amount.flags & RC_WGSL_SEMA_VALUE) && amount.value < 32) {
        rc_glsl_write_infix(w, left, rc_glsl_operator_text(op), right, scalar, arena);
        return;
    }

    rc_writer_put(w, RC_STR("("), arena);
    rc_glsl_write_expr(w, left, scalar, arena);
    rc_writer_put(w, RC_STR(" "), arena);
    rc_writer_put(w, rc_glsl_operator_text(op), arena);
    rc_writer_put(w, RC_STR(" ("), arena);
    rc_glsl_write_expr(w, right, scalar, arena);
    rc_writer_put(w, RC_STR(" & "), arena);
    if (rc_glsl_is_vector(w, amount_type)) {
        rc_glsl_write_type(w, &w->code, amount_type, right, arena);
        rc_writer_put(w, RC_STR("(31u)))"), arena);
    }
    else {
        rc_writer_put(w, RC_STR("31u))"), arena);
    }
}


// Write / or %, guarded where WGSL defines what GLSL leaves undefined: integer division by zero,
// the most negative i32 over -1, and the remainder of negative integers.
static void rc_glsl_write_division(rc_writer *w, rc_wgsl_token_type op, uint32_t left,
    uint32_t right, uint32_t type, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_typeinfo_type scalar_type = rc_glsl_scalar_type(w, type);
    bool remainder = op == RC_WGSL_TOKEN_TYPE_PERCENT;

    if (scalar_type == RC_WGSL_TYPEINFO_TYPE_F32 && !remainder) {
        rc_glsl_write_infix(w, left, RC_STR("/"), right, scalar, arena);
    }
    else if (scalar_type == RC_WGSL_TYPEINFO_TYPE_F32) {
        rc_glsl_write_polyfill_binary(w, RC_GLSL_POLYFILL_FMOD, left, right, type, scalar, arena);
    }
    else if (rc_glsl_is_plain_division(w, remainder, right, scalar_type)) {
        rc_glsl_write_infix(w, left, remainder ? RC_STR("%") : RC_STR("/"), right, scalar, arena);
    }
    else {
        rc_glsl_write_polyfill_binary(w, remainder ? RC_GLSL_POLYFILL_MOD : RC_GLSL_POLYFILL_DIV, left, right, type, scalar, arena);
    }
}


void rc_glsl_write_operation(rc_writer *w, rc_wgsl_token_type op, uint32_t left,
    uint32_t right, uint32_t type, uint32_t scalar, rc_arena *arena)
{
    uint32_t left_type = rc_writer_expr_type(w, left, scalar, arena);
    bool vector = rc_glsl_is_vector(w, left_type);
    bool boolean = rc_wgsl_type_scalar_of(w->types.infos.view, left_type) == RC_WGSL_TYPE_ID_BOOL;

    switch (op) {
    case RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL:
    case RC_WGSL_TOKEN_TYPE_BANG_EQUAL:
    case RC_WGSL_TOKEN_TYPE_LESS:
    case RC_WGSL_TOKEN_TYPE_LESS_EQUAL:
    case RC_WGSL_TOKEN_TYPE_GREATER:
    case RC_WGSL_TOKEN_TYPE_GREATER_EQUAL:
        if (vector) {
            rc_glsl_write_prefix(w, rc_glsl_vector_comparison(op), left, right, scalar, arena);
            return;
        }

        break;
    case RC_WGSL_TOKEN_TYPE_AND:
    case RC_WGSL_TOKEN_TYPE_OR:
        if (boolean) {
            rc_glsl_write_polyfill_binary(w, op == RC_WGSL_TOKEN_TYPE_AND ? RC_GLSL_POLYFILL_AND : RC_GLSL_POLYFILL_OR,
                left, right, type, scalar, arena);
            return;
        }

        break;
    case RC_WGSL_TOKEN_TYPE_SHIFT_LEFT:
    case RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT:
        rc_glsl_write_shift(w, op, left, right, scalar, arena);
        return;
    case RC_WGSL_TOKEN_TYPE_SLASH:
    case RC_WGSL_TOKEN_TYPE_PERCENT:
        rc_glsl_write_division(w, op, left, right, type, scalar, arena);
        return;
    default:
        break;
    }
    rc_glsl_write_infix(w, left, rc_glsl_operator_text(op), right, scalar, arena);
}


// Write a binary operator.
static void rc_glsl_write_binary(rc_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    rc_wgsl_token_type op = (rc_wgsl_token_type)rc_view_wgsl_token_get(w->module.ast.tokens, n.token).type;

    rc_glsl_write_operation(w, op, n.lhs, n.rhs, rc_writer_expr_type(w, node, scalar, arena), scalar, arena);
}


// Write a unary operator. & and * leave no trace: see the top of this file.
static void rc_glsl_write_unary(rc_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    rc_wgsl_token_type op = (rc_wgsl_token_type)rc_view_wgsl_token_get(w->module.ast.tokens, n.token).type;
    uint32_t type = rc_writer_expr_type(w, n.lhs, scalar, arena);

    if (op == RC_WGSL_TOKEN_TYPE_AND || op == RC_WGSL_TOKEN_TYPE_STAR) {
        rc_glsl_write_expr(w, n.lhs, scalar, arena);
        return;
    }

    if (op == RC_WGSL_TOKEN_TYPE_BANG && rc_glsl_is_vector(w, type)) {
        rc_writer_put(w, RC_STR("not("), arena);
    }
    else {
        rc_writer_put(w, RC_STR("("), arena);
        rc_writer_put(w, rc_wgsl_ast_text(w->module.ast, node), arena);
    }

    rc_glsl_write_expr(w, n.lhs, scalar, arena);
    rc_writer_put(w, RC_STR(")"), arena);
}


// Write the arguments of a call, separated by commas.
static void rc_glsl_write_args(rc_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_view_wgsl_index args = rc_wgsl_ast_call_args(w->module.ast, node);

    for (uint32_t i = 0; i < args.num; i++) {
        if (i > 0) {
            rc_writer_put(w, RC_STR(", "), arena);
        }

        rc_glsl_write_expr(w, rc_view_wgsl_index_get(args, i), scalar, arena);
    }
}


// The function that reinterprets a 32-bit scalar of type from as type to, or "" if the bits pass
// through a plain conversion between i32 and u32.
static rc_str rc_glsl_bitcast_function(rc_wgsl_typeinfo_type from, rc_wgsl_typeinfo_type to)
{
    if (from == RC_WGSL_TYPEINFO_TYPE_F32 && to == RC_WGSL_TYPEINFO_TYPE_I32) {
        return RC_STR("floatBitsToInt");
    }

    if (from == RC_WGSL_TYPEINFO_TYPE_F32 && to == RC_WGSL_TYPEINFO_TYPE_U32) {
        return RC_STR("floatBitsToUint");
    }

    if (from == RC_WGSL_TYPEINFO_TYPE_I32 && to == RC_WGSL_TYPEINFO_TYPE_F32) {
        return RC_STR("intBitsToFloat");
    }

    if (from == RC_WGSL_TYPEINFO_TYPE_U32 && to == RC_WGSL_TYPEINFO_TYPE_F32) {
        return RC_STR("uintBitsToFloat");
    }

    return RC_STR("");
}


// True if a scalar of type is an i32 or a u32.
static bool rc_glsl_is_integer(rc_wgsl_typeinfo_type type)
{
    return type == RC_WGSL_TYPEINFO_TYPE_I32 || type == RC_WGSL_TYPEINFO_TYPE_U32;
}


// Write a conversion from one scalar or vector type to another, saturating float to integer as
// WGSL does.
static void rc_glsl_write_convert(rc_writer *w, uint32_t node, uint32_t type, uint32_t scalar, rc_arena *arena)
{
    uint32_t arg = rc_view_wgsl_index_get(rc_wgsl_ast_call_args(w->module.ast, node), 0);
    uint32_t from = rc_writer_expr_type(w, arg, scalar, arena);
    rc_wgsl_typeinfo_type from_scalar_type = rc_glsl_scalar_type(w, from);
    rc_wgsl_typeinfo_type to_scalar_type = rc_glsl_scalar_type(w, type);

    if (from_scalar_type == RC_WGSL_TYPEINFO_TYPE_F32 && rc_glsl_is_integer(to_scalar_type)) {
        rc_glsl_polyfill polyfill = to_scalar_type == RC_WGSL_TYPEINFO_TYPE_I32 ? RC_GLSL_POLYFILL_F2I : RC_GLSL_POLYFILL_F2U;

        rc_glsl_write_polyfill_call(w, polyfill, from, node, arena);
    }
    else {
        rc_glsl_write_type(w, &w->code, type, node, arena);
        rc_writer_put(w, RC_STR("("), arena);
    }

    rc_glsl_write_expr(w, arg, scalar, arena);
    rc_writer_put(w, RC_STR(")"), arena);
}


// Write a bitcast.
static void rc_glsl_write_bitcast(rc_writer *w, uint32_t node, uint32_t type, rc_arena *arena)
{
    uint32_t arg = rc_view_wgsl_index_get(rc_wgsl_ast_call_args(w->module.ast, node), 0);
    uint32_t from = rc_writer_sema(w, arg).type;
    rc_str function = rc_glsl_bitcast_function(rc_glsl_scalar_type(w, from), rc_glsl_scalar_type(w, type));

    if (from == type) {
        rc_glsl_write_expr(w, arg, 0, arena);
        return;
    }

    if (rc_str_is_empty(function)) {
        rc_glsl_write_type(w, &w->code, type, node, arena);
    }
    else {
        rc_writer_put(w, function, arena);
    }

    rc_writer_put(w, RC_STR("("), arena);
    rc_glsl_write_expr(w, arg, 0, arena);
    rc_writer_put(w, RC_STR(")"), arena);
}


// True if the construction node makes a scalar of type from one of its own type, which is just
// that value.
static bool rc_glsl_is_identity_construct(rc_writer *w, uint32_t node, uint32_t type, uint32_t scalar, rc_arena *arena)
{
    uint32_t arg = rc_view_wgsl_index_get(rc_wgsl_ast_call_args(w->module.ast, node), 0);

    if (!rc_wgsl_type_is_scalar(w->types.infos.view, type)) {
        return false;
    }

    return rc_writer_expr_type(w, arg, scalar, arena) == type;
}


// Write a construction of type: its zero value, its one argument unchanged, or a call of the
// type's constructor.
static void rc_glsl_write_construct(rc_writer *w, uint32_t node, uint32_t type, uint32_t scalar, rc_arena *arena)
{
    if (rc_wgsl_ast_node(w->module.ast, node).rhs == 0) {
        rc_glsl_write_zero(w, &w->code, type, node, arena);
        return;
    }

    if (rc_glsl_is_identity_construct(w, node, type, scalar, arena)) {
        rc_glsl_write_args(w, node, scalar, arena);
        return;
    }

    rc_glsl_write_type(w, &w->code, type, node, arena);
    rc_writer_put(w, RC_STR("("), arena);
    rc_glsl_write_args(w, node, rc_wgsl_type_leaf_scalar(w->types.infos.view, type), arena);
    rc_writer_put(w, RC_STR(")"), arena);
}


// Write a call of any kind.
static void rc_glsl_write_call(rc_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_sema sema = rc_writer_sema(w, node);
    uint32_t type = rc_writer_expr_type(w, node, scalar, arena);

    switch ((rc_wgsl_call)sema.call) {
    case RC_WGSL_CALL_FUNCTION:
        rc_glsl_append_name(&w->code, rc_wgsl_ast_text(w->module.ast, sema.target), arena);
        rc_writer_put(w, RC_STR("("), arena);
        rc_glsl_write_args(w, node, scalar, arena);
        rc_writer_put(w, RC_STR(")"), arena);
        break;
    case RC_WGSL_CALL_CONSTRUCT:
        rc_glsl_write_construct(w, node, type, scalar, arena);
        break;
    case RC_WGSL_CALL_CONVERT:
        rc_glsl_write_convert(w, node, type, scalar, arena);
        break;
    case RC_WGSL_CALL_BITCAST:
        rc_glsl_write_bitcast(w, node, type, arena);
        break;
    default:
        rc_glsl_write_builtin(w, node, scalar, arena);
        break;
    }
}


// The number of elements in the vector, matrix or array type, or in what the pointer type points
// at; 0 for an array sized at run time.
static uint32_t rc_glsl_element_count(const rc_writer *w, uint32_t type)
{
    rc_wgsl_typeinfo t = rc_writer_typeinfo(w, type);

    return rc_wgsl_type_element_count(w->types.infos.view, t.type == RC_WGSL_TYPEINFO_TYPE_POINTER ? t.store_type : type);
}


// Write the index of the INDEX node n clamped within bounds, then "]": up to count - 1, or for an
// array sized at run time (count 0), up to its length - 1.
static void rc_glsl_write_clamped_index(rc_writer *w, rc_wgsl_node n, uint32_t count, uint32_t scalar, rc_arena *arena)
{
    bool unsigned_index = rc_writer_expr_type(w, n.rhs, 0, arena) == RC_WGSL_TYPE_ID_U32;

    rc_writer_put(w, unsigned_index ? RC_STR("min(") : RC_STR("clamp("), arena);
    rc_glsl_write_expr(w, n.rhs, 0, arena);
    rc_writer_put(w, unsigned_index ? RC_STR(", ") : RC_STR(", 0, "), arena);
    if (count != 0) {
        rc_mstr_append_u32(&w->code, count - 1, arena);
    }
    else {
        rc_writer_put(w, unsigned_index ? RC_STR("uint(") : RC_STR("int("), arena);
        rc_glsl_write_expr(w, n.lhs, scalar, arena);
        rc_writer_put(w, RC_STR(".length()) - 1"), arena);
    }

    rc_writer_put(w, unsigned_index ? RC_STR("u)]") : RC_STR(")]"), arena);
}


// Write base[index]. WGSL keeps an index it can't check in advance within bounds, where GLSL leaves
// what happens undefined, so such an index is clamped: against the count, or for an array sized at
// run time, against its length.
static void rc_glsl_write_index(rc_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    uint32_t count = rc_glsl_element_count(w, rc_writer_sema(w, n.lhs).type);

    rc_glsl_write_expr(w, n.lhs, scalar, arena);
    rc_writer_put(w, RC_STR("["), arena);
    if ((rc_writer_sema(w, n.rhs).flags & RC_WGSL_SEMA_VALUE) && count != 0) {
        rc_glsl_write_expr(w, n.rhs, 0, arena);
        rc_writer_put(w, RC_STR("]"), arena);
        return;
    }

    rc_glsl_write_clamped_index(w, n, count, scalar, arena);
}


// Write a member access: a struct's field, or a swizzle, whose letters GLSL shares.
static void rc_glsl_write_member(rc_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    uint32_t field = rc_writer_sema(w, node).target;

    rc_glsl_write_expr(w, n.lhs, scalar, arena);
    rc_writer_put(w, RC_STR("."), arena);
    if (field != 0) {
        rc_glsl_append_name(&w->code, rc_wgsl_ast_text(w->module.ast, node), arena);
    }
    else {
        rc_writer_put(w, rc_wgsl_ast_text(w->module.ast, node), arena);
    }
}


// True if an expression with sema and type is an integer or bool constant, which is written as its
// value, whatever made it.
static bool rc_glsl_is_written_as_value(const rc_writer *w, rc_wgsl_sema sema, uint32_t type)
{
    if (!(sema.flags & RC_WGSL_SEMA_VALUE)) {
        return false;
    }

    return rc_wgsl_type_is_scalar(w->types.infos.view, type) && type != RC_WGSL_TYPE_ID_F32;
}


void rc_glsl_write_expr(rc_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_sema sema = rc_writer_sema(w, node);
    uint32_t type = rc_writer_expr_type(w, node, scalar, arena);
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);

    if (rc_glsl_is_written_as_value(w, sema, type)) {
        rc_glsl_write_value(&w->code, sema.value, rc_writer_typeinfo(w, type).type, arena);
        return;
    }

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_INT:
        rc_glsl_write_value(&w->code, sema.value, rc_writer_typeinfo(w, type).type, arena);
        break;
    case RC_WGSL_NODE_TYPE_FLOAT:
        rc_writer_append_float(&w->code, rc_wgsl_ast_text(w->module.ast, node), arena);
        break;
    case RC_WGSL_NODE_TYPE_IDENT:
        rc_glsl_write_ident(w, node, scalar, arena);
        break;
    case RC_WGSL_NODE_TYPE_CALL:
        rc_glsl_write_call(w, node, scalar, arena);
        break;
    case RC_WGSL_NODE_TYPE_INDEX:
        rc_glsl_write_index(w, node, scalar, arena);
        break;
    case RC_WGSL_NODE_TYPE_MEMBER:
        rc_glsl_write_member(w, node, scalar, arena);
        break;
    case RC_WGSL_NODE_TYPE_UNARY:
        rc_glsl_write_unary(w, node, scalar, arena);
        break;
    case RC_WGSL_NODE_TYPE_BINARY:
        rc_glsl_write_binary(w, node, scalar, arena);
        break;
    default:
        rc_writer_fail(w, node, RC_STR("this can't be written in GLSL"));
        break;
    }
}
