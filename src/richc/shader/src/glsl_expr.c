// glsl_expr.c
//
// Types, values and expressions in GLSL. Every operation is written in parentheses, so GLSL's
// precedence never comes into it.
//
// An expression of pointer type is written as the memory it points at: GLSL has no pointers, and
// passes pointer parameters as inout instead. So &x is just x, and so is *p for p = &x.

#include "glsl_writer.h"

// ---- types ----

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
static void rc_glsl_write_array_type(rc_glsl_writer *w, rc_mstr *out, uint32_t type,
    uint32_t node, rc_arena *arena)
{
    uint32_t element = type;
    uint32_t depth = 0;

    while (rc_glsl_typeinfo(w, element).type == RC_WGSL_TYPEINFO_TYPE_ARRAY) {
        element = rc_glsl_typeinfo(w, element).element;
        depth++;
    }
    if (depth > 1 && w->dialect == RC_GLSL_DIALECT_ES300) {
        rc_glsl_fail_dialect(w, node, RC_STR("arrays of arrays"), arena);
    }
    rc_glsl_write_type(w, out, element, node, arena);
    for (element = type; rc_glsl_typeinfo(w, element).type == RC_WGSL_TYPEINFO_TYPE_ARRAY;
        element = rc_glsl_typeinfo(w, element).element) {
        rc_mstr_append_char(out, '[', arena);
        if (rc_glsl_typeinfo(w, element).count != 0) {
            rc_mstr_append_u32(out, rc_glsl_typeinfo(w, element).count, arena);
        }
        rc_mstr_append_char(out, ']', arena);
    }
}


void rc_glsl_write_type(rc_glsl_writer *w, rc_mstr *out, uint32_t type, uint32_t node,
    rc_arena *arena)
{
    rc_wgsl_typeinfo t = rc_glsl_typeinfo(w, type);

    switch ((rc_wgsl_typeinfo_type)t.type) {
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
        rc_mstr_append(out, rc_glsl_vector_prefix(rc_glsl_typeinfo(w, t.element).type), arena);
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


void rc_glsl_write_declaration(rc_glsl_writer *w, rc_mstr *out, uint32_t type, rc_str name,
    uint32_t node, rc_arena *arena)
{
    rc_glsl_write_type(w, out, type, node, arena);
    rc_mstr_append_char(out, ' ', arena);
    rc_glsl_append_name(out, name, arena);
}


// Append each field type's zero value of a struct, separated by commas.
static void rc_glsl_write_struct_zeros(rc_glsl_writer *w, rc_mstr *out, uint32_t decl,
    uint32_t node, rc_arena *arena)
{
    rc_view_wgsl_index fields = rc_wgsl_ast_list(w->module.ast,
        rc_wgsl_ast_node(w->module.ast, decl).lhs);

    for (uint32_t i = 0; i < fields.num; i++) {
        if (i > 0) {
            rc_mstr_append(out, RC_STR(", "), arena);
        }
        rc_glsl_write_zero(w, out, rc_glsl_sema(w, rc_view_wgsl_index_get(fields, i)).type, node,
            arena);
    }
}


void rc_glsl_write_zero(rc_glsl_writer *w, rc_mstr *out, uint32_t type, uint32_t node,
    rc_arena *arena)
{
    rc_wgsl_typeinfo t = rc_glsl_typeinfo(w, type);

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
static void rc_glsl_write_value(rc_mstr *out, int64_t value, rc_wgsl_typeinfo_type type,
    rc_arena *arena)
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


// The value of a hex digit.
static uint32_t rc_glsl_hex_digit(char c)
{
    return c <= '9' ? (uint32_t)(c - '0') : (uint32_t)((c | 0x20) - 'a' + 10);
}


// The value of a hex float literal, without its 0x and suffix: digits, a point and a binary
// exponent. Mantissas past 53 bits lose their lowest bits.
static double rc_glsl_parse_hex_float(rc_str text)
{
    double mantissa = 0.0;
    int32_t exponent = 0;
    int32_t written = 0;
    bool after_point = false;
    bool negative = false;
    uint32_t i = 0;

    for (; i < text.len && (text.data[i] | 0x20) != 'p'; i++) {
        if (text.data[i] == '.') {
            after_point = true;
            continue;
        }
        mantissa = mantissa * 16.0 + rc_glsl_hex_digit(text.data[i]);
        exponent -= after_point ? 4 : 0;
    }
    if (i + 1 < text.len) {
        negative = text.data[i + 1] == '-';
        for (i += (text.data[i + 1] == '-' || text.data[i + 1] == '+') ? 2 : 1; i < text.len; i++) {
            written = written * 10 + (text.data[i] - '0');
            if (written > 2000) {
                written = 2000;
            }
        }
    }
    exponent += negative ? -written : written;
    for (; exponent > 0; exponent--) {
        mantissa *= 2.0;
    }
    for (; exponent < 0; exponent++) {
        mantissa *= 0.5;
    }
    return mantissa;
}


// Append a WGSL float literal as GLSL takes it: no suffix, a point or exponent always, and hex
// floats in decimal.
static void rc_glsl_write_float(rc_mstr *out, rc_str text, rc_arena *arena)
{
    bool hex = rc_str_starts_with(text, RC_STR("0x")) || rc_str_starts_with(text, RC_STR("0X"));
    rc_mstr decimal = {0};
    char last = text.data[text.len - 1];

    if (hex) {
        bool exponent = rc_str_contains(text, RC_STR("p")) || rc_str_contains(text, RC_STR("P"));
        rc_str body = rc_str_drop_first(text, 2);

        body = exponent && (last == 'f' || last == 'h') ? rc_str_drop_last(body, 1) : body;
        rc_mstr_append_f64(&decimal, rc_glsl_parse_hex_float(body),
            (rc_float_format) {.precision = 17}, arena);
        text = decimal.view;
    }
    else if (last == 'f' || last == 'h') {
        text = rc_str_drop_last(text, 1);
    }
    rc_mstr_append(out, text, arena);
    if (!rc_str_contains(text, RC_STR(".")) && !rc_str_contains(text, RC_STR("e"))
        && !rc_str_contains(text, RC_STR("E"))) {
        rc_mstr_append(out, RC_STR(".0"), arena);
    }
}

// ---- expressions ----

uint32_t rc_glsl_expr_type(rc_glsl_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    uint32_t type = rc_glsl_sema(w, node).type;

    if (!rc_wgsl_type_is_abstract(w->types.view, type)) {
        return type;
    }
    if (scalar != 0) {
        return rc_wgsl_type_with_scalar(&w->types, type, scalar, arena);
    }
    return rc_wgsl_type_concretise(&w->types, type, arena);
}


bool rc_glsl_has_calls(const rc_glsl_writer *w, uint32_t node)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_CALL: {
        rc_view_wgsl_index args = rc_wgsl_ast_list(w->module.ast, n.rhs);

        if (rc_glsl_sema(w, node).call == RC_WGSL_CALL_FUNCTION) {
            return true;
        }
        for (uint32_t i = 0; i < args.num; i++) {
            if (rc_glsl_has_calls(w, rc_view_wgsl_index_get(args, i))) {
                return true;
            }
        }
        return false;
    }
    case RC_WGSL_NODE_TYPE_INDEX:
    case RC_WGSL_NODE_TYPE_BINARY:
        return rc_glsl_has_calls(w, n.lhs) || rc_glsl_has_calls(w, n.rhs);
    case RC_WGSL_NODE_TYPE_MEMBER:
    case RC_WGSL_NODE_TYPE_UNARY:
        return rc_glsl_has_calls(w, n.lhs);
    default:
        return false;
    }
}


// Write a name used as a value. A pointer let is the memory it points at, and an abstract const
// its value, in place.
static void rc_glsl_write_ident(rc_glsl_writer *w, uint32_t node, uint32_t scalar,
    rc_arena *arena)
{
    uint32_t decl = rc_glsl_sema(w, node).target;
    rc_wgsl_node d = rc_wgsl_ast_node(w->module.ast, decl);
    uint32_t type = rc_glsl_sema(w, decl).type;

    if (d.type == RC_WGSL_NODE_TYPE_LET
        && rc_glsl_typeinfo(w, type).type == RC_WGSL_TYPEINFO_TYPE_POINTER) {
        rc_glsl_write_expr(w, d.rhs, scalar, arena);
        return;
    }
    if (d.type == RC_WGSL_NODE_TYPE_CONST && rc_wgsl_type_is_abstract(w->types.view, type)) {
        rc_glsl_put(w, RC_STR("("), arena);
        rc_glsl_write_expr(w, d.rhs, rc_wgsl_type_leaf_scalar(w->types.view,
            rc_glsl_expr_type(w, node, scalar, arena)), arena);
        rc_glsl_put(w, RC_STR(")"), arena);
        return;
    }
    rc_glsl_append_name(&w->code, rc_wgsl_ast_text(w->module.ast, node), arena);
}


// Write the expression node as a value of type, spreading a scalar across a vector if type is one.
static void rc_glsl_write_as(rc_glsl_writer *w, uint32_t node, uint32_t scalar, uint32_t type,
    rc_arena *arena)
{
    uint32_t own = rc_glsl_expr_type(w, node, scalar, arena);

    if (rc_glsl_typeinfo(w, type).type == RC_WGSL_TYPEINFO_TYPE_VECTOR
        && rc_glsl_typeinfo(w, own).type != RC_WGSL_TYPEINFO_TYPE_VECTOR) {
        rc_glsl_write_type(w, &w->code, type, node, arena);
        rc_glsl_put(w, RC_STR("("), arena);
        rc_glsl_write_expr(w, node, scalar, arena);
        rc_glsl_put(w, RC_STR(")"), arena);
        return;
    }
    rc_glsl_write_expr(w, node, scalar, arena);
}


// Write "(left op right)".
static void rc_glsl_write_infix(rc_glsl_writer *w, uint32_t left, rc_str op, uint32_t right,
    uint32_t scalar, rc_arena *arena)
{
    rc_glsl_put(w, RC_STR("("), arena);
    rc_glsl_write_expr(w, left, scalar, arena);
    rc_glsl_put(w, RC_STR(" "), arena);
    rc_glsl_put(w, op, arena);
    rc_glsl_put(w, RC_STR(" "), arena);
    rc_glsl_write_expr(w, right, scalar, arena);
    rc_glsl_put(w, RC_STR(")"), arena);
}


// Write "name(left, right)", for the vector comparisons.
static void rc_glsl_write_prefix(rc_glsl_writer *w, rc_str name, uint32_t left, uint32_t right,
    uint32_t scalar, rc_arena *arena)
{
    rc_glsl_put(w, name, arena);
    rc_glsl_put(w, RC_STR("("), arena);
    rc_glsl_write_expr(w, left, scalar, arena);
    rc_glsl_put(w, RC_STR(", "), arena);
    rc_glsl_write_expr(w, right, scalar, arena);
    rc_glsl_put(w, RC_STR(")"), arena);
}


// Write a call to polyfill for type, on left and right, each spread to type if a scalar.
static void rc_glsl_write_polyfill_binary(rc_glsl_writer *w, rc_glsl_polyfill polyfill, uint32_t left,
    uint32_t right, uint32_t type, uint32_t scalar, rc_arena *arena)
{
    rc_glsl_write_polyfill_call(w, polyfill, type, left, arena);
    rc_glsl_write_as(w, left, scalar, type, arena);
    rc_glsl_put(w, RC_STR(", "), arena);
    rc_glsl_write_as(w, right, scalar, type, arena);
    rc_glsl_put(w, RC_STR(")"), arena);
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
static bool rc_glsl_is_safe_divisor(const rc_glsl_writer *w, uint32_t node)
{
    rc_wgsl_sema sema = rc_glsl_sema(w, node);

    return (sema.flags & RC_WGSL_SEMA_VALUE) && sema.value != 0 && sema.value != -1;
}


// Write a shift: WGSL takes the amount modulo 32, where GLSL leaves larger amounts undefined.
static void rc_glsl_write_shift(rc_glsl_writer *w, rc_wgsl_token_type op, uint32_t left,
    uint32_t right, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_sema amount = rc_glsl_sema(w, right);
    uint32_t amount_type = rc_glsl_expr_type(w, right, 0, arena);

    if ((amount.flags & RC_WGSL_SEMA_VALUE) && amount.value < 32) {
        rc_glsl_write_infix(w, left, rc_glsl_operator_text(op), right, scalar, arena);
        return;
    }
    rc_glsl_put(w, RC_STR("("), arena);
    rc_glsl_write_expr(w, left, scalar, arena);
    rc_glsl_put(w, RC_STR(" "), arena);
    rc_glsl_put(w, rc_glsl_operator_text(op), arena);
    rc_glsl_put(w, RC_STR(" ("), arena);
    rc_glsl_write_expr(w, right, scalar, arena);
    rc_glsl_put(w, RC_STR(" & "), arena);
    if (rc_glsl_typeinfo(w, amount_type).type == RC_WGSL_TYPEINFO_TYPE_VECTOR) {
        rc_glsl_write_type(w, &w->code, amount_type, right, arena);
        rc_glsl_put(w, RC_STR("(31u)))"), arena);
    }
    else {
        rc_glsl_put(w, RC_STR("31u))"), arena);
    }
}


// Write / or %, guarded where WGSL defines what GLSL leaves undefined: integer division by zero,
// the most negative i32 over -1, and the remainder of negative integers.
static void rc_glsl_write_division(rc_glsl_writer *w, rc_wgsl_token_type op, uint32_t left,
    uint32_t right, uint32_t type, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_typeinfo_type scalar_type = rc_glsl_typeinfo(w, rc_wgsl_type_scalar_of(w->types.view, type)).type;
    bool remainder = op == RC_WGSL_TOKEN_TYPE_PERCENT;

    if (scalar_type == RC_WGSL_TYPEINFO_TYPE_F32 && !remainder) {
        rc_glsl_write_infix(w, left, RC_STR("/"), right, scalar, arena);
    }
    else if (scalar_type == RC_WGSL_TYPEINFO_TYPE_F32) {
        rc_glsl_write_polyfill_binary(w, RC_GLSL_POLYFILL_FMOD, left, right, type, scalar, arena);
    }
    else if (rc_glsl_is_safe_divisor(w, right) && (!remainder || scalar_type == RC_WGSL_TYPEINFO_TYPE_U32)) {
        rc_glsl_write_infix(w, left, remainder ? RC_STR("%") : RC_STR("/"), right, scalar, arena);
    }
    else {
        rc_glsl_write_polyfill_binary(w, remainder ? RC_GLSL_POLYFILL_MOD : RC_GLSL_POLYFILL_DIV, left,
            right, type, scalar, arena);
    }
}


void rc_glsl_write_operation(rc_glsl_writer *w, rc_wgsl_token_type op, uint32_t left,
    uint32_t right, uint32_t type, uint32_t scalar, rc_arena *arena)
{
    uint32_t left_type = rc_glsl_expr_type(w, left, scalar, arena);
    bool vector = rc_glsl_typeinfo(w, left_type).type == RC_WGSL_TYPEINFO_TYPE_VECTOR;
    bool boolean = rc_wgsl_type_scalar_of(w->types.view, left_type) == RC_WGSL_TYPE_ID_BOOL;

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
            rc_glsl_write_polyfill_binary(w, op == RC_WGSL_TOKEN_TYPE_AND ? RC_GLSL_POLYFILL_AND
                                                                   : RC_GLSL_POLYFILL_OR,
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
static void rc_glsl_write_binary(rc_glsl_writer *w, uint32_t node, uint32_t scalar,
    rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    rc_wgsl_token_type op = (rc_wgsl_token_type)rc_view_wgsl_token_get(w->module.ast.tokens,
        n.token).type;

    rc_glsl_write_operation(w, op, n.lhs, n.rhs, rc_glsl_expr_type(w, node, scalar, arena), scalar,
        arena);
}


// Write a unary operator. & and * leave no trace: see the top of this file.
static void rc_glsl_write_unary(rc_glsl_writer *w, uint32_t node, uint32_t scalar,
    rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    rc_wgsl_token_type op = (rc_wgsl_token_type)rc_view_wgsl_token_get(w->module.ast.tokens,
        n.token).type;
    uint32_t type = rc_glsl_expr_type(w, n.lhs, scalar, arena);

    if (op == RC_WGSL_TOKEN_TYPE_AND || op == RC_WGSL_TOKEN_TYPE_STAR) {
        rc_glsl_write_expr(w, n.lhs, scalar, arena);
        return;
    }
    if (op == RC_WGSL_TOKEN_TYPE_BANG
        && rc_glsl_typeinfo(w, type).type == RC_WGSL_TYPEINFO_TYPE_VECTOR) {
        rc_glsl_put(w, RC_STR("not("), arena);
    }
    else {
        rc_glsl_put(w, RC_STR("("), arena);
        rc_glsl_put(w, rc_wgsl_ast_text(w->module.ast, node), arena);
    }
    rc_glsl_write_expr(w, n.lhs, scalar, arena);
    rc_glsl_put(w, RC_STR(")"), arena);
}


// Write the arguments of a call, separated by commas.
static void rc_glsl_write_args(rc_glsl_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_view_wgsl_index args = rc_wgsl_ast_list(w->module.ast,
        rc_wgsl_ast_node(w->module.ast, node).rhs);

    for (uint32_t i = 0; i < args.num; i++) {
        if (i > 0) {
            rc_glsl_put(w, RC_STR(", "), arena);
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


// Write a conversion from one scalar or vector type to another, saturating float to integer as
// WGSL does.
static void rc_glsl_write_convert(rc_glsl_writer *w, uint32_t node, uint32_t type, uint32_t scalar,
    rc_arena *arena)
{
    uint32_t arg = rc_view_wgsl_index_get(rc_wgsl_ast_list(w->module.ast,
        rc_wgsl_ast_node(w->module.ast, node).rhs), 0);
    uint32_t from = rc_glsl_expr_type(w, arg, scalar, arena);
    uint32_t from_scalar = rc_wgsl_type_scalar_of(w->types.view, from);
    uint32_t to_scalar = rc_wgsl_type_scalar_of(w->types.view, type);
    rc_wgsl_typeinfo_type from_scalar_type = rc_glsl_typeinfo(w, from_scalar).type;
    rc_wgsl_typeinfo_type to_scalar_type = rc_glsl_typeinfo(w, to_scalar).type;

    if (from_scalar_type == RC_WGSL_TYPEINFO_TYPE_F32
        && (to_scalar_type == RC_WGSL_TYPEINFO_TYPE_I32 || to_scalar_type == RC_WGSL_TYPEINFO_TYPE_U32)) {
        rc_glsl_polyfill polyfill = to_scalar_type == RC_WGSL_TYPEINFO_TYPE_I32
            ? RC_GLSL_POLYFILL_F2I : RC_GLSL_POLYFILL_F2U;

        rc_glsl_write_polyfill_call(w, polyfill, from, node, arena);
    }
    else {
        rc_glsl_write_type(w, &w->code, type, node, arena);
        rc_glsl_put(w, RC_STR("("), arena);
    }
    rc_glsl_write_expr(w, arg, scalar, arena);
    rc_glsl_put(w, RC_STR(")"), arena);
}


// Write a bitcast.
static void rc_glsl_write_bitcast(rc_glsl_writer *w, uint32_t node, uint32_t type, rc_arena *arena)
{
    uint32_t arg = rc_view_wgsl_index_get(rc_wgsl_ast_list(w->module.ast,
        rc_wgsl_ast_node(w->module.ast, node).rhs), 0);
    uint32_t from = rc_glsl_sema(w, arg).type;
    rc_str function = rc_glsl_bitcast_function(
        rc_glsl_typeinfo(w, rc_wgsl_type_scalar_of(w->types.view, from)).type,
        rc_glsl_typeinfo(w, rc_wgsl_type_scalar_of(w->types.view, type)).type);

    if (from == type) {
        rc_glsl_write_expr(w, arg, 0, arena);
        return;
    }
    if (rc_str_is_empty(function)) {
        rc_glsl_write_type(w, &w->code, type, node, arena);
    }
    else {
        rc_glsl_put(w, function, arena);
    }
    rc_glsl_put(w, RC_STR("("), arena);
    rc_glsl_write_expr(w, arg, 0, arena);
    rc_glsl_put(w, RC_STR(")"), arena);
}


// Write a call of any kind.
static void rc_glsl_write_call(rc_glsl_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_sema sema = rc_glsl_sema(w, node);
    uint32_t type = rc_glsl_expr_type(w, node, scalar, arena);
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);

    switch ((rc_wgsl_call)sema.call) {
    case RC_WGSL_CALL_FUNCTION:
        rc_glsl_append_name(&w->code, rc_wgsl_ast_text(w->module.ast, sema.target), arena);
        rc_glsl_put(w, RC_STR("("), arena);
        rc_glsl_write_args(w, node, scalar, arena);
        rc_glsl_put(w, RC_STR(")"), arena);
        break;
    case RC_WGSL_CALL_CONSTRUCT:
        if (n.rhs == 0) {
            rc_glsl_write_zero(w, &w->code, type, node, arena);
            break;
        }

        // A scalar made from one of its own type is just that value
        if (rc_wgsl_type_is_scalar(w->types.view, type)
            && rc_glsl_expr_type(w, rc_view_wgsl_index_get(rc_wgsl_ast_list(w->module.ast, n.rhs), 0),
                scalar, arena) == type) {
            rc_glsl_write_args(w, node, scalar, arena);
            break;
        }
        rc_glsl_write_type(w, &w->code, type, node, arena);
        rc_glsl_put(w, RC_STR("("), arena);
        rc_glsl_write_args(w, node, rc_wgsl_type_leaf_scalar(w->types.view, type), arena);
        rc_glsl_put(w, RC_STR(")"), arena);
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


// Write a member access: a struct's field, or a swizzle, whose letters GLSL shares.
static void rc_glsl_write_member(rc_glsl_writer *w, uint32_t node, uint32_t scalar,
    rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    uint32_t field = rc_glsl_sema(w, node).target;

    rc_glsl_write_expr(w, n.lhs, scalar, arena);
    rc_glsl_put(w, RC_STR("."), arena);
    if (field != 0) {
        rc_glsl_append_name(&w->code, rc_wgsl_ast_text(w->module.ast, node), arena);
    }
    else {
        rc_glsl_put(w, rc_wgsl_ast_text(w->module.ast, node), arena);
    }
}


void rc_glsl_write_expr(rc_glsl_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_sema sema = rc_glsl_sema(w, node);
    uint32_t type = rc_glsl_expr_type(w, node, scalar, arena);
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);

    // Integer and bool constants are written as their values, whatever made them
    if ((sema.flags & RC_WGSL_SEMA_VALUE) && rc_wgsl_type_is_scalar(w->types.view, type)
        && type != RC_WGSL_TYPE_ID_F32) {
        rc_glsl_write_value(&w->code, sema.value, rc_glsl_typeinfo(w, type).type, arena);
        return;
    }
    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_INT:
        rc_glsl_write_value(&w->code, sema.value, rc_glsl_typeinfo(w, type).type, arena);
        break;
    case RC_WGSL_NODE_TYPE_FLOAT:
        rc_glsl_write_float(&w->code, rc_wgsl_ast_text(w->module.ast, node), arena);
        break;
    case RC_WGSL_NODE_TYPE_IDENT:
        rc_glsl_write_ident(w, node, scalar, arena);
        break;
    case RC_WGSL_NODE_TYPE_CALL:
        rc_glsl_write_call(w, node, scalar, arena);
        break;
    case RC_WGSL_NODE_TYPE_INDEX:
        rc_glsl_write_expr(w, n.lhs, scalar, arena);
        rc_glsl_put(w, RC_STR("["), arena);
        rc_glsl_write_expr(w, n.rhs, 0, arena);
        rc_glsl_put(w, RC_STR("]"), arena);
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
        rc_glsl_fail(w, node, RC_STR("this can't be written in GLSL"));
        break;
    }
}
