// check_expr.c
//
// Expressions: their types, whether they name memory, whether they are known before the shader
// runs, and the values of the integer and bool ones.
//
// Integer constants are worked out in 64 bits and must fit their type. An abstract integer that
// overflows, or a concrete one that leaves its type's range, is an error, as WGSL has it.

#include "checker.h"

#include "richc/mstr.h"

// ---- small helpers ----

// What an expression gives: a value of type, with flags.
static rc_wgsl_sema rc_wgsl_value_of(uint32_t type, uint8_t flags)
{
    return (rc_wgsl_sema) {.type = type, .flags = flags};
}


// An integer or bool constant of type.
static rc_wgsl_sema rc_wgsl_constant_of(uint32_t type, int64_t value)
{
    return (rc_wgsl_sema) {
        .value = value,
        .type = type,
        .flags = RC_WGSL_SEMA_CONST | RC_WGSL_SEMA_VALUE
    };
}


// The flags an operation on a and b keeps: const if both are.
static uint8_t rc_wgsl_const_flags(rc_wgsl_sema a, rc_wgsl_sema b)
{
    return (uint8_t)(a.flags & b.flags & RC_WGSL_SEMA_CONST);
}


// The token type of node's operator.
static rc_wgsl_token_type rc_wgsl_operator_of(const rc_wgsl_checker *c, uint32_t node)
{
    uint32_t token = rc_wgsl_ast_node(c->ast, node).token;

    return (rc_wgsl_token_type)rc_view_wgsl_token_get(c->ast.tokens, token).type;
}


// True if type is a matrix.
static bool rc_wgsl_is_matrix(const rc_wgsl_checker *c, uint32_t type)
{
    return rc_wgsl_check_typeinfo_type(c, type) == RC_WGSL_TYPEINFO_TYPE_MATRIX;
}


// True if value is in the range of the integer type.
static bool rc_wgsl_fits(rc_wgsl_typeinfo_type type, int64_t value)
{
    switch (type) {
    case RC_WGSL_TYPEINFO_TYPE_I32:
        return value >= INT32_MIN && value <= INT32_MAX;
    case RC_WGSL_TYPEINFO_TYPE_U32:
        return value >= 0 && value <= UINT32_MAX;
    default:
        return true;
    }
}

// ---- literals ----

// An integer literal: AbstractInt, or i32 or u32 by its suffix. The lexer has checked its digits,
// so parsing only fails on range: an abstract integer holds what an i64 does.
static rc_wgsl_sema rc_wgsl_check_int(rc_wgsl_checker *c, uint32_t node)
{
    rc_str text = rc_wgsl_ast_text(c->ast, node);
    char suffix = text.data[text.len - 1];
    bool suffixed = suffix == 'i' || suffix == 'u';
    rc_parsed_i64 parsed = rc_str_as_i64(rc_str_drop_last(text, suffixed ? 1 : 0), 0);
    uint32_t type = suffix == 'i' ? RC_WGSL_TYPE_ID_I32 : suffix == 'u' ? RC_WGSL_TYPE_ID_U32 : RC_WGSL_TYPE_ID_ABSTRACT_INT;

    RC_ASSERT(parsed.error != RC_PARSE_ERROR_SYNTAX);
    if (parsed.error != RC_PARSE_ERROR_NONE || !rc_wgsl_fits(rc_wgsl_check_typeinfo_type(c, type), parsed.value)) {
        rc_wgsl_check_fail(c, node, RC_STR("this integer is too big for its type"));
    }

    return rc_wgsl_constant_of(type, parsed.value);
}


// True if the float literal text ends in a suffix. In a hex float without an exponent, a final f
// is a digit.
static bool rc_wgsl_float_suffixed(rc_str text)
{
    char last = text.data[text.len - 1];

    if (last != 'f' && last != 'h') {
        return false;
    }

    if (!rc_wgsl_is_hex_literal(text)) {
        return true;
    }

    return rc_str_contains(text, RC_STR("p")) || rc_str_contains(text, RC_STR("P"));
}


// True if the float literal's value, rounded to its type, overflows it: f32 when suffixed, and an
// abstract float holds what an f64 does. The lexer has checked its text.
static bool rc_wgsl_float_overflows(rc_str text, bool suffixed)
{
    rc_str number = rc_str_drop_last(text, suffixed ? 1 : 0);

    if (suffixed) {
        return rc_str_as_f32(number, 0).error == RC_PARSE_ERROR_RANGE;
    }
    return rc_str_as_f64(number, 0).error == RC_PARSE_ERROR_RANGE;
}


// A floating-point literal: AbstractFloat, or f32 by its suffix.
static rc_wgsl_sema rc_wgsl_check_float(rc_wgsl_checker *c, uint32_t node)
{
    rc_str text = rc_wgsl_ast_text(c->ast, node);
    bool suffixed = rc_wgsl_float_suffixed(text);

    if (suffixed && text.data[text.len - 1] == 'h') {
        rc_wgsl_check_fail(c, node, RC_STR("f16 isn't supported yet"));
    }

    if (rc_wgsl_float_overflows(text, suffixed)) {
        rc_wgsl_check_fail(c, node, RC_STR("this float is too big for its type"));
    }

    return rc_wgsl_value_of(suffixed ? RC_WGSL_TYPE_ID_F32 : RC_WGSL_TYPE_ID_ABSTRACT_FLOAT, RC_WGSL_SEMA_CONST);
}

// ---- names ----

// What a use at node of decl gives, where declared is what decl itself gives.
static rc_wgsl_sema rc_wgsl_use_of(rc_wgsl_checker *c, uint32_t node, uint32_t decl, rc_wgsl_sema declared, rc_arena *arena)
{
    switch (rc_wgsl_ast_type(c->ast, decl)) {
    case RC_WGSL_NODE_TYPE_VAR:
        return (rc_wgsl_sema) {
            .type = declared.type,
            .target = decl,
            .flags = RC_WGSL_SEMA_REF,
            .space = declared.space,
            .access = declared.access
        };
    case RC_WGSL_NODE_TYPE_CONST:
    case RC_WGSL_NODE_TYPE_LET:
    case RC_WGSL_NODE_TYPE_PARAM:
        declared.target = decl;
        return declared;
    case RC_WGSL_NODE_TYPE_FN:
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is a function: call it with ( and )"), arena);
        return declared;
    default:
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is a type, not a value"), arena);
        return declared;
    }
}


// What a use of the declaration found gives. A global is checked first, if it hasn't been yet.
static rc_wgsl_sema rc_wgsl_check_use(rc_wgsl_checker *c, uint32_t node, rc_wgsl_found found, rc_arena *arena)
{
    if (found.global) {
        rc_wgsl_check_global(c, found.node, node, arena);
    }

    return rc_wgsl_use_of(c, node, found.node, rc_wgsl_check_sema(c, found.node), arena);
}


// A name used as a value.
static rc_wgsl_sema rc_wgsl_check_ident(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_found found = rc_wgsl_check_lookup(c, rc_wgsl_ast_text(c->ast, node));

    if (rc_wgsl_ast_node(c->ast, node).lhs != 0) {
        return rc_wgsl_check_fail(c, node, RC_STR("a type can't be used as a value"));
    }

    if (found.node != 0) {
        return rc_wgsl_check_use(c, node, found, arena);
    }

    switch ((rc_wgsl_predeclared_type)found.predeclared.type) {
    case RC_WGSL_PREDECLARED_TYPE_GENERATOR:
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is a type, not a value"), arena);
        break;
    case RC_WGSL_PREDECLARED_TYPE_FUNCTION:
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is a function: call it with ( and )"), arena);
        break;
    case RC_WGSL_PREDECLARED_TYPE_ENUMERANT:
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' only goes in a template list"), arena);
        break;
    case RC_WGSL_PREDECLARED_TYPE_UNSUPPORTED:
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' isn't supported yet"), arena);
        break;
    default:
        rc_wgsl_check_fail_name(c, node, RC_STR("there is nothing called '"), RC_STR("'"), arena);
        break;
    }
    return (rc_wgsl_sema) {0};
}

// ---- indexing and members ----

// What base gives when its parts are reached: a pointer is followed to the memory it points at.
static rc_wgsl_sema rc_wgsl_composite_base(const rc_wgsl_checker *c, rc_wgsl_sema base)
{
    rc_wgsl_typeinfo type = rc_wgsl_check_typeinfo(c, base.type);

    if (type.type != RC_WGSL_TYPEINFO_TYPE_POINTER) {
        return base;
    }

    return (rc_wgsl_sema) {
        .type = type.store_type,
        .flags = RC_WGSL_SEMA_REF,
        .space = type.space,
        .access = type.access
    };
}


// What a part of base gives, of type: memory if base is, and const if base is. A field names
// itself as target; anything else gives 0.
static rc_wgsl_sema rc_wgsl_part_of(rc_wgsl_sema base, uint32_t type, bool keeps_ref, uint32_t target)
{
    uint8_t ref = keeps_ref ? (uint8_t)(base.flags & RC_WGSL_SEMA_REF) : 0;

    return (rc_wgsl_sema) {
        .type = type,
        .target = target,
        .flags = (uint8_t)(ref | (base.flags & RC_WGSL_SEMA_CONST)),
        .space = ref ? base.space : 0,
        .access = ref ? base.access : 0
    };
}


// The type an element of an indexable type has, or 0.
static uint32_t rc_wgsl_element_type(rc_wgsl_checker *c, uint32_t type, rc_arena *arena)
{
    rc_wgsl_typeinfo t = rc_wgsl_check_typeinfo(c, type);

    switch ((rc_wgsl_typeinfo_type)t.type) {
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
    case RC_WGSL_TYPEINFO_TYPE_ARRAY:
        return t.element;
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
        return rc_wgsl_type_column(&c->types, type, arena);
    default:
        return 0;
    }
}


// True if index is a value known already that falls outside count elements. A count of 0 is
// unknown, so only a negative index is out.
static bool rc_wgsl_out_of_bounds(rc_wgsl_sema index, uint32_t count)
{
    if (!(index.flags & RC_WGSL_SEMA_VALUE)) {
        return false;
    }

    if (index.value < 0) {
        return true;
    }

    return count != 0 && index.value >= count;
}


// base[index].
static rc_wgsl_sema rc_wgsl_check_index(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_sema base = rc_wgsl_composite_base(c, rc_wgsl_check_expr(c, n.lhs, arena));
    rc_wgsl_sema index = rc_wgsl_check_expr(c, n.rhs, arena);
    uint32_t element = rc_wgsl_element_type(c, base.type, arena);
    uint32_t count = rc_wgsl_type_element_count(c->types.infos.view, base.type);

    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }

    if (element == 0) {
        rc_wgsl_check_fail_type(c, n.lhs, RC_STR("can't index "), base.type, RC_STR(""), arena);
    }
    else if (!rc_wgsl_type_is_integer_scalar(c->types.infos.view, index.type)) {
        rc_wgsl_check_fail_type(c, n.rhs, RC_STR("an index must be an integer, not "), index.type, RC_STR(""), arena);
    }
    else if (rc_wgsl_out_of_bounds(index, count)) {
        rc_wgsl_check_fail(c, n.rhs, RC_STR("this index is out of bounds"));
    }

    rc_wgsl_check_concretise_default(c, n.rhs, arena);
    return rc_wgsl_part_of(base, element, true, 0);
}


// The position in its set of a swizzle letter: xyzw or rgba, numbered from 0 within each.
static uint32_t rc_wgsl_swizzle_letter(char letter, bool rgba)
{
    rc_str set = rgba ? RC_STR("rgba") : RC_STR("xyzw");

    for (uint32_t i = 0; i < 4; i++) {
        if (set.data[i] == letter) {
            return i;
        }
    }
    return RC_INDEX_NONE;
}


// A swizzle of the vector base, as v.xy or v.rgba.
static rc_wgsl_sema rc_wgsl_check_swizzle(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema base, rc_arena *arena)
{
    rc_str name = rc_wgsl_ast_text(c->ast, node);
    rc_wgsl_typeinfo vector = rc_wgsl_check_typeinfo(c, base.type);
    bool rgba = rc_wgsl_swizzle_letter(name.data[0], true) != RC_INDEX_NONE;

    if (name.len > 4) {
        return rc_wgsl_check_fail(c, node, RC_STR("a swizzle has at most four letters"));
    }

    for (uint32_t i = 0; i < name.len; i++) {
        uint32_t component = rc_wgsl_swizzle_letter(name.data[i], rgba);

        if (component == RC_INDEX_NONE || component >= vector.width) {
            return rc_wgsl_check_fail_type(c, node, RC_STR("this isn't a swizzle of "), base.type, RC_STR(""), arena);
        }
    }

    // A single letter names memory, but several make a new value
    if (name.len == 1) {
        return rc_wgsl_part_of(base, vector.element, true, 0);
    }

    return rc_wgsl_part_of(base, rc_wgsl_type_vector(&c->types, vector.element, name.len, arena), false, 0);
}


// The field of the struct type named by node, or 0.
static uint32_t rc_wgsl_find_field(const rc_wgsl_checker *c, uint32_t type, uint32_t node)
{
    uint32_t decl = rc_wgsl_check_typeinfo(c, type).decl;
    rc_view_wgsl_index fields = rc_wgsl_ast_struct_fields(c->ast, decl);

    for (uint32_t i = 0; i < fields.num; i++) {
        uint32_t field = rc_view_wgsl_index_get(fields, i);

        if (rc_str_is_equal(rc_wgsl_ast_text(c->ast, field), rc_wgsl_ast_text(c->ast, node))) {
            return field;
        }
    }
    return 0;
}


// The field of the struct base named by node, or fail if it has none.
static rc_wgsl_sema rc_wgsl_check_member_field(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema base, rc_arena *arena)
{
    uint32_t field = rc_wgsl_find_field(c, base.type, node);

    if (field == 0) {
        rc_mstr after = rc_mstr_from_str(RC_STR(" has no member called '"), 64, arena);

        rc_mstr_append(&after, rc_wgsl_ast_text(c->ast, node), arena);
        rc_mstr_append(&after, RC_STR("'"), arena);
        return rc_wgsl_check_fail_type(c, node, RC_STR(""), base.type, after.view, arena);
    }

    return rc_wgsl_part_of(base, rc_wgsl_check_sema(c, field).type, true, field);
}


// base.member: a struct's field, or a vector's swizzle.
static rc_wgsl_sema rc_wgsl_check_member(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_sema base = rc_wgsl_composite_base(c, rc_wgsl_check_expr(c, n.lhs, arena));
    rc_wgsl_typeinfo_type type = rc_wgsl_check_typeinfo_type(c, base.type);

    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }

    if (type == RC_WGSL_TYPEINFO_TYPE_VECTOR) {
        return rc_wgsl_check_swizzle(c, node, base, arena);
    }

    if (type != RC_WGSL_TYPEINFO_TYPE_STRUCT) {
        return rc_wgsl_check_fail_type(c, node, RC_STR(""), base.type, RC_STR(" has no members"), arena);
    }

    return rc_wgsl_check_member_field(c, node, base, arena);
}

// ---- unary operators ----

// True if type can be negated: a scalar or vector of a numeric type other than u32.
static bool rc_wgsl_negatable(const rc_wgsl_checker *c, uint32_t type)
{
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.infos.view, type);

    if (!rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, type)) {
        return false;
    }

    return rc_wgsl_type_is_numeric_scalar(c->types.infos.view, scalar) && scalar != RC_WGSL_TYPE_ID_U32;
}


// -operand, on a signed scalar or vector.
static rc_wgsl_sema rc_wgsl_check_negate(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema operand, rc_arena *arena)
{
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.infos.view, operand.type);
    rc_wgsl_typeinfo_type type = rc_wgsl_check_typeinfo_type(c, scalar);
    rc_wgsl_sema result = rc_wgsl_value_of(operand.type, operand.flags & RC_WGSL_SEMA_CONST);

    if (!rc_wgsl_negatable(c, operand.type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("can't negate "), operand.type, RC_STR(""), arena);
    }
    else if (operand.flags & RC_WGSL_SEMA_VALUE) {
        if (operand.value == INT64_MIN || !rc_wgsl_fits(type, -operand.value)) {
            rc_wgsl_check_fail(c, node, RC_STR("this overflows"));
        }

        result = rc_wgsl_constant_of(operand.type, -operand.value);
    }

    return result;
}


// !operand, on a bool or a vector of them.
static rc_wgsl_sema rc_wgsl_check_not(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema operand, rc_arena *arena)
{
    if (rc_wgsl_type_scalar_of(c->types.infos.view, operand.type) != RC_WGSL_TYPE_ID_BOOL || rc_wgsl_is_matrix(c, operand.type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("'!' needs a bool, not "), operand.type, RC_STR(""), arena);
    }

    if (operand.flags & RC_WGSL_SEMA_VALUE) {
        return rc_wgsl_constant_of(operand.type, !operand.value);
    }

    return rc_wgsl_value_of(operand.type, operand.flags & RC_WGSL_SEMA_CONST);
}


// ~operand, on an integer or a vector of them.
static rc_wgsl_sema rc_wgsl_check_complement(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema operand, rc_arena *arena)
{
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.infos.view, operand.type);

    if (!rc_wgsl_type_is_integer_scalar(c->types.infos.view, scalar) || rc_wgsl_is_matrix(c, operand.type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("'~' needs an integer, not "), operand.type, RC_STR(""), arena);
    }

    if (operand.flags & RC_WGSL_SEMA_VALUE) {
        int64_t value = ~operand.value;

        return rc_wgsl_constant_of(operand.type, scalar == RC_WGSL_TYPE_ID_U32 ? (int64_t)((uint64_t)value & UINT32_MAX) : value);
    }

    return rc_wgsl_value_of(operand.type, operand.flags & RC_WGSL_SEMA_CONST);
}


// True if node is a member of a vector: a component, which has no address of its own.
static bool rc_wgsl_is_component(const rc_wgsl_checker *c, uint32_t node)
{
    if (rc_wgsl_ast_type(c->ast, node) != RC_WGSL_NODE_TYPE_MEMBER) {
        return false;
    }

    return rc_wgsl_check_typeinfo_type(c, rc_wgsl_check_sema(c, rc_wgsl_ast_node(c->ast, node).lhs).type) == RC_WGSL_TYPEINFO_TYPE_VECTOR;
}


// &operand, the address of memory.
static rc_wgsl_sema rc_wgsl_check_address_of(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema operand, rc_arena *arena)
{
    bool component = rc_wgsl_is_component(c, rc_wgsl_ast_node(c->ast, node).lhs);
    uint32_t pointer = rc_wgsl_type_pointer(&c->types, operand.type, (rc_wgsl_space)operand.space, (rc_wgsl_access)operand.access, arena);

    if (!(operand.flags & RC_WGSL_SEMA_REF)) {
        rc_wgsl_check_fail(c, node, RC_STR("only memory has an address: a var, or part of one"));
    }
    else if (component || operand.space == RC_WGSL_SPACE_HANDLE) {
        rc_wgsl_check_fail(c, node, RC_STR("can't take the address of this"));
    }

    return rc_wgsl_value_of(pointer, 0);
}


// *operand, the memory a pointer points at.
static rc_wgsl_sema rc_wgsl_check_deref(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema operand, rc_arena *arena)
{
    if (rc_wgsl_check_typeinfo_type(c, operand.type) != RC_WGSL_TYPEINFO_TYPE_POINTER) {
        return rc_wgsl_check_fail_type(c, node, RC_STR("'*' needs a pointer, not "), operand.type, RC_STR(""), arena);
    }

    return rc_wgsl_composite_base(c, operand);
}


// A unary operator and its operand.
static rc_wgsl_sema rc_wgsl_check_unary(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_sema operand = rc_wgsl_check_expr(c, rc_wgsl_ast_node(c->ast, node).lhs, arena);

    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }

    switch (rc_wgsl_operator_of(c, node)) {
    case RC_WGSL_TOKEN_TYPE_MINUS:
        return rc_wgsl_check_negate(c, node, operand, arena);
    case RC_WGSL_TOKEN_TYPE_BANG:
        return rc_wgsl_check_not(c, node, operand, arena);
    case RC_WGSL_TOKEN_TYPE_TILDE:
        return rc_wgsl_check_complement(c, node, operand, arena);
    case RC_WGSL_TOKEN_TYPE_AND:
        return rc_wgsl_check_address_of(c, node, operand, arena);
    default:
        return rc_wgsl_check_deref(c, node, operand, arena);
    }
}

// ---- integer constants ----

// True if a + b overflows 64 bits.
static bool rc_wgsl_add_overflows(int64_t a, int64_t b)
{
    return b > 0 ? a > INT64_MAX - b : a < INT64_MIN - b;
}


// True if a - b overflows 64 bits.
static bool rc_wgsl_subtract_overflows(int64_t a, int64_t b)
{
    return b < 0 ? a > INT64_MAX + b : a < INT64_MIN + b;
}


// True if a * b overflows 64 bits.
static bool rc_wgsl_multiply_overflows(int64_t a, int64_t b)
{
    if (a == 0 || b == 0) {
        return false;
    }

    if (a > 0) {
        return b > 0 ? a > INT64_MAX / b : b < INT64_MIN / a;
    }

    return b > 0 ? a < INT64_MIN / b : a < INT64_MAX / b;
}


// True if a << b shifts too far for an integer of type, or loses bits off the top.
static bool rc_wgsl_shift_overflows(int64_t a, int64_t b, rc_wgsl_typeinfo_type type)
{
    if (b >= (type == RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT ? 63 : 32)) {
        return true;
    }

    return a != 0 && ((a << b) >> b) != a;
}


// a op b for integers, or fail at node if it overflows or divides by zero. The result must fit
// type, the operands' type.
static int64_t rc_wgsl_fold_int(rc_wgsl_checker *c, uint32_t node, rc_wgsl_token_type op, int64_t a, int64_t b, rc_wgsl_typeinfo_type type)
{
    int64_t result = 0;
    bool bad = false;

    switch (op) {
    case RC_WGSL_TOKEN_TYPE_PLUS:
        bad = rc_wgsl_add_overflows(a, b);
        result = bad ? 0 : a + b;
        break;
    case RC_WGSL_TOKEN_TYPE_MINUS:
        bad = rc_wgsl_subtract_overflows(a, b);
        result = bad ? 0 : a - b;
        break;
    case RC_WGSL_TOKEN_TYPE_STAR:
        bad = rc_wgsl_multiply_overflows(a, b);
        result = bad ? 0 : a * b;
        break;
    case RC_WGSL_TOKEN_TYPE_SLASH:
    case RC_WGSL_TOKEN_TYPE_PERCENT:
        if (b == 0) {
            rc_wgsl_check_fail(c, node, RC_STR("this divides by zero"));
            return 0;
        }

        bad = a == INT64_MIN && b == -1;
        result = bad ? 0 : op == RC_WGSL_TOKEN_TYPE_SLASH ? a / b : a % b;
        break;
    case RC_WGSL_TOKEN_TYPE_AND:
        result = a & b;
        break;
    case RC_WGSL_TOKEN_TYPE_OR:
        result = a | b;
        break;
    case RC_WGSL_TOKEN_TYPE_XOR:
        result = a ^ b;
        break;
    case RC_WGSL_TOKEN_TYPE_SHIFT_LEFT:
        bad = rc_wgsl_shift_overflows(a, b, type);
        result = bad ? 0 : (int64_t)((uint64_t)a << b);
        break;
    case RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT:
        bad = b >= (type == RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT ? 64 : 32);
        result = bad ? 0 : a >> b;
        break;
    default:
        RC_UNREACHABLE();
    }
    if (bad || !rc_wgsl_fits(type, result)) {
        rc_wgsl_check_fail(c, node, RC_STR("this overflows"));
    }

    return result;
}


// a op b for a comparison of integers or bools.
static int64_t rc_wgsl_fold_comparison(rc_wgsl_token_type op, int64_t a, int64_t b)
{
    switch (op) {
    case RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL:
        return a == b;
    case RC_WGSL_TOKEN_TYPE_BANG_EQUAL:
        return a != b;
    case RC_WGSL_TOKEN_TYPE_LESS:
        return a < b;
    case RC_WGSL_TOKEN_TYPE_LESS_EQUAL:
        return a <= b;
    case RC_WGSL_TOKEN_TYPE_GREATER:
        return a > b;
    default:
        return a >= b;
    }
}

// ---- binary operators ----

// Settle each abstract operand of a binary node on scalar, keeping its shape. A shift's amount
// settles on u32 instead.
static void rc_wgsl_concretise_operands(rc_wgsl_checker *c, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_token_type op = rc_wgsl_operator_of(c, node);
    bool shift = op == RC_WGSL_TOKEN_TYPE_SHIFT_LEFT || op == RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT;
    uint32_t left = rc_wgsl_check_sema(c, n.lhs).type;
    uint32_t right = rc_wgsl_check_sema(c, n.rhs).type;

    if (rc_wgsl_type_is_abstract(c->types.infos.view, scalar)) {
        return;
    }

    rc_wgsl_check_concretise(c, n.lhs, rc_wgsl_type_with_scalar(&c->types, left, scalar, arena), arena);
    rc_wgsl_check_concretise(c, n.rhs, rc_wgsl_type_with_scalar(&c->types, right, shift ? RC_WGSL_TYPE_ID_U32 : scalar, arena), arena);
}


// The type of a product involving a matrix, of scalar elements, or 0 if there is none.
static uint32_t rc_wgsl_matrix_product_type(rc_wgsl_checker *c, uint32_t left, uint32_t right, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_typeinfo l = rc_wgsl_check_typeinfo(c, left);
    rc_wgsl_typeinfo r = rc_wgsl_check_typeinfo(c, right);

    if (l.type == RC_WGSL_TYPEINFO_TYPE_MATRIX && r.type == RC_WGSL_TYPEINFO_TYPE_MATRIX) {
        return l.columns == r.rows ? rc_wgsl_type_matrix(&c->types, scalar, r.columns, l.rows, arena) : 0;
    }

    if (l.type == RC_WGSL_TYPEINFO_TYPE_MATRIX && r.type == RC_WGSL_TYPEINFO_TYPE_VECTOR) {
        return l.columns == r.width ? rc_wgsl_type_vector(&c->types, scalar, l.rows, arena) : 0;
    }

    if (l.type == RC_WGSL_TYPEINFO_TYPE_VECTOR && r.type == RC_WGSL_TYPEINFO_TYPE_MATRIX) {
        return l.width == r.rows ? rc_wgsl_type_vector(&c->types, scalar, r.columns, arena) : 0;
    }

    if (rc_wgsl_type_is_scalar(c->types.infos.view, left)) {
        return rc_wgsl_type_with_scalar(&c->types, right, scalar, arena);
    }

    return rc_wgsl_type_is_scalar(c->types.infos.view, right) ? rc_wgsl_type_with_scalar(&c->types, left, scalar, arena) : 0;
}


// True if left and right are the same kind of type with the same columns and rows: matrices that
// + and - can take.
static bool rc_wgsl_same_matrix_shape(const rc_wgsl_checker *c, uint32_t left, uint32_t right)
{
    rc_wgsl_typeinfo l = rc_wgsl_check_typeinfo(c, left);
    rc_wgsl_typeinfo r = rc_wgsl_check_typeinfo(c, right);

    if (l.type != r.type) {
        return false;
    }

    return l.columns == r.columns && l.rows == r.rows;
}


// The type of arithmetic involving a matrix, or 0 if there is none.
static uint32_t rc_wgsl_matrix_arithmetic_type(rc_wgsl_checker *c, rc_wgsl_token_type op,
    uint32_t left, uint32_t right, uint32_t scalar, rc_arena *arena)
{
    if (!rc_wgsl_type_is_float_scalar(c->types.infos.view, scalar)) {
        return 0;
    }

    if (op == RC_WGSL_TOKEN_TYPE_STAR) {
        return rc_wgsl_matrix_product_type(c, left, right, scalar, arena);
    }

    if (op != RC_WGSL_TOKEN_TYPE_PLUS && op != RC_WGSL_TOKEN_TYPE_MINUS) {
        return 0;
    }

    return rc_wgsl_same_matrix_shape(c, left, right) ? rc_wgsl_type_with_scalar(&c->types, left, scalar, arena) : 0;
}


// True if left and right are scalars or vectors that arithmetic can pair: a scalar with either,
// or two vectors of one width.
static bool rc_wgsl_arithmetic_shapes_match(const rc_wgsl_checker *c, uint32_t left, uint32_t right)
{
    uint32_t left_width = rc_wgsl_type_width_of(c->types.infos.view, left);
    uint32_t right_width = rc_wgsl_type_width_of(c->types.infos.view, right);

    if (!rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, left) || !rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, right)) {
        return false;
    }

    if (left_width > 1 && right_width > 1) {
        return left_width == right_width;
    }

    return true;
}


// The type of left op right for + - * / %, or 0 if the operator can't take them.
static uint32_t rc_wgsl_arithmetic_type(rc_wgsl_checker *c, rc_wgsl_token_type op, uint32_t left, uint32_t right, rc_arena *arena)
{
    uint32_t scalar = rc_wgsl_type_common(c->types.infos.view, rc_wgsl_type_scalar_of(c->types.infos.view, left),
        rc_wgsl_type_scalar_of(c->types.infos.view, right));
    uint32_t left_width = rc_wgsl_type_width_of(c->types.infos.view, left);
    uint32_t right_width = rc_wgsl_type_width_of(c->types.infos.view, right);

    if (scalar == 0 || !rc_wgsl_type_is_numeric_scalar(c->types.infos.view, scalar)) {
        return 0;
    }

    if (rc_wgsl_is_matrix(c, left) || rc_wgsl_is_matrix(c, right)) {
        return rc_wgsl_matrix_arithmetic_type(c, op, left, right, scalar, arena);
    }

    if (!rc_wgsl_arithmetic_shapes_match(c, left, right)) {
        return 0;
    }

    if (left_width == 1 && right_width == 1) {
        return scalar;
    }

    return rc_wgsl_type_vector(&c->types, scalar, left_width > right_width ? left_width : right_width, arena);
}


// The type of left op right for a comparison, or 0 if it can't compare them. Both operands
// settle on a common type.
static uint32_t rc_wgsl_comparison_type(rc_wgsl_checker *c, uint32_t node, rc_wgsl_token_type op,
    uint32_t left, uint32_t right, rc_arena *arena)
{
    uint32_t common = rc_wgsl_type_common(c->types.infos.view, left, right);
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.infos.view, common);
    bool equality = op == RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL || op == RC_WGSL_TOKEN_TYPE_BANG_EQUAL;

    if (common == 0 || !rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, common)) {
        return 0;
    }

    if (!equality && !rc_wgsl_type_is_numeric_scalar(c->types.infos.view, scalar)) {
        return 0;
    }

    // The result is a concrete bool, so abstract operands settle here, on their default type
    rc_wgsl_concretise_operands(c, node, rc_wgsl_type_concretise(&c->types, scalar, arena), arena);
    return rc_wgsl_type_with_scalar(&c->types, common, RC_WGSL_TYPE_ID_BOOL, arena);
}


// The type of left op right for & | ^, or 0: integers, or bools for & and |, of the same shape.
static uint32_t rc_wgsl_bitwise_type(rc_wgsl_checker *c, rc_wgsl_token_type op, uint32_t left, uint32_t right)
{
    uint32_t common = rc_wgsl_type_common(c->types.infos.view, left, right);
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.infos.view, common);
    bool boolean = scalar == RC_WGSL_TYPE_ID_BOOL && op != RC_WGSL_TOKEN_TYPE_XOR;

    if (common == 0 || !rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, common)) {
        return 0;
    }

    return boolean || rc_wgsl_type_is_integer_scalar(c->types.infos.view, scalar) ? common : 0;
}


// The type of left shifted by right, or 0: an integer, by u32 of the same shape.
static uint32_t rc_wgsl_shift_type(rc_wgsl_checker *c, uint32_t left, uint32_t right)
{
    uint32_t right_scalar = rc_wgsl_type_scalar_of(c->types.infos.view, right);

    if (!rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, left) || !rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, right)) {
        return 0;
    }

    if (!rc_wgsl_type_is_integer_scalar(c->types.infos.view, rc_wgsl_type_scalar_of(c->types.infos.view, left))) {
        return 0;
    }

    if (rc_wgsl_type_width_of(c->types.infos.view, left) != rc_wgsl_type_width_of(c->types.infos.view, right)) {
        return 0;
    }

    return right_scalar == RC_WGSL_TYPE_ID_U32 || right_scalar == RC_WGSL_TYPE_ID_ABSTRACT_INT ? left : 0;
}


uint32_t rc_wgsl_check_operation_type(rc_wgsl_checker *c, rc_wgsl_token_type op, uint32_t left, uint32_t right, rc_arena *arena)
{
    switch (op) {
    case RC_WGSL_TOKEN_TYPE_AND:
    case RC_WGSL_TOKEN_TYPE_OR:
    case RC_WGSL_TOKEN_TYPE_XOR:
        return rc_wgsl_bitwise_type(c, op, left, right);
    case RC_WGSL_TOKEN_TYPE_SHIFT_LEFT:
    case RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT:
        return rc_wgsl_shift_type(c, left, right);
    default:
        return rc_wgsl_arithmetic_type(c, op, left, right, arena);
    }
}


// The type of left op right for any binary operator, or 0 if it can't take them.
static uint32_t rc_wgsl_binary_type(rc_wgsl_checker *c, uint32_t node, rc_wgsl_token_type op,
    uint32_t left, uint32_t right, rc_arena *arena)
{
    switch (op) {
    case RC_WGSL_TOKEN_TYPE_AND_AND:
    case RC_WGSL_TOKEN_TYPE_OR_OR:
        return left == RC_WGSL_TYPE_ID_BOOL && right == RC_WGSL_TYPE_ID_BOOL ? RC_WGSL_TYPE_ID_BOOL : 0;
    case RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL:
    case RC_WGSL_TOKEN_TYPE_BANG_EQUAL:
    case RC_WGSL_TOKEN_TYPE_LESS:
    case RC_WGSL_TOKEN_TYPE_LESS_EQUAL:
    case RC_WGSL_TOKEN_TYPE_GREATER:
    case RC_WGSL_TOKEN_TYPE_GREATER_EQUAL:
        return rc_wgsl_comparison_type(c, node, op, left, right, arena);
    default:
        return rc_wgsl_check_operation_type(c, op, left, right, arena);
    }
}


// The value of a binary node whose operands are both integer or bool constants.
static rc_wgsl_sema rc_wgsl_fold_binary(rc_wgsl_checker *c, uint32_t node, rc_wgsl_token_type op,
    rc_wgsl_sema left, rc_wgsl_sema right, uint32_t type)
{
    rc_wgsl_typeinfo_type operand_type = rc_wgsl_check_typeinfo_type(c, left.type);

    switch (op) {
    case RC_WGSL_TOKEN_TYPE_AND_AND:
        return rc_wgsl_constant_of(type, left.value && right.value);
    case RC_WGSL_TOKEN_TYPE_OR_OR:
        return rc_wgsl_constant_of(type, left.value || right.value);
    case RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL:
    case RC_WGSL_TOKEN_TYPE_BANG_EQUAL:
    case RC_WGSL_TOKEN_TYPE_LESS:
    case RC_WGSL_TOKEN_TYPE_LESS_EQUAL:
    case RC_WGSL_TOKEN_TYPE_GREATER:
    case RC_WGSL_TOKEN_TYPE_GREATER_EQUAL:
        return rc_wgsl_constant_of(type, rc_wgsl_fold_comparison(op, left.value, right.value));
    default:
        // A mix of abstract and concrete folds in the concrete type
        if (operand_type == RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT) {
            operand_type = rc_wgsl_check_typeinfo_type(c, type);
        }

        return rc_wgsl_constant_of(type, rc_wgsl_fold_int(c, node, op, left.value, right.value, operand_type));
    }
}


// Fail at the binary node, whose operator can't take left and right.
static rc_wgsl_sema rc_wgsl_binary_mismatch(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema left, rc_wgsl_sema right, rc_arena *arena)
{
    rc_mstr message = {0};

    rc_mstr_append(&message, RC_STR("can't use '"), arena);
    rc_mstr_append(&message, rc_wgsl_ast_text(c->ast, node), arena);
    rc_mstr_append(&message, RC_STR("' on "), arena);
    return rc_wgsl_check_fail_types(c, node, message.view, left.type, RC_STR(" and "), right.type, arena);
}


// What the binary node gives, whose operands give left and right.
static rc_wgsl_sema rc_wgsl_binary_result(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema left, rc_wgsl_sema right, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_token_type op = rc_wgsl_operator_of(c, node);
    uint32_t type = rc_wgsl_binary_type(c, node, op, left.type, right.type, arena);

    if (type == 0) {
        return rc_wgsl_binary_mismatch(c, node, left, right, arena);
    }

    if (rc_wgsl_type_scalar_of(c->types.infos.view, type) != RC_WGSL_TYPE_ID_BOOL) {
        rc_wgsl_concretise_operands(c, node, rc_wgsl_type_scalar_of(c->types.infos.view, type), arena);
    }

    if (left.flags & right.flags & RC_WGSL_SEMA_VALUE) {
        return rc_wgsl_fold_binary(c, node, op, rc_wgsl_check_sema(c, n.lhs), rc_wgsl_check_sema(c, n.rhs), type);
    }

    return rc_wgsl_value_of(type, rc_wgsl_const_flags(left, right));
}


// A binary operator and its operands.
static rc_wgsl_sema rc_wgsl_check_binary(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_sema left = rc_wgsl_check_expr(c, n.lhs, arena);
    rc_wgsl_sema right = rc_wgsl_check_expr(c, n.rhs, arena);

    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }

    return rc_wgsl_binary_result(c, node, left, right, arena);
}

// ---- expressions ----

// Check the expression node, by its node type.
static rc_wgsl_sema rc_wgsl_check_expr_by_node_type(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    switch (rc_wgsl_ast_type(c->ast, node)) {
    case RC_WGSL_NODE_TYPE_INT:
        return rc_wgsl_check_int(c, node);
    case RC_WGSL_NODE_TYPE_FLOAT:
        return rc_wgsl_check_float(c, node);
    case RC_WGSL_NODE_TYPE_BOOL:
        return rc_wgsl_constant_of(RC_WGSL_TYPE_ID_BOOL, rc_str_is_equal(rc_wgsl_ast_text(c->ast, node), RC_STR("true")));
    case RC_WGSL_NODE_TYPE_IDENT:
        return rc_wgsl_check_ident(c, node, arena);
    case RC_WGSL_NODE_TYPE_CALL:
        return rc_wgsl_check_call(c, node, arena);
    case RC_WGSL_NODE_TYPE_INDEX:
        return rc_wgsl_check_index(c, node, arena);
    case RC_WGSL_NODE_TYPE_MEMBER:
        return rc_wgsl_check_member(c, node, arena);
    case RC_WGSL_NODE_TYPE_UNARY:
        return rc_wgsl_check_unary(c, node, arena);
    case RC_WGSL_NODE_TYPE_BINARY:
        return rc_wgsl_check_binary(c, node, arena);
    default:
        return rc_wgsl_check_fail(c, node, RC_STR("expected an expression"));
    }
}


// Note that node gives sema, unless checking it failed, and return what it gives.
static rc_wgsl_sema rc_wgsl_set_expr(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema sema)
{
    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }

    rc_wgsl_check_set(c, node, sema);
    return sema;
}


rc_wgsl_sema rc_wgsl_check_expr(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }

    return rc_wgsl_set_expr(c, node, rc_wgsl_check_expr_by_node_type(c, node, arena));
}


uint32_t rc_wgsl_check_value(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_sema sema = rc_wgsl_check_expr(c, node, arena);

    if (!rc_wgsl_check_failed(c) && sema.type == 0) {
        rc_wgsl_check_fail(c, node, RC_STR("this gives no value"));
    }

    return sema.type;
}

// ---- settling abstract types ----

// Settle child, if abstract, on scalar, keeping its shape.
static void rc_wgsl_concretise_child(rc_wgsl_checker *c, uint32_t child, uint32_t scalar, rc_arena *arena)
{
    uint32_t type = rc_wgsl_check_sema(c, child).type;

    if (child != 0 && rc_wgsl_type_is_abstract(c->types.infos.view, type)) {
        rc_wgsl_check_concretise(c, child, rc_wgsl_type_with_scalar(&c->types, type, scalar, arena), arena);
    }
}


// Settle the parts of node, which has just settled on a type with scalar elements.
static void rc_wgsl_concretise_parts(rc_wgsl_checker *c, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_UNARY:
    case RC_WGSL_NODE_TYPE_MEMBER:
    case RC_WGSL_NODE_TYPE_INDEX:
        rc_wgsl_concretise_child(c, n.lhs, scalar, arena);
        break;
    case RC_WGSL_NODE_TYPE_BINARY:
        rc_wgsl_concretise_operands(c, node, scalar, arena);
        break;
    case RC_WGSL_NODE_TYPE_CALL: {
        rc_view_wgsl_index args = rc_wgsl_ast_list(c->ast, n.rhs);

        for (uint32_t i = 0; i < args.num; i++) {
            rc_wgsl_concretise_child(c, rc_view_wgsl_index_get(args, i), scalar, arena);
        }
        break;
    }
    default:
        break;
    }
}


void rc_wgsl_check_concretise(rc_wgsl_checker *c, uint32_t node, uint32_t type, rc_arena *arena)
{
    rc_wgsl_sema sema = rc_wgsl_check_sema(c, node);
    uint32_t scalar = rc_wgsl_type_leaf_scalar(c->types.infos.view, type);

    if (rc_wgsl_check_failed(c)) {
        return;
    }

    if (!rc_wgsl_type_is_abstract(c->types.infos.view, sema.type) || sema.type == type) {
        return;
    }

    if (!rc_wgsl_type_converts(c->types.infos.view, sema.type, type)) {
        rc_wgsl_check_fail_types(c, node, RC_STR("can't convert "), sema.type, RC_STR(" to "), type, arena);
        return;
    }

    if ((sema.flags & RC_WGSL_SEMA_VALUE) && !rc_wgsl_fits(rc_wgsl_check_typeinfo_type(c, type), sema.value)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("this is out of range for "), type, RC_STR(""), arena);
        return;
    }

    rc_wgsl_check_set_type(c, node, type);

    // A value worked out already is converted whole, as WGSL does abstract constants: its parts
    // stay abstract, and may not even fit the type on their own
    if (!(sema.flags & RC_WGSL_SEMA_VALUE)) {
        rc_wgsl_concretise_parts(c, node, scalar, arena);
    }
}


void rc_wgsl_check_concretise_default(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    uint32_t type = rc_wgsl_check_sema(c, node).type;

    rc_wgsl_check_concretise(c, node, rc_wgsl_type_concretise(&c->types, type, arena), arena);
}


void rc_wgsl_check_convert(rc_wgsl_checker *c, uint32_t node, uint32_t type, rc_arena *arena)
{
    uint32_t from = rc_wgsl_check_sema(c, node).type;

    if (rc_wgsl_check_failed(c) || from == type) {
        return;
    }

    if (!rc_wgsl_type_converts(c->types.infos.view, from, type)) {
        rc_wgsl_check_fail_types(c, node, RC_STR("expected "), type, RC_STR(", got "), from, arena);
        return;
    }

    rc_wgsl_check_concretise(c, node, type, arena);
}
