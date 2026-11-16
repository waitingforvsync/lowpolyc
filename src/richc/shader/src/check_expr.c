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


// The scalar both a and b convert to, the more specific of the two, or 0 if there is none.
static uint32_t rc_wgsl_common_type(const rc_wgsl_checker *c, uint32_t a, uint32_t b)
{
    if (rc_wgsl_type_converts(c->types.view, a, b)) {
        return b;
    }
    if (rc_wgsl_type_converts(c->types.view, b, a)) {
        return a;
    }
    return 0;
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

// The value of a decimal or hex integer's digits, or -1 if it is past what 64 bits hold.
static int64_t rc_wgsl_parse_digits(rc_str digits, uint32_t base)
{
    uint64_t value = 0;

    for (uint32_t i = 0; i < digits.len; i++) {
        char d = digits.data[i];
        uint32_t digit = d >= '0' && d <= '9' ? (uint32_t)(d - '0')
                                              : (uint32_t)((d | 0x20) - 'a' + 10);

        if (value > ((uint64_t)INT64_MAX - digit) / base) {
            return -1;
        }
        value = value * base + digit;
    }
    return (int64_t)value;
}


// An integer literal: AbstractInt, or i32 or u32 by its suffix.
static rc_wgsl_sema rc_wgsl_check_int(rc_wgsl_checker *c, uint32_t node)
{
    rc_str text = rc_wgsl_ast_text(c->ast, node);
    char suffix = text.data[text.len - 1];
    bool suffixed = suffix == 'i' || suffix == 'u';
    rc_str digits = rc_str_drop_last(text, suffixed ? 1 : 0);
    bool hex = rc_str_starts_with(digits, RC_STR("0x")) || rc_str_starts_with(digits, RC_STR("0X"));
    int64_t value = rc_wgsl_parse_digits(rc_str_drop_first(digits, hex ? 2 : 0), hex ? 16 : 10);
    uint32_t type = suffix == 'i' ? RC_WGSL_TYPE_ID_I32
                  : suffix == 'u' ? RC_WGSL_TYPE_ID_U32 : RC_WGSL_TYPE_ID_ABSTRACT_INT;

    if (value < 0 || !rc_wgsl_fits(rc_wgsl_check_typeinfo_type(c, type), value)) {
        rc_wgsl_check_fail(c, node, RC_STR("this integer is too big for its type"));
    }
    return rc_wgsl_constant_of(type, value);
}


// A floating-point literal: AbstractFloat, or f32 by its suffix.
static rc_wgsl_sema rc_wgsl_check_float(rc_wgsl_checker *c, uint32_t node)
{
    rc_str text = rc_wgsl_ast_text(c->ast, node);
    char last = text.data[text.len - 1];
    bool hex = rc_str_starts_with(text, RC_STR("0x")) || rc_str_starts_with(text, RC_STR("0X"));

    // In a hex float without an exponent, a final f is a digit
    bool suffixed = (last == 'f' || last == 'h')
        && (!hex || rc_str_contains(text, RC_STR("p")) || rc_str_contains(text, RC_STR("P")));

    if (suffixed && last == 'h') {
        rc_wgsl_check_fail(c, node, RC_STR("f16 isn't supported yet"));
    }
    return rc_wgsl_value_of(suffixed ? RC_WGSL_TYPE_ID_F32 : RC_WGSL_TYPE_ID_ABSTRACT_FLOAT,
        RC_WGSL_SEMA_CONST);
}

// ---- names ----

// What a use of the declaration found gives.
static rc_wgsl_sema rc_wgsl_check_use(rc_wgsl_checker *c, uint32_t node, rc_wgsl_found found,
    rc_arena *arena)
{
    uint32_t decl = found.node;
    rc_wgsl_node_type node_type = rc_wgsl_ast_type(c->ast, decl);
    rc_wgsl_sema declared;

    if (found.global) {
        rc_wgsl_check_global(c, decl, arena);
    }
    declared = rc_wgsl_check_sema(c, decl);
    switch (node_type) {
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
        rc_wgsl_check_fail_name(c, node, RC_STR("'"),
            RC_STR("' is a function: call it with ( and )"), arena);
        return declared;
    default:
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is a type, not a value"), arena);
        return declared;
    }
}


// A name used as a value.
static rc_wgsl_sema rc_wgsl_check_ident(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_found found = rc_wgsl_check_lookup(c, rc_wgsl_ast_text(c->ast, node));

    if (rc_wgsl_ast_node(c->ast, node).lhs != 0) {
        rc_wgsl_check_fail(c, node, RC_STR("a type can't be used as a value"));
        return (rc_wgsl_sema) {0};
    }
    if (found.node != 0) {
        return rc_wgsl_check_use(c, node, found, arena);
    }
    switch ((rc_wgsl_predeclared_type)found.predeclared.type) {
    case RC_WGSL_PREDECLARED_TYPE_GENERATOR:
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is a type, not a value"), arena);
        break;
    case RC_WGSL_PREDECLARED_TYPE_FUNCTION:
        rc_wgsl_check_fail_name(c, node, RC_STR("'"),
            RC_STR("' is a function: call it with ( and )"), arena);
        break;
    case RC_WGSL_PREDECLARED_TYPE_ENUMERANT:
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' only goes in a template list"),
            arena);
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
        .type = type.element,
        .flags = RC_WGSL_SEMA_REF,
        .space = type.space,
        .access = type.access
    };
}


// What a part of base gives, of type: memory if base is, and const if base is.
static rc_wgsl_sema rc_wgsl_part_of(rc_wgsl_sema base, uint32_t type, bool keeps_ref)
{
    uint8_t ref = keeps_ref ? (uint8_t)(base.flags & RC_WGSL_SEMA_REF) : 0;

    return (rc_wgsl_sema) {
        .type = type,
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


// The number of elements an indexable type has, or 0 if it is sized at run time.
static uint32_t rc_wgsl_element_count(const rc_wgsl_checker *c, uint32_t type)
{
    rc_wgsl_typeinfo t = rc_wgsl_check_typeinfo(c, type);

    switch ((rc_wgsl_typeinfo_type)t.type) {
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
        return t.width;
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
        return t.columns;
    default:
        return t.count;
    }
}


// base[index].
static rc_wgsl_sema rc_wgsl_check_index(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_sema base = rc_wgsl_composite_base(c, rc_wgsl_check_expr(c, n.lhs, arena));
    rc_wgsl_sema index = rc_wgsl_check_expr(c, n.rhs, arena);
    uint32_t element = rc_wgsl_element_type(c, base.type, arena);
    uint32_t count = rc_wgsl_element_count(c, base.type);

    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }
    if (element == 0) {
        rc_wgsl_check_fail_type(c, n.lhs, RC_STR("can't index "), base.type, RC_STR(""), arena);
    }
    else if (!rc_wgsl_type_is_integer_scalar(c->types.view, index.type)) {
        rc_wgsl_check_fail_type(c, n.rhs, RC_STR("an index must be an integer, not "), index.type,
            RC_STR(""), arena);
    }
    else if ((index.flags & RC_WGSL_SEMA_VALUE) && (index.value < 0
        || (count != 0 && index.value >= count))) {
        rc_wgsl_check_fail(c, n.rhs, RC_STR("this index is out of bounds"));
    }
    rc_wgsl_check_concretise_default(c, n.rhs, arena);
    return rc_wgsl_part_of(base, element, true);
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
static rc_wgsl_sema rc_wgsl_check_swizzle(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema base,
    rc_arena *arena)
{
    rc_str name = rc_wgsl_ast_text(c->ast, node);
    rc_wgsl_typeinfo vector = rc_wgsl_check_typeinfo(c, base.type);
    bool rgba = rc_wgsl_swizzle_letter(name.data[0], true) != RC_INDEX_NONE;

    if (name.len > 4) {
        rc_wgsl_check_fail(c, node, RC_STR("a swizzle has at most four letters"));
        return (rc_wgsl_sema) {0};
    }
    for (uint32_t i = 0; i < name.len; i++) {
        uint32_t component = rc_wgsl_swizzle_letter(name.data[i], rgba);

        if (component == RC_INDEX_NONE || component >= vector.width) {
            rc_wgsl_check_fail_type(c, node, RC_STR("this isn't a swizzle of "), base.type,
                RC_STR(""), arena);
            return (rc_wgsl_sema) {0};
        }
    }

    // A single letter names memory, but several make a new value
    if (name.len == 1) {
        return rc_wgsl_part_of(base, vector.element, true);
    }
    return rc_wgsl_part_of(base, rc_wgsl_type_vector(&c->types, vector.element, name.len, arena),
        false);
}


// The field of the struct type named by node, or 0.
static uint32_t rc_wgsl_find_field(const rc_wgsl_checker *c, uint32_t type, uint32_t node)
{
    uint32_t decl = rc_wgsl_check_typeinfo(c, type).decl;
    rc_view_wgsl_index fields = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, decl).lhs);

    for (uint32_t i = 0; i < fields.num; i++) {
        uint32_t field = rc_view_wgsl_index_get(fields, i);

        if (rc_str_is_equal(rc_wgsl_ast_text(c->ast, field), rc_wgsl_ast_text(c->ast, node))) {
            return field;
        }
    }
    return 0;
}


// base.member: a struct's field, or a vector's swizzle.
static rc_wgsl_sema rc_wgsl_check_member(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_sema base = rc_wgsl_composite_base(c, rc_wgsl_check_expr(c, n.lhs, arena));
    rc_wgsl_typeinfo_type type = rc_wgsl_check_typeinfo_type(c, base.type);
    uint32_t field;
    rc_wgsl_sema result;

    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }
    if (type == RC_WGSL_TYPEINFO_TYPE_VECTOR) {
        return rc_wgsl_check_swizzle(c, node, base, arena);
    }
    if (type != RC_WGSL_TYPEINFO_TYPE_STRUCT) {
        rc_wgsl_check_fail_type(c, node, RC_STR(""), base.type, RC_STR(" has no members"), arena);
        return (rc_wgsl_sema) {0};
    }
    field = rc_wgsl_find_field(c, base.type, node);
    if (field == 0) {
        rc_mstr after = rc_mstr_from_str(RC_STR(" has no member called '"), 64, arena);

        rc_mstr_append(&after, rc_wgsl_ast_text(c->ast, node), arena);
        rc_mstr_append(&after, RC_STR("'"), arena);
        rc_wgsl_check_fail_type(c, node, RC_STR(""), base.type, after.view, arena);
        return (rc_wgsl_sema) {0};
    }
    result = rc_wgsl_part_of(base, rc_wgsl_check_sema(c, field).type, true);
    result.target = field;
    return result;
}

// ---- unary operators ----

// -operand, on a signed scalar or vector.
static rc_wgsl_sema rc_wgsl_check_negate(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema operand,
    rc_arena *arena)
{
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.view, operand.type);
    rc_wgsl_typeinfo_type type = rc_wgsl_check_typeinfo_type(c, scalar);
    rc_wgsl_sema result = rc_wgsl_value_of(operand.type, operand.flags & RC_WGSL_SEMA_CONST);

    if (!rc_wgsl_type_is_scalar_or_vector(c->types.view, operand.type)
        || !rc_wgsl_type_is_numeric_scalar(c->types.view, scalar)
        || type == RC_WGSL_TYPEINFO_TYPE_U32) {
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
static rc_wgsl_sema rc_wgsl_check_not(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema operand,
    rc_arena *arena)
{
    if (rc_wgsl_type_scalar_of(c->types.view, operand.type) != RC_WGSL_TYPE_ID_BOOL
        || rc_wgsl_check_typeinfo_type(c, operand.type) == RC_WGSL_TYPEINFO_TYPE_MATRIX) {
        rc_wgsl_check_fail_type(c, node, RC_STR("'!' needs a bool, not "), operand.type,
            RC_STR(""), arena);
    }
    if (operand.flags & RC_WGSL_SEMA_VALUE) {
        return rc_wgsl_constant_of(operand.type, !operand.value);
    }
    return rc_wgsl_value_of(operand.type, operand.flags & RC_WGSL_SEMA_CONST);
}


// ~operand, on an integer or a vector of them.
static rc_wgsl_sema rc_wgsl_check_complement(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_sema operand, rc_arena *arena)
{
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.view, operand.type);

    if (!rc_wgsl_type_is_integer_scalar(c->types.view, scalar)
        || rc_wgsl_check_typeinfo_type(c, operand.type) == RC_WGSL_TYPEINFO_TYPE_MATRIX) {
        rc_wgsl_check_fail_type(c, node, RC_STR("'~' needs an integer, not "), operand.type,
            RC_STR(""), arena);
    }
    if (operand.flags & RC_WGSL_SEMA_VALUE) {
        int64_t value = ~operand.value;

        return rc_wgsl_constant_of(operand.type,
            scalar == RC_WGSL_TYPE_ID_U32 ? (int64_t)((uint64_t)value & UINT32_MAX) : value);
    }
    return rc_wgsl_value_of(operand.type, operand.flags & RC_WGSL_SEMA_CONST);
}


// &operand, the address of memory.
static rc_wgsl_sema rc_wgsl_check_address_of(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_sema operand, rc_arena *arena)
{
    uint32_t lhs = rc_wgsl_ast_node(c->ast, node).lhs;
    bool component = rc_wgsl_ast_type(c->ast, lhs) == RC_WGSL_NODE_TYPE_MEMBER
        && rc_wgsl_check_typeinfo_type(c, rc_wgsl_check_sema(c, rc_wgsl_ast_node(c->ast, lhs).lhs).type)
            == RC_WGSL_TYPEINFO_TYPE_VECTOR;

    if (!(operand.flags & RC_WGSL_SEMA_REF)) {
        rc_wgsl_check_fail(c, node, RC_STR("only memory has an address: a var, or part of one"));
    }
    else if (component || operand.space == RC_WGSL_SPACE_HANDLE) {
        rc_wgsl_check_fail(c, node, RC_STR("can't take the address of this"));
    }
    return rc_wgsl_value_of(rc_wgsl_type_pointer(&c->types, operand.type,
        (rc_wgsl_space)operand.space, (rc_wgsl_access)operand.access, arena), 0);
}


// *operand, the memory a pointer points at.
static rc_wgsl_sema rc_wgsl_check_deref(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema operand,
    rc_arena *arena)
{
    if (rc_wgsl_check_typeinfo_type(c, operand.type) != RC_WGSL_TYPEINFO_TYPE_POINTER) {
        rc_wgsl_check_fail_type(c, node, RC_STR("'*' needs a pointer, not "), operand.type,
            RC_STR(""), arena);
        return (rc_wgsl_sema) {0};
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


// a op b for integers, or fail at node if it overflows or divides by zero. The result must fit
// type, the operands' type.
static int64_t rc_wgsl_fold_int(rc_wgsl_checker *c, uint32_t node, rc_wgsl_token_type op,
    int64_t a, int64_t b, rc_wgsl_typeinfo_type type)
{
    int64_t result = 0;
    bool bad = false;

    switch (op) {
    case RC_WGSL_TOKEN_TYPE_PLUS:
        bad = (b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b);
        result = bad ? 0 : a + b;
        break;
    case RC_WGSL_TOKEN_TYPE_MINUS:
        bad = (b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b);
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
        bad = b >= (type == RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT ? 63 : 32)
            || (a != 0 && ((a << b) >> b) != a);
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
static void rc_wgsl_concretise_operands(rc_wgsl_checker *c, uint32_t node, uint32_t scalar,
    rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_token_type op = rc_wgsl_operator_of(c, node);
    bool shift = op == RC_WGSL_TOKEN_TYPE_SHIFT_LEFT || op == RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT;
    uint32_t left = rc_wgsl_check_sema(c, n.lhs).type;
    uint32_t right = rc_wgsl_check_sema(c, n.rhs).type;

    if (rc_wgsl_type_is_abstract(c->types.view, scalar)) {
        return;
    }
    rc_wgsl_check_concretise(c, n.lhs, rc_wgsl_type_with_scalar(&c->types, left, scalar, arena),
        arena);
    rc_wgsl_check_concretise(c, n.rhs, rc_wgsl_type_with_scalar(&c->types, right,
        shift ? RC_WGSL_TYPE_ID_U32 : scalar, arena), arena);
}


// The type of a product involving a matrix, of scalar elements, or 0 if there is none.
static uint32_t rc_wgsl_matrix_product_type(rc_wgsl_checker *c, uint32_t left, uint32_t right,
    uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_typeinfo l = rc_wgsl_check_typeinfo(c, left);
    rc_wgsl_typeinfo r = rc_wgsl_check_typeinfo(c, right);

    if (l.type == RC_WGSL_TYPEINFO_TYPE_MATRIX && r.type == RC_WGSL_TYPEINFO_TYPE_MATRIX) {
        return l.columns == r.rows
            ? rc_wgsl_type_matrix(&c->types, scalar, r.columns, l.rows, arena) : 0;
    }
    if (l.type == RC_WGSL_TYPEINFO_TYPE_MATRIX && r.type == RC_WGSL_TYPEINFO_TYPE_VECTOR) {
        return l.columns == r.width ? rc_wgsl_type_vector(&c->types, scalar, l.rows, arena) : 0;
    }
    if (l.type == RC_WGSL_TYPEINFO_TYPE_VECTOR && r.type == RC_WGSL_TYPEINFO_TYPE_MATRIX) {
        return l.width == r.rows ? rc_wgsl_type_vector(&c->types, scalar, r.columns, arena) : 0;
    }
    if (rc_wgsl_type_is_scalar(c->types.view, left)) {
        return rc_wgsl_type_with_scalar(&c->types, right, scalar, arena);
    }
    return rc_wgsl_type_is_scalar(c->types.view, right)
        ? rc_wgsl_type_with_scalar(&c->types, left, scalar, arena) : 0;
}


// The type of arithmetic involving a matrix, or 0 if there is none.
static uint32_t rc_wgsl_matrix_arithmetic_type(rc_wgsl_checker *c, rc_wgsl_token_type op,
    uint32_t left, uint32_t right, uint32_t scalar, rc_arena *arena)
{
    if (!rc_wgsl_type_is_float_scalar(c->types.view, scalar)) {
        return 0;
    }
    if (op == RC_WGSL_TOKEN_TYPE_STAR) {
        return rc_wgsl_matrix_product_type(c, left, right, scalar, arena);
    }
    if (op == RC_WGSL_TOKEN_TYPE_PLUS || op == RC_WGSL_TOKEN_TYPE_MINUS) {
        rc_wgsl_typeinfo l = rc_wgsl_check_typeinfo(c, left);
        rc_wgsl_typeinfo r = rc_wgsl_check_typeinfo(c, right);

        return l.type == r.type && l.columns == r.columns && l.rows == r.rows
            ? rc_wgsl_type_with_scalar(&c->types, left, scalar, arena) : 0;
    }
    return 0;
}


// The type of left op right for + - * / %, or 0 if the operator can't take them.
static uint32_t rc_wgsl_arithmetic_type(rc_wgsl_checker *c, rc_wgsl_token_type op, uint32_t left,
    uint32_t right, rc_arena *arena)
{
    uint32_t scalar = rc_wgsl_common_type(c, rc_wgsl_type_scalar_of(c->types.view, left),
        rc_wgsl_type_scalar_of(c->types.view, right));
    uint32_t left_width = rc_wgsl_type_width_of(c->types.view, left);
    uint32_t right_width = rc_wgsl_type_width_of(c->types.view, right);

    if (scalar == 0 || !rc_wgsl_type_is_numeric_scalar(c->types.view, scalar)) {
        return 0;
    }
    if (rc_wgsl_check_typeinfo_type(c, left) == RC_WGSL_TYPEINFO_TYPE_MATRIX
        || rc_wgsl_check_typeinfo_type(c, right) == RC_WGSL_TYPEINFO_TYPE_MATRIX) {
        return rc_wgsl_matrix_arithmetic_type(c, op, left, right, scalar, arena);
    }
    if (!rc_wgsl_type_is_scalar_or_vector(c->types.view, left)
        || !rc_wgsl_type_is_scalar_or_vector(c->types.view, right)
        || (left_width > 1 && right_width > 1 && left_width != right_width)) {
        return 0;
    }
    if (left_width == 1 && right_width == 1) {
        return scalar;
    }
    return rc_wgsl_type_vector(&c->types, scalar, left_width > right_width ? left_width
                                                                           : right_width, arena);
}


// The type of left op right for a comparison, or 0 if it can't compare them. Both operands
// settle on a common type.
static uint32_t rc_wgsl_comparison_type(rc_wgsl_checker *c, uint32_t node, rc_wgsl_token_type op,
    uint32_t left, uint32_t right, rc_arena *arena)
{
    uint32_t common = rc_wgsl_common_type(c, left, right);
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.view, common);
    bool equality = op == RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL || op == RC_WGSL_TOKEN_TYPE_BANG_EQUAL;

    if (common == 0 || !rc_wgsl_type_is_scalar_or_vector(c->types.view, common)
        || (!equality && !rc_wgsl_type_is_numeric_scalar(c->types.view, scalar))) {
        return 0;
    }

    // The result is a concrete bool, so abstract operands settle here, on their default type
    rc_wgsl_concretise_operands(c, node, rc_wgsl_type_concretise(&c->types, scalar, arena), arena);
    return rc_wgsl_type_with_scalar(&c->types, common, RC_WGSL_TYPE_ID_BOOL, arena);
}


// The type of left op right for & | ^, or 0: integers, or bools for & and |, of the same shape.
static uint32_t rc_wgsl_bitwise_type(rc_wgsl_checker *c, rc_wgsl_token_type op, uint32_t left,
    uint32_t right)
{
    uint32_t common = rc_wgsl_common_type(c, left, right);
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.view, common);
    bool boolean = scalar == RC_WGSL_TYPE_ID_BOOL && op != RC_WGSL_TOKEN_TYPE_XOR;

    if (common == 0 || !rc_wgsl_type_is_scalar_or_vector(c->types.view, common)
        || (!boolean && !rc_wgsl_type_is_integer_scalar(c->types.view, scalar))) {
        return 0;
    }
    return common;
}


// The type of left shifted by right, or 0: an integer, by u32 of the same shape.
static uint32_t rc_wgsl_shift_type(rc_wgsl_checker *c, uint32_t left, uint32_t right)
{
    uint32_t right_scalar = rc_wgsl_type_scalar_of(c->types.view, right);

    if (!rc_wgsl_type_is_scalar_or_vector(c->types.view, left)
        || !rc_wgsl_type_is_integer_scalar(c->types.view, rc_wgsl_type_scalar_of(c->types.view, left))
        || rc_wgsl_type_width_of(c->types.view, left) != rc_wgsl_type_width_of(c->types.view, right)
        || !rc_wgsl_type_is_scalar_or_vector(c->types.view, right)
        || (right_scalar != RC_WGSL_TYPE_ID_U32 && right_scalar != RC_WGSL_TYPE_ID_ABSTRACT_INT)) {
        return 0;
    }
    return left;
}


uint32_t rc_wgsl_check_operation_type(rc_wgsl_checker *c, rc_wgsl_token_type op, uint32_t left,
    uint32_t right, rc_arena *arena)
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
        return left == RC_WGSL_TYPE_ID_BOOL && right == RC_WGSL_TYPE_ID_BOOL
            ? RC_WGSL_TYPE_ID_BOOL : 0;
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
        return rc_wgsl_constant_of(type, rc_wgsl_fold_int(c, node, op, left.value, right.value,
            operand_type));
    }
}


// A binary operator and its operands.
static rc_wgsl_sema rc_wgsl_check_binary(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_token_type op = rc_wgsl_operator_of(c, node);
    rc_wgsl_sema left = rc_wgsl_check_expr(c, n.lhs, arena);
    rc_wgsl_sema right = rc_wgsl_check_expr(c, n.rhs, arena);
    uint32_t type;

    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }
    type = rc_wgsl_binary_type(c, node, op, left.type, right.type, arena);
    if (type == 0) {
        rc_mstr message = {0};

        rc_mstr_append(&message, RC_STR("can't use '"), arena);
        rc_mstr_append(&message, rc_wgsl_ast_text(c->ast, node), arena);
        rc_mstr_append(&message, RC_STR("' on "), arena);
        rc_wgsl_check_fail_types(c, node, message.view, left.type, RC_STR(" and "), right.type,
            arena);
        return (rc_wgsl_sema) {0};
    }
    if (rc_wgsl_type_scalar_of(c->types.view, type) != RC_WGSL_TYPE_ID_BOOL) {
        rc_wgsl_concretise_operands(c, node, rc_wgsl_type_scalar_of(c->types.view, type), arena);
    }
    if (left.flags & right.flags & RC_WGSL_SEMA_VALUE) {
        return rc_wgsl_fold_binary(c, node, op, rc_wgsl_check_sema(c, n.lhs),
            rc_wgsl_check_sema(c, n.rhs), type);
    }
    return rc_wgsl_value_of(type, rc_wgsl_const_flags(left, right));
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
        return rc_wgsl_constant_of(RC_WGSL_TYPE_ID_BOOL,
            rc_str_is_equal(rc_wgsl_ast_text(c->ast, node), RC_STR("true")));
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
        rc_wgsl_check_fail(c, node, RC_STR("expected an expression"));
        return (rc_wgsl_sema) {0};
    }
}


rc_wgsl_sema rc_wgsl_check_expr(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_sema sema;

    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }
    sema = rc_wgsl_check_expr_by_node_type(c, node, arena);
    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }
    rc_wgsl_check_set(c, node, sema);
    return sema;
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
static void rc_wgsl_concretise_child(rc_wgsl_checker *c, uint32_t child, uint32_t scalar,
    rc_arena *arena)
{
    uint32_t type = rc_wgsl_check_sema(c, child).type;

    if (child != 0 && rc_wgsl_type_is_abstract(c->types.view, type)) {
        rc_wgsl_check_concretise(c, child, rc_wgsl_type_with_scalar(&c->types, type, scalar, arena),
            arena);
    }
}


// Settle the parts of node, which has just settled on a type with scalar elements.
static void rc_wgsl_concretise_parts(rc_wgsl_checker *c, uint32_t node, uint32_t scalar,
    rc_arena *arena)
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
    uint32_t scalar = rc_wgsl_type_leaf_scalar(c->types.view, type);

    if (rc_wgsl_check_failed(c) || !rc_wgsl_type_is_abstract(c->types.view, sema.type)
        || sema.type == type) {
        return;
    }
    if (!rc_wgsl_type_converts(c->types.view, sema.type, type)) {
        rc_wgsl_check_fail_types(c, node, RC_STR("can't convert "), sema.type, RC_STR(" to "), type,
            arena);
        return;
    }
    if ((sema.flags & RC_WGSL_SEMA_VALUE)
        && !rc_wgsl_fits(rc_wgsl_check_typeinfo_type(c, type), sema.value)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("this is out of range for "), type, RC_STR(""),
            arena);
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
    if (!rc_wgsl_type_converts(c->types.view, from, type)) {
        rc_wgsl_check_fail_types(c, node, RC_STR("expected "), type, RC_STR(", got "), from, arena);
        return;
    }
    rc_wgsl_check_concretise(c, node, type, arena);
}
