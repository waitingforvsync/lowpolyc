// check_builtin.c
//
// Calls to WGSL's built-in functions. Most share one of a few shapes of signature, the rules
// below, and the texture functions each read their arguments in turn.
//
// A built-in whose arguments are all abstract gives an abstract result, which settles later
// along with its arguments, as any other abstract expression does.

#include "checker.h"

#include "richc/mstr.h"

// The shapes of signature the built-ins share.
typedef enum rc_wgsl_rule {
    RC_WGSL_RULE_NONE = 0,

    // (T, ...) -> T for a float scalar or vector T
    RC_WGSL_RULE_FLOAT,

    // (T, ...) -> T for a numeric scalar or vector T
    RC_WGSL_RULE_NUMERIC,

    // (T) -> T for a float or signed integer scalar or vector T
    RC_WGSL_RULE_SIGNED,

    // (T) -> T for an f32 scalar or vector T, in fragment shaders only
    RC_WGSL_RULE_DERIVATIVE,

    // (T, ...) -> the scalar of T, a float scalar or vector
    RC_WGSL_RULE_LENGTH,

    // (vecN<T>, vecN<T>) -> T, numeric
    RC_WGSL_RULE_DOT,

    // (vec3<T>, vec3<T>) -> vec3<T>, float
    RC_WGSL_RULE_CROSS,

    // (vecN<T>, ...) -> vecN<T>, float
    RC_WGSL_RULE_FLOAT_VECTOR,

    // (vecN<T>, vecN<T>, T) -> vecN<T>, float
    RC_WGSL_RULE_REFRACT,

    // (T, T, T) -> T, or (vecN<T>, vecN<T>, T) -> vecN<T>, float
    RC_WGSL_RULE_MIX,

    // (matNxN<T>) -> T
    RC_WGSL_RULE_DETERMINANT,

    // (matCxR<T>) -> matRxC<T>
    RC_WGSL_RULE_TRANSPOSE,

    // (bool or vecN<bool>) -> bool
    RC_WGSL_RULE_REDUCE,

    // (T, T, bool) -> T, or (vecN<T>, vecN<T>, vecN<bool>) -> vecN<T>
    RC_WGSL_RULE_SELECT,

    // (vec2<f32>) -> u32, and back
    RC_WGSL_RULE_PACK,
    RC_WGSL_RULE_UNPACK,

    // bitcast<T>(e) for 32-bit T and e of the same width
    RC_WGSL_RULE_BITCAST,

    // (ptr<storage, array<T>>) -> u32
    RC_WGSL_RULE_ARRAY_LENGTH,

    // Each texture function its own way
    RC_WGSL_RULE_TEXTURE
} rc_wgsl_rule;

// A built-in's rule, and how many arguments it takes (for the rules that fix it).
typedef struct rc_wgsl_builtin_info {
    uint8_t rule;  // rc_wgsl_rule
    uint8_t args;
} rc_wgsl_builtin_info;

// The rule and argument count of builtin.
static rc_wgsl_builtin_info rc_wgsl_builtin_info_of(rc_wgsl_builtin builtin)
{
    static const rc_wgsl_builtin_info infos[] = {
        [RC_WGSL_BUILTIN_ABS] = {RC_WGSL_RULE_NUMERIC, 1},
        [RC_WGSL_BUILTIN_ACOS] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_ACOSH] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_ALL] = {RC_WGSL_RULE_REDUCE, 1},
        [RC_WGSL_BUILTIN_ANY] = {RC_WGSL_RULE_REDUCE, 1},
        [RC_WGSL_BUILTIN_ARRAY_LENGTH] = {RC_WGSL_RULE_ARRAY_LENGTH, 1},
        [RC_WGSL_BUILTIN_ASIN] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_ASINH] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_ATAN] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_ATAN2] = {RC_WGSL_RULE_FLOAT, 2},
        [RC_WGSL_BUILTIN_ATANH] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_BITCAST] = {RC_WGSL_RULE_BITCAST, 1},
        [RC_WGSL_BUILTIN_CEIL] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_CLAMP] = {RC_WGSL_RULE_NUMERIC, 3},
        [RC_WGSL_BUILTIN_COS] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_COSH] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_CROSS] = {RC_WGSL_RULE_CROSS, 2},
        [RC_WGSL_BUILTIN_DEGREES] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_DETERMINANT] = {RC_WGSL_RULE_DETERMINANT, 1},
        [RC_WGSL_BUILTIN_DISTANCE] = {RC_WGSL_RULE_LENGTH, 2},
        [RC_WGSL_BUILTIN_DOT] = {RC_WGSL_RULE_DOT, 2},
        [RC_WGSL_BUILTIN_DPDX] = {RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_DPDX_COARSE] = {RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_DPDX_FINE] = {RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_DPDY] = {RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_DPDY_COARSE] = {RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_DPDY_FINE] = {RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_EXP] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_EXP2] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_FACE_FORWARD] = {RC_WGSL_RULE_FLOAT_VECTOR, 3},
        [RC_WGSL_BUILTIN_FLOOR] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_FMA] = {RC_WGSL_RULE_FLOAT, 3},
        [RC_WGSL_BUILTIN_FRACT] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_FWIDTH] = {RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_FWIDTH_COARSE] = {RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_FWIDTH_FINE] = {RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_INVERSE_SQRT] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_LENGTH] = {RC_WGSL_RULE_LENGTH, 1},
        [RC_WGSL_BUILTIN_LOG] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_LOG2] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_MAX] = {RC_WGSL_RULE_NUMERIC, 2},
        [RC_WGSL_BUILTIN_MIN] = {RC_WGSL_RULE_NUMERIC, 2},
        [RC_WGSL_BUILTIN_MIX] = {RC_WGSL_RULE_MIX, 3},
        [RC_WGSL_BUILTIN_NORMALIZE] = {RC_WGSL_RULE_FLOAT_VECTOR, 1},
        [RC_WGSL_BUILTIN_PACK2X16FLOAT] = {RC_WGSL_RULE_PACK, 1},
        [RC_WGSL_BUILTIN_PACK2X16SNORM] = {RC_WGSL_RULE_PACK, 1},
        [RC_WGSL_BUILTIN_PACK2X16UNORM] = {RC_WGSL_RULE_PACK, 1},
        [RC_WGSL_BUILTIN_POW] = {RC_WGSL_RULE_FLOAT, 2},
        [RC_WGSL_BUILTIN_RADIANS] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_REFLECT] = {RC_WGSL_RULE_FLOAT_VECTOR, 2},
        [RC_WGSL_BUILTIN_REFRACT] = {RC_WGSL_RULE_REFRACT, 3},
        [RC_WGSL_BUILTIN_ROUND] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_SATURATE] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_SELECT] = {RC_WGSL_RULE_SELECT, 3},
        [RC_WGSL_BUILTIN_SIGN] = {RC_WGSL_RULE_SIGNED, 1},
        [RC_WGSL_BUILTIN_SIN] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_SINH] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_SMOOTHSTEP] = {RC_WGSL_RULE_FLOAT, 3},
        [RC_WGSL_BUILTIN_SQRT] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_STEP] = {RC_WGSL_RULE_FLOAT, 2},
        [RC_WGSL_BUILTIN_TAN] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_TANH] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_TEXTURE_DIMENSIONS] = {RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_LOAD] = {RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_NUM_LAYERS] = {RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_NUM_LEVELS] = {RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_NUM_SAMPLES] = {RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_SAMPLE] = {RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_SAMPLE_BIAS] = {RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE] = {RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE_LEVEL] = {RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_SAMPLE_GRAD] = {RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL] = {RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TRANSPOSE] = {RC_WGSL_RULE_TRANSPOSE, 1},
        [RC_WGSL_BUILTIN_TRUNC] = {RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_UNPACK2X16FLOAT] = {RC_WGSL_RULE_UNPACK, 1},
        [RC_WGSL_BUILTIN_UNPACK2X16SNORM] = {RC_WGSL_RULE_UNPACK, 1},
        [RC_WGSL_BUILTIN_UNPACK2X16UNORM] = {RC_WGSL_RULE_UNPACK, 1},
    };

    return infos[builtin];
}

// ---- arguments ----

// The type of argument i.
static uint32_t rc_wgsl_builtin_arg(const rc_wgsl_checker *c, rc_view_wgsl_index args, uint32_t i)
{
    return rc_wgsl_check_sema(c, rc_view_wgsl_index_get(args, i)).type;
}


// Fail at node, naming the built-in and the argument types it can't take.
static void rc_wgsl_builtin_mismatch(rc_wgsl_checker *c, uint32_t node, rc_wgsl_builtin builtin,
    rc_view_wgsl_index args, rc_arena *arena)
{
    rc_mstr message = rc_mstr_from_str(RC_STR("'"), 64, arena);

    rc_mstr_append(&message, rc_wgsl_builtin_name(builtin), arena);
    rc_mstr_append(&message, RC_STR("' can't take ("), arena);
    for (uint32_t i = 0; i < args.num; i++) {
        if (i > 0) {
            rc_mstr_append(&message, RC_STR(", "), arena);
        }
        rc_wgsl_type_append_name(&message, c->types.view, rc_wgsl_builtin_arg(c, args, i), c->ast,
            arena);
    }
    rc_mstr_append(&message, RC_STR(")"), arena);
    rc_wgsl_check_fail(c, node, message.view);
}


// The type every argument converts to, or 0 if there is none.
static uint32_t rc_wgsl_builtin_common(rc_wgsl_checker *c, rc_view_wgsl_index args,
    uint32_t first, uint32_t last)
{
    uint32_t common = rc_wgsl_builtin_arg(c, args, first);

    for (uint32_t i = first + 1; i < last && common != 0; i++) {
        uint32_t type = rc_wgsl_builtin_arg(c, args, i);

        if (rc_wgsl_type_converts(c->types.view, common, type)) {
            common = type;
        }
        else if (!rc_wgsl_type_converts(c->types.view, type, common)) {
            common = 0;
        }
    }
    return common;
}


// Convert arguments [first, last) to type.
static void rc_wgsl_builtin_convert(rc_wgsl_checker *c, rc_view_wgsl_index args, uint32_t first,
    uint32_t last, uint32_t type, rc_arena *arena)
{
    for (uint32_t i = first; i < last; i++) {
        rc_wgsl_check_convert(c, rc_view_wgsl_index_get(args, i), type, arena);
    }
}


// A float version of type: abstract integers become abstract floats.
static uint32_t rc_wgsl_floated(rc_wgsl_checker *c, uint32_t type, rc_arena *arena)
{
    if (rc_wgsl_type_scalar_of(c->types.view, type) == RC_WGSL_TYPE_ID_ABSTRACT_INT) {
        return rc_wgsl_type_with_scalar(&c->types, type, RC_WGSL_TYPE_ID_ABSTRACT_FLOAT, arena);
    }
    return type;
}


// ---- the shared rules ----

// The type all of a FLOAT, NUMERIC, SIGNED or DERIVATIVE built-in's arguments share, which its
// result has too; 0 if they have none.
static uint32_t rc_wgsl_same_type(rc_wgsl_checker *c, rc_wgsl_rule rule, rc_view_wgsl_index args,
    rc_arena *arena)
{
    uint32_t type = rc_wgsl_builtin_common(c, args, 0, args.num);
    uint32_t scalar;

    if (rule != RC_WGSL_RULE_NUMERIC) {
        type = rc_wgsl_floated(c, type, arena);
    }
    scalar = rc_wgsl_type_scalar_of(c->types.view, type);
    if (type == 0 || !rc_wgsl_type_is_scalar_or_vector(c->types.view, type)
        || !rc_wgsl_type_is_numeric_scalar(c->types.view, scalar)
        || (rule == RC_WGSL_RULE_SIGNED && scalar == RC_WGSL_TYPE_ID_U32)) {
        return 0;
    }
    if (rule == RC_WGSL_RULE_FLOAT && !rc_wgsl_type_is_float_scalar(c->types.view, scalar)) {
        return 0;
    }

    // The derivatives are f32 only, so their arguments settle now
    if (rule == RC_WGSL_RULE_DERIVATIVE) {
        type = rc_wgsl_type_with_scalar(&c->types, type, RC_WGSL_TYPE_ID_F32, arena);
    }
    rc_wgsl_builtin_convert(c, args, 0, args.num, type, arena);
    return type;
}


// The result of a built-in with a vector-shaped rule (LENGTH, DOT, CROSS, FLOAT_VECTOR), or 0.
static uint32_t rc_wgsl_vector_rule(rc_wgsl_checker *c, rc_wgsl_rule rule, rc_view_wgsl_index args,
    rc_arena *arena)
{
    uint32_t type = rc_wgsl_builtin_common(c, args, 0, args.num);
    rc_wgsl_typeinfo t;

    if (rule != RC_WGSL_RULE_DOT) {
        type = rc_wgsl_floated(c, type, arena);
    }
    t = rc_wgsl_check_typeinfo(c, type);
    if (type == 0 || !rc_wgsl_type_is_scalar_or_vector(c->types.view, type)
        || !rc_wgsl_type_is_numeric_scalar(c->types.view, rc_wgsl_type_scalar_of(c->types.view, type))
        || (rule != RC_WGSL_RULE_DOT && !rc_wgsl_type_is_float_scalar(c->types.view,
            rc_wgsl_type_scalar_of(c->types.view, type)))
        || (rule != RC_WGSL_RULE_LENGTH && t.type != RC_WGSL_TYPEINFO_TYPE_VECTOR)
        || (rule == RC_WGSL_RULE_CROSS && t.width != 3)) {
        return 0;
    }
    rc_wgsl_builtin_convert(c, args, 0, args.num, type, arena);
    if (rule == RC_WGSL_RULE_LENGTH || rule == RC_WGSL_RULE_DOT) {
        return rc_wgsl_type_scalar_of(c->types.view, type);
    }
    return type;
}


// The result of refract(v, v, s), or mix(v, v, s) with a scalar blend, or 0.
static uint32_t rc_wgsl_vector_scalar_rule(rc_wgsl_checker *c, rc_view_wgsl_index args,
    rc_arena *arena)
{
    uint32_t vector = rc_wgsl_floated(c, rc_wgsl_builtin_common(c, args, 0, 2), arena);
    uint32_t scalar = rc_wgsl_floated(c, rc_wgsl_builtin_arg(c, args, 2), arena);
    uint32_t element = rc_wgsl_type_scalar_of(c->types.view, vector);

    if (vector == 0 || rc_wgsl_check_typeinfo_type(c, vector) != RC_WGSL_TYPEINFO_TYPE_VECTOR
        || !rc_wgsl_type_is_scalar(c->types.view, scalar)) {
        return 0;
    }

    // The vector and the scalar settle on the more specific of their element types
    if (rc_wgsl_type_converts(c->types.view, element, scalar)) {
        element = scalar;
    }
    else if (!rc_wgsl_type_converts(c->types.view, scalar, element)) {
        return 0;
    }
    if (!rc_wgsl_type_is_float_scalar(c->types.view, element)) {
        return 0;
    }
    vector = rc_wgsl_type_with_scalar(&c->types, vector, element, arena);
    rc_wgsl_builtin_convert(c, args, 0, 2, vector, arena);
    rc_wgsl_builtin_convert(c, args, 2, 3, element, arena);
    return vector;
}


// The result of a matrix built-in (DETERMINANT, TRANSPOSE), or 0.
static uint32_t rc_wgsl_matrix_rule(rc_wgsl_checker *c, rc_wgsl_rule rule, rc_view_wgsl_index args,
    rc_arena *arena)
{
    uint32_t type = rc_wgsl_builtin_arg(c, args, 0);
    rc_wgsl_typeinfo t = rc_wgsl_check_typeinfo(c, type);

    if (t.type != RC_WGSL_TYPEINFO_TYPE_MATRIX) {
        return 0;
    }
    if (rule == RC_WGSL_RULE_DETERMINANT) {
        return t.columns == t.rows ? t.element : 0;
    }
    return rc_wgsl_type_matrix(&c->types, t.element, t.rows, t.columns, arena);
}


// The result of select(f, t, condition), or 0.
static uint32_t rc_wgsl_select_rule(rc_wgsl_checker *c, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t type = rc_wgsl_builtin_common(c, args, 0, 2);
    uint32_t condition = rc_wgsl_builtin_arg(c, args, 2);

    if (type == 0 || !rc_wgsl_type_is_scalar_or_vector(c->types.view, type)
        || rc_wgsl_type_scalar_of(c->types.view, condition) != RC_WGSL_TYPE_ID_BOOL
        || !rc_wgsl_type_is_scalar_or_vector(c->types.view, condition)
        || (rc_wgsl_check_typeinfo_type(c, condition) == RC_WGSL_TYPEINFO_TYPE_VECTOR
            && rc_wgsl_type_width_of(c->types.view, condition)
                != rc_wgsl_type_width_of(c->types.view, type))) {
        return 0;
    }
    rc_wgsl_builtin_convert(c, args, 0, 2, type, arena);
    return type;
}


// The result of a built-in with one fixed argument type and result: the packs and unpacks.
static uint32_t rc_wgsl_fixed_rule(rc_wgsl_checker *c, rc_wgsl_rule rule, rc_view_wgsl_index args,
    rc_arena *arena)
{
    uint32_t vec2f = rc_wgsl_type_vector(&c->types, RC_WGSL_TYPE_ID_F32, 2, arena);
    uint32_t from = rule == RC_WGSL_RULE_PACK ? vec2f : RC_WGSL_TYPE_ID_U32;

    if (!rc_wgsl_type_converts(c->types.view, rc_wgsl_builtin_arg(c, args, 0), from)) {
        return 0;
    }
    rc_wgsl_builtin_convert(c, args, 0, 1, from, arena);
    return rule == RC_WGSL_RULE_PACK ? RC_WGSL_TYPE_ID_U32 : vec2f;
}


// True for i32, u32 and f32, and vectors of them: the types bitcast takes.
static bool rc_wgsl_is_bitcastable(const rc_wgsl_checker *c, uint32_t type)
{
    rc_wgsl_typeinfo_type scalar_type = rc_wgsl_check_typeinfo_type(c,
        rc_wgsl_type_scalar_of(c->types.view, type));

    return rc_wgsl_type_is_scalar_or_vector(c->types.view, type)
        && (scalar_type == RC_WGSL_TYPEINFO_TYPE_I32 || scalar_type == RC_WGSL_TYPEINFO_TYPE_U32
            || scalar_type == RC_WGSL_TYPEINFO_TYPE_F32);
}


// The result of bitcast<T>(e), or 0.
static uint32_t rc_wgsl_bitcast_rule(rc_wgsl_checker *c, uint32_t node, rc_view_wgsl_index args,
    rc_arena *arena)
{
    uint32_t callee = rc_wgsl_ast_node(c->ast, node).lhs;
    rc_view_wgsl_index templates = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, callee).lhs);
    uint32_t type;
    uint32_t from;

    if (templates.num != 1) {
        rc_wgsl_check_fail(c, callee, RC_STR("bitcast needs its type, as bitcast<u32>(x)"));
        return 0;
    }
    type = rc_wgsl_check_type_expr(c, rc_view_wgsl_index_get(templates, 0), arena);
    rc_wgsl_check_concretise_default(c, rc_view_wgsl_index_get(args, 0), arena);
    from = rc_wgsl_builtin_arg(c, args, 0);
    if (rc_wgsl_check_failed(c) || !rc_wgsl_is_bitcastable(c, type)
        || !rc_wgsl_is_bitcastable(c, from)
        || rc_wgsl_type_width_of(c->types.view, type) != rc_wgsl_type_width_of(c->types.view, from)) {
        return 0;
    }
    return type;
}


// The result of arrayLength(p), or 0.
static uint32_t rc_wgsl_array_length_rule(rc_wgsl_checker *c, rc_view_wgsl_index args)
{
    rc_wgsl_typeinfo pointer = rc_wgsl_check_typeinfo(c, rc_wgsl_builtin_arg(c, args, 0));
    rc_wgsl_typeinfo array = rc_wgsl_check_typeinfo(c, pointer.element);

    if (pointer.type != RC_WGSL_TYPEINFO_TYPE_POINTER || pointer.space != RC_WGSL_SPACE_STORAGE
        || array.type != RC_WGSL_TYPEINFO_TYPE_ARRAY || array.count != 0) {
        return 0;
    }
    return RC_WGSL_TYPE_ID_U32;
}


// The result type of a call to builtin by its rule, or 0 if the arguments don't fit it.
static uint32_t rc_wgsl_builtin_result(rc_wgsl_checker *c, uint32_t node, rc_wgsl_rule rule,
    rc_view_wgsl_index args, rc_arena *arena)
{
    switch (rule) {
    case RC_WGSL_RULE_FLOAT:
    case RC_WGSL_RULE_NUMERIC:
    case RC_WGSL_RULE_SIGNED:
        return rc_wgsl_same_type(c, rule, args, arena);
    case RC_WGSL_RULE_DERIVATIVE:
        rc_wgsl_check_note_fragment_only(c, node);
        return rc_wgsl_same_type(c, rule, args, arena);
    case RC_WGSL_RULE_LENGTH:
    case RC_WGSL_RULE_DOT:
    case RC_WGSL_RULE_CROSS:
    case RC_WGSL_RULE_FLOAT_VECTOR:
        return rc_wgsl_vector_rule(c, rule, args, arena);
    case RC_WGSL_RULE_REFRACT:
        return rc_wgsl_vector_scalar_rule(c, args, arena);
    case RC_WGSL_RULE_MIX:
        if (rc_wgsl_type_is_scalar(c->types.view, rc_wgsl_builtin_arg(c, args, 2))
            && rc_wgsl_check_typeinfo_type(c, rc_wgsl_builtin_arg(c, args, 0))
                == RC_WGSL_TYPEINFO_TYPE_VECTOR) {
            return rc_wgsl_vector_scalar_rule(c, args, arena);
        }
        return rc_wgsl_same_type(c, RC_WGSL_RULE_FLOAT, args, arena);
    case RC_WGSL_RULE_DETERMINANT:
    case RC_WGSL_RULE_TRANSPOSE:
        return rc_wgsl_matrix_rule(c, rule, args, arena);
    case RC_WGSL_RULE_REDUCE:
        return rc_wgsl_type_scalar_of(c->types.view, rc_wgsl_builtin_arg(c, args, 0))
                == RC_WGSL_TYPE_ID_BOOL
            && rc_wgsl_type_is_scalar_or_vector(c->types.view, rc_wgsl_builtin_arg(c, args, 0))
            ? RC_WGSL_TYPE_ID_BOOL : 0;
    case RC_WGSL_RULE_SELECT:
        return rc_wgsl_select_rule(c, args, arena);
    case RC_WGSL_RULE_PACK:
    case RC_WGSL_RULE_UNPACK:
        return rc_wgsl_fixed_rule(c, rule, args, arena);
    case RC_WGSL_RULE_BITCAST:
        return rc_wgsl_bitcast_rule(c, node, args, arena);
    case RC_WGSL_RULE_ARRAY_LENGTH:
        return rc_wgsl_array_length_rule(c, args);
    default:
        RC_UNREACHABLE();
        return 0;
    }
}

// ---- textures ----

// Reading a texture call's arguments in turn.
typedef struct rc_wgsl_texture_args {
    rc_view_wgsl_index args;
    uint32_t next;
    uint32_t node;
} rc_wgsl_texture_args;

// The number of coordinates a texture of dim takes.
static uint32_t rc_wgsl_texture_coords(rc_wgsl_dim dim)
{
    switch (dim) {
    case RC_WGSL_DIM_1D:
        return 1;
    case RC_WGSL_DIM_2D:
    case RC_WGSL_DIM_2D_ARRAY:
        return 2;
    default:
        return 3;
    }
}


// True for the dims with layers.
static bool rc_wgsl_texture_is_array(rc_wgsl_dim dim)
{
    return dim == RC_WGSL_DIM_2D_ARRAY || dim == RC_WGSL_DIM_CUBE_ARRAY;
}


// The next argument, failing at the call if there is none.
static uint32_t rc_wgsl_texture_next(rc_wgsl_checker *c, rc_wgsl_texture_args *t)
{
    if (t->next >= t->args.num) {
        rc_wgsl_check_fail(c, t->node, RC_STR("this needs more arguments"));
        return 0;
    }
    return rc_view_wgsl_index_get(t->args, t->next++);
}


// The scalar, or vector of width, of element: what coordinates and the like are.
static uint32_t rc_wgsl_texture_vector(rc_wgsl_checker *c, uint32_t element, uint32_t width,
    rc_arena *arena)
{
    return width == 1 ? element : rc_wgsl_type_vector(&c->types, element, width, arena);
}


// The next argument, which must convert to type.
static void rc_wgsl_texture_take(rc_wgsl_checker *c, rc_wgsl_texture_args *t, uint32_t type,
    rc_arena *arena)
{
    uint32_t arg = rc_wgsl_texture_next(c, t);

    if (arg != 0) {
        rc_wgsl_check_convert(c, arg, type, arena);
    }
}


// The next argument, which must be an i32 or u32, or a vector of width of them.
static void rc_wgsl_texture_take_int(rc_wgsl_checker *c, rc_wgsl_texture_args *t, uint32_t width,
    rc_arena *arena)
{
    uint32_t arg = rc_wgsl_texture_next(c, t);
    uint32_t type = rc_wgsl_check_sema(c, arg).type;

    if (arg == 0) {
        return;
    }
    if (!rc_wgsl_type_is_scalar_or_vector(c->types.view, type)
        || rc_wgsl_type_width_of(c->types.view, type) != width
        || !rc_wgsl_type_is_integer_scalar(c->types.view,
            rc_wgsl_type_scalar_of(c->types.view, type))) {
        rc_wgsl_check_fail_type(c, arg, RC_STR("expected i32 or u32 here, not "), type, RC_STR(""),
            arena);
        return;
    }
    rc_wgsl_check_concretise_default(c, arg, arena);
}


// The texture argument, first; fails unless it is a texture of one of the classes in classes, a
// bit set of rc_wgsl_texture_class.
static rc_wgsl_typeinfo rc_wgsl_texture_take_texture(rc_wgsl_checker *c, rc_wgsl_texture_args *t,
    uint32_t classes, bool float_only, rc_arena *arena)
{
    uint32_t arg = rc_wgsl_texture_next(c, t);
    uint32_t type = rc_wgsl_check_sema(c, arg).type;
    rc_wgsl_typeinfo texture = rc_wgsl_check_typeinfo(c, type);

    if (arg == 0) {
        return (rc_wgsl_typeinfo) {0};
    }
    if (texture.type != RC_WGSL_TYPEINFO_TYPE_TEXTURE || !(classes & (1u << texture.texture_class))
        || (float_only && texture.element != RC_WGSL_TYPE_ID_F32)) {
        rc_wgsl_check_fail_type(c, arg, RC_STR("this can't take "), type, RC_STR(""), arena);
    }
    return texture;
}


// The sampler argument, a comparison sampler if comparison.
static void rc_wgsl_texture_take_sampler(rc_wgsl_checker *c, rc_wgsl_texture_args *t,
    bool comparison, rc_arena *arena)
{
    uint32_t arg = rc_wgsl_texture_next(c, t);
    uint32_t type = rc_wgsl_check_sema(c, arg).type;
    uint32_t expected = comparison ? RC_WGSL_TYPE_ID_SAMPLER_COMPARISON : RC_WGSL_TYPE_ID_SAMPLER;

    if (arg != 0 && type != expected) {
        rc_wgsl_check_fail_types(c, arg, RC_STR("expected "), expected, RC_STR(", got "), type,
            arena);
    }
}


// The optional offset argument last, a constant vector of i32, if the texture's dim takes one.
static void rc_wgsl_texture_take_offset(rc_wgsl_checker *c, rc_wgsl_texture_args *t,
    rc_wgsl_dim dim, rc_arena *arena)
{
    uint32_t arg;

    if (t->next >= t->args.num || dim == RC_WGSL_DIM_CUBE || dim == RC_WGSL_DIM_CUBE_ARRAY
        || dim == RC_WGSL_DIM_1D) {
        return;
    }
    arg = rc_view_wgsl_index_get(t->args, t->next);
    rc_wgsl_texture_take(c, t, rc_wgsl_type_vector(&c->types, RC_WGSL_TYPE_ID_I32,
        rc_wgsl_texture_coords(dim), arena), arena);
    if (!(rc_wgsl_check_sema(c, arg).flags & RC_WGSL_SEMA_CONST) && !rc_wgsl_check_failed(c)) {
        rc_wgsl_check_fail(c, arg, RC_STR("an offset must be known before the shader runs"));
    }
}


// The coordinates, and the layer for an arrayed texture, for sampling.
static void rc_wgsl_texture_take_place(rc_wgsl_checker *c, rc_wgsl_texture_args *t,
    rc_wgsl_typeinfo texture, rc_arena *arena)
{
    rc_wgsl_dim dim = (rc_wgsl_dim)texture.dim;

    rc_wgsl_texture_take(c, t, rc_wgsl_texture_vector(c, RC_WGSL_TYPE_ID_F32,
        rc_wgsl_texture_coords(dim), arena), arena);
    if (rc_wgsl_texture_is_array(dim)) {
        rc_wgsl_texture_take_int(c, t, 1, arena);
    }
}


// The classes that hold floats to sample: sampled and depth textures.
enum {
    RC_WGSL_SAMPLED_BIT = 1u << RC_WGSL_TEXTURE_CLASS_SAMPLED,
    RC_WGSL_MULTISAMPLED_BIT = 1u << RC_WGSL_TEXTURE_CLASS_MULTISAMPLED,
    RC_WGSL_DEPTH_BIT = 1u << RC_WGSL_TEXTURE_CLASS_DEPTH,
    RC_WGSL_DEPTH_MULTISAMPLED_BIT = 1u << RC_WGSL_TEXTURE_CLASS_DEPTH_MULTISAMPLED
};

// textureSample, textureSampleBias, textureSampleLevel and textureSampleGrad.
static uint32_t rc_wgsl_check_texture_sample(rc_wgsl_checker *c, rc_wgsl_builtin builtin,
    rc_wgsl_texture_args *t, rc_arena *arena)
{
    bool depth_ok = builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE
        || builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL;
    rc_wgsl_typeinfo texture = rc_wgsl_texture_take_texture(c, t,
        RC_WGSL_SAMPLED_BIT | (depth_ok ? RC_WGSL_DEPTH_BIT : 0u), true, arena);
    bool depth = texture.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH;
    uint32_t coords = rc_wgsl_texture_vector(c, RC_WGSL_TYPE_ID_F32,
        rc_wgsl_texture_coords((rc_wgsl_dim)texture.dim), arena);

    rc_wgsl_texture_take_sampler(c, t, false, arena);
    rc_wgsl_texture_take_place(c, t, texture, arena);
    if (builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_BIAS) {
        rc_wgsl_texture_take(c, t, RC_WGSL_TYPE_ID_F32, arena);
    }
    else if (builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL && depth) {
        rc_wgsl_texture_take_int(c, t, 1, arena);
    }
    else if (builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL) {
        rc_wgsl_texture_take(c, t, RC_WGSL_TYPE_ID_F32, arena);
    }
    else if (builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_GRAD) {
        rc_wgsl_texture_take(c, t, coords, arena);
        rc_wgsl_texture_take(c, t, coords, arena);
    }
    if (builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE || builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_BIAS) {
        rc_wgsl_check_note_fragment_only(c, t->node);
    }
    rc_wgsl_texture_take_offset(c, t, (rc_wgsl_dim)texture.dim, arena);
    return depth ? RC_WGSL_TYPE_ID_F32 : rc_wgsl_type_vector(&c->types, RC_WGSL_TYPE_ID_F32, 4,
        arena);
}


// textureSampleCompare and textureSampleCompareLevel.
static uint32_t rc_wgsl_check_texture_compare(rc_wgsl_checker *c, rc_wgsl_builtin builtin,
    rc_wgsl_texture_args *t, rc_arena *arena)
{
    rc_wgsl_typeinfo texture = rc_wgsl_texture_take_texture(c, t, RC_WGSL_DEPTH_BIT, false, arena);

    rc_wgsl_texture_take_sampler(c, t, true, arena);
    rc_wgsl_texture_take_place(c, t, texture, arena);
    rc_wgsl_texture_take(c, t, RC_WGSL_TYPE_ID_F32, arena);
    rc_wgsl_texture_take_offset(c, t, (rc_wgsl_dim)texture.dim, arena);
    if (builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE) {
        rc_wgsl_check_note_fragment_only(c, t->node);
    }
    return RC_WGSL_TYPE_ID_F32;
}


// textureLoad of a sampled, multisampled or depth texture.
static uint32_t rc_wgsl_check_texture_load(rc_wgsl_checker *c, rc_wgsl_texture_args *t,
    rc_arena *arena)
{
    rc_wgsl_typeinfo texture = rc_wgsl_texture_take_texture(c, t, RC_WGSL_SAMPLED_BIT
        | RC_WGSL_MULTISAMPLED_BIT | RC_WGSL_DEPTH_BIT | RC_WGSL_DEPTH_MULTISAMPLED_BIT, false,
        arena);
    rc_wgsl_dim dim = (rc_wgsl_dim)texture.dim;
    bool depth = texture.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH
        || texture.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH_MULTISAMPLED;

    if (dim == RC_WGSL_DIM_CUBE || dim == RC_WGSL_DIM_CUBE_ARRAY) {
        rc_wgsl_check_fail(c, t->node, RC_STR("textureLoad can't read a cube texture"));
        return 0;
    }
    rc_wgsl_texture_take_int(c, t, rc_wgsl_texture_coords(dim), arena);
    if (rc_wgsl_texture_is_array(dim)) {
        rc_wgsl_texture_take_int(c, t, 1, arena);
    }

    // The level, or for a multisampled texture the sample
    rc_wgsl_texture_take_int(c, t, 1, arena);
    return depth ? RC_WGSL_TYPE_ID_F32 : rc_wgsl_type_vector(&c->types, texture.element, 4, arena);
}


// textureDimensions, with its optional level.
static uint32_t rc_wgsl_check_texture_dimensions(rc_wgsl_checker *c, rc_wgsl_texture_args *t,
    rc_arena *arena)
{
    rc_wgsl_typeinfo texture = rc_wgsl_texture_take_texture(c, t, RC_WGSL_SAMPLED_BIT
        | RC_WGSL_MULTISAMPLED_BIT | RC_WGSL_DEPTH_BIT | RC_WGSL_DEPTH_MULTISAMPLED_BIT, false,
        arena);
    rc_wgsl_dim dim = (rc_wgsl_dim)texture.dim;
    bool multisampled = texture.texture_class == RC_WGSL_TEXTURE_CLASS_MULTISAMPLED
        || texture.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH_MULTISAMPLED;

    if (t->next < t->args.num && !multisampled) {
        rc_wgsl_texture_take_int(c, t, 1, arena);
    }
    return rc_wgsl_texture_vector(c, RC_WGSL_TYPE_ID_U32, dim == RC_WGSL_DIM_1D ? 1
        : dim == RC_WGSL_DIM_3D ? 3 : 2, arena);
}


// textureNumLayers, textureNumLevels and textureNumSamples.
static uint32_t rc_wgsl_check_texture_count(rc_wgsl_checker *c, rc_wgsl_builtin builtin,
    rc_wgsl_texture_args *t, rc_arena *arena)
{
    uint32_t classes = builtin == RC_WGSL_BUILTIN_TEXTURE_NUM_SAMPLES
        ? RC_WGSL_MULTISAMPLED_BIT | RC_WGSL_DEPTH_MULTISAMPLED_BIT
        : RC_WGSL_SAMPLED_BIT | RC_WGSL_DEPTH_BIT;
    rc_wgsl_typeinfo texture = rc_wgsl_texture_take_texture(c, t, classes, false, arena);

    if (builtin == RC_WGSL_BUILTIN_TEXTURE_NUM_LAYERS
        && !rc_wgsl_texture_is_array((rc_wgsl_dim)texture.dim) && !rc_wgsl_check_failed(c)) {
        rc_wgsl_check_fail(c, t->node, RC_STR("only an arrayed texture has layers"));
    }
    return RC_WGSL_TYPE_ID_U32;
}


// A texture function's result, after checking its arguments one by one.
static uint32_t rc_wgsl_check_texture_call(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_builtin builtin, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_texture_args t = {.args = args, .node = node};
    uint32_t result;

    switch (builtin) {
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE:
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE_LEVEL:
        result = rc_wgsl_check_texture_compare(c, builtin, &t, arena);
        break;
    case RC_WGSL_BUILTIN_TEXTURE_LOAD:
        result = rc_wgsl_check_texture_load(c, &t, arena);
        break;
    case RC_WGSL_BUILTIN_TEXTURE_DIMENSIONS:
        result = rc_wgsl_check_texture_dimensions(c, &t, arena);
        break;
    case RC_WGSL_BUILTIN_TEXTURE_NUM_LAYERS:
    case RC_WGSL_BUILTIN_TEXTURE_NUM_LEVELS:
    case RC_WGSL_BUILTIN_TEXTURE_NUM_SAMPLES:
        result = rc_wgsl_check_texture_count(c, builtin, &t, arena);
        break;
    default:
        result = rc_wgsl_check_texture_sample(c, builtin, &t, arena);
        break;
    }
    if (!rc_wgsl_check_failed(c) && t.next < args.num) {
        rc_wgsl_check_fail(c, rc_view_wgsl_index_get(args, t.next),
            RC_STR("this is one argument too many"));
    }
    return result;
}

// ---- calls ----

rc_wgsl_sema rc_wgsl_check_builtin_call(rc_wgsl_checker *c, uint32_t node, rc_wgsl_builtin builtin,
    rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_builtin_info info = rc_wgsl_builtin_info_of(builtin);
    rc_wgsl_rule rule = (rc_wgsl_rule)info.rule;
    uint32_t callee = rc_wgsl_ast_node(c->ast, node).lhs;
    uint32_t type;
    uint8_t flags = RC_WGSL_SEMA_CONST;

    if (rule != RC_WGSL_RULE_BITCAST && rc_wgsl_ast_node(c->ast, callee).lhs != 0) {
        rc_wgsl_check_fail(c, callee, RC_STR("this function takes no template list"));
        return (rc_wgsl_sema) {0};
    }
    if (rule != RC_WGSL_RULE_TEXTURE && args.num != info.args) {
        rc_wgsl_builtin_mismatch(c, node, builtin, args, arena);
        return (rc_wgsl_sema) {0};
    }
    type = rule == RC_WGSL_RULE_TEXTURE ? rc_wgsl_check_texture_call(c, node, builtin, args, arena)
                                        : rc_wgsl_builtin_result(c, node, rule, args, arena);
    if (type == 0 && !rc_wgsl_check_failed(c)) {
        rc_wgsl_builtin_mismatch(c, node, builtin, args, arena);
    }
    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }

    // Built-ins are const when their arguments are, except those that read the shader's state
    for (uint32_t i = 0; i < args.num; i++) {
        flags &= rc_wgsl_check_sema(c, rc_view_wgsl_index_get(args, i)).flags;
    }
    if (rule == RC_WGSL_RULE_TEXTURE || rule == RC_WGSL_RULE_DERIVATIVE
        || rule == RC_WGSL_RULE_ARRAY_LENGTH) {
        flags = 0;
    }

    return (rc_wgsl_sema) {
        .type = type,
        .flags = (uint8_t)(flags & RC_WGSL_SEMA_CONST),
        .call = rule == RC_WGSL_RULE_BITCAST ? RC_WGSL_CALL_BITCAST : RC_WGSL_CALL_BUILTIN,
        .builtin = (uint8_t)builtin
    };
}
