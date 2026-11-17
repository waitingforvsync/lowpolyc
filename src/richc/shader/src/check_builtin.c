// check_builtin.c
//
// Calls to WGSL's built-in functions. Most share one of a few shapes of signature, the rules
// below, and the texture functions each read their arguments in turn.
//
// A built-in whose arguments are all abstract gives an abstract result, which settles later
// along with its arguments, as any other abstract expression does.

#include "checker.h"

#include "richc/mstr.h"

// ---- arguments ----

// Fail at node, naming the built-in and the argument types it can't take.
static void rc_wgsl_builtin_mismatch(rc_wgsl_checker *c, uint32_t node, rc_wgsl_builtin builtin, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_mstr message = rc_mstr_from_str(RC_STR("'"), 64, arena);

    rc_mstr_append(&message, rc_wgsl_builtin_name(builtin), arena);
    rc_mstr_append(&message, RC_STR("' can't take ("), arena);
    for (uint32_t i = 0; i < args.num; i++) {
        if (i > 0) {
            rc_mstr_append(&message, RC_STR(", "), arena);
        }

        rc_wgsl_type_append_name(&message, c->types.infos.view, rc_wgsl_check_arg_type(c, args, i), c->ast, arena);
    }
    rc_mstr_append(&message, RC_STR(")"), arena);
    rc_wgsl_check_fail(c, node, message.view);
}


// A float version of type: abstract integers become abstract floats.
static uint32_t rc_wgsl_floated(rc_wgsl_checker *c, uint32_t type, rc_arena *arena)
{
    if (rc_wgsl_type_scalar_of(c->types.infos.view, type) == RC_WGSL_TYPE_ID_ABSTRACT_INT) {
        return rc_wgsl_type_with_scalar(&c->types, type, RC_WGSL_TYPE_ID_ABSTRACT_FLOAT, arena);
    }

    return type;
}


// True for a scalar or vector of numbers.
static bool rc_wgsl_is_numeric_scalar_or_vector(const rc_wgsl_checker *c, uint32_t type)
{
    if (type == 0 || !rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, type)) {
        return false;
    }

    return rc_wgsl_type_is_numeric_scalar(c->types.infos.view, rc_wgsl_type_scalar_of(c->types.infos.view, type));
}


// True for a bool, or a vector of them.
static bool rc_wgsl_is_bool_scalar_or_vector(const rc_wgsl_checker *c, uint32_t type)
{
    if (rc_wgsl_type_scalar_of(c->types.infos.view, type) != RC_WGSL_TYPE_ID_BOOL) {
        return false;
    }

    return rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, type);
}

// ---- the shared rules ----

// The type all of a FLOAT, NUMERIC, SIGNED or DERIVATIVE built-in's arguments share, which its
// result has too; 0 if they have none.
static uint32_t rc_wgsl_same_type(rc_wgsl_checker *c, rc_wgsl_rule rule, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t common = rc_wgsl_check_common_arg_type(c, args, 0, args.num, false);
    uint32_t type = rule != RC_WGSL_RULE_NUMERIC ? rc_wgsl_floated(c, common, arena) : common;
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.infos.view, type);

    if (!rc_wgsl_is_numeric_scalar_or_vector(c, type)) {
        return 0;
    }

    if (rule == RC_WGSL_RULE_SIGNED && scalar == RC_WGSL_TYPE_ID_U32) {
        return 0;
    }

    if (rule == RC_WGSL_RULE_FLOAT && !rc_wgsl_type_is_float_scalar(c->types.infos.view, scalar)) {
        return 0;
    }

    // The derivatives are f32 only, so their arguments settle now
    if (rule == RC_WGSL_RULE_DERIVATIVE) {
        type = rc_wgsl_type_with_scalar(&c->types, type, RC_WGSL_TYPE_ID_F32, arena);
    }

    rc_wgsl_check_convert_args(c, args, 0, args.num, type, arena);
    return type;
}


// The result of a built-in with a vector-shaped rule (LENGTH, DOT, CROSS, FLOAT_VECTOR), or 0.
static uint32_t rc_wgsl_vector_rule(rc_wgsl_checker *c, rc_wgsl_rule rule, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t common = rc_wgsl_check_common_arg_type(c, args, 0, args.num, false);
    uint32_t type = rule != RC_WGSL_RULE_DOT ? rc_wgsl_floated(c, common, arena) : common;
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.infos.view, type);
    rc_wgsl_typeinfo t = rc_wgsl_check_typeinfo(c, type);

    if (!rc_wgsl_is_numeric_scalar_or_vector(c, type)) {
        return 0;
    }

    if (rule != RC_WGSL_RULE_DOT && !rc_wgsl_type_is_float_scalar(c->types.infos.view, scalar)) {
        return 0;
    }

    if (rule != RC_WGSL_RULE_LENGTH && t.type != RC_WGSL_TYPEINFO_TYPE_VECTOR) {
        return 0;
    }

    if (rule == RC_WGSL_RULE_CROSS && t.width != 3) {
        return 0;
    }

    rc_wgsl_check_convert_args(c, args, 0, args.num, type, arena);
    if (rule == RC_WGSL_RULE_LENGTH || rule == RC_WGSL_RULE_DOT) {
        return scalar;
    }

    return type;
}


// The result of refract(v, v, s), or mix(v, v, s) with a scalar blend, or 0.
static uint32_t rc_wgsl_vector_scalar_rule(rc_wgsl_checker *c, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t vector = rc_wgsl_floated(c, rc_wgsl_check_common_arg_type(c, args, 0, 2, false), arena);
    uint32_t scalar = rc_wgsl_floated(c, rc_wgsl_check_arg_type(c, args, 2), arena);
    uint32_t element = rc_wgsl_type_scalar_of(c->types.infos.view, vector);

    if (vector == 0 || rc_wgsl_check_typeinfo_type(c, vector) != RC_WGSL_TYPEINFO_TYPE_VECTOR) {
        return 0;
    }

    if (!rc_wgsl_type_is_scalar(c->types.infos.view, scalar)) {
        return 0;
    }

    // The vector and the scalar settle on the more specific of their element types
    if (rc_wgsl_type_converts(c->types.infos.view, element, scalar)) {
        element = scalar;
    }
    else if (!rc_wgsl_type_converts(c->types.infos.view, scalar, element)) {
        return 0;
    }

    if (!rc_wgsl_type_is_float_scalar(c->types.infos.view, element)) {
        return 0;
    }

    vector = rc_wgsl_type_with_scalar(&c->types, vector, element, arena);
    rc_wgsl_check_convert_args(c, args, 0, 2, vector, arena);
    rc_wgsl_check_convert_args(c, args, 2, 3, element, arena);
    return vector;
}


// The result of mix(a, b, blend), whose blend may be one scalar for vectors, or 0.
static uint32_t rc_wgsl_mix_rule(rc_wgsl_checker *c, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t value = rc_wgsl_check_arg_type(c, args, 0);
    uint32_t blend = rc_wgsl_check_arg_type(c, args, 2);

    if (rc_wgsl_type_is_scalar(c->types.infos.view, blend) && rc_wgsl_check_typeinfo_type(c, value) == RC_WGSL_TYPEINFO_TYPE_VECTOR) {
        return rc_wgsl_vector_scalar_rule(c, args, arena);
    }

    return rc_wgsl_same_type(c, RC_WGSL_RULE_FLOAT, args, arena);
}


// The result of a matrix built-in (DETERMINANT, TRANSPOSE), or 0.
static uint32_t rc_wgsl_matrix_rule(rc_wgsl_checker *c, rc_wgsl_rule rule, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t type = rc_wgsl_check_arg_type(c, args, 0);
    rc_wgsl_typeinfo t = rc_wgsl_check_typeinfo(c, type);

    if (t.type != RC_WGSL_TYPEINFO_TYPE_MATRIX) {
        return 0;
    }

    if (rule == RC_WGSL_RULE_DETERMINANT) {
        return t.columns == t.rows ? t.element : 0;
    }

    return rc_wgsl_type_matrix(&c->types, t.element, t.rows, t.columns, arena);
}


// True if condition can pick between values of type: a bool, or a vector of bools as wide as type.
static bool rc_wgsl_is_select_condition(const rc_wgsl_checker *c, uint32_t condition, uint32_t type)
{
    if (!rc_wgsl_is_bool_scalar_or_vector(c, condition)) {
        return false;
    }

    if (rc_wgsl_check_typeinfo_type(c, condition) != RC_WGSL_TYPEINFO_TYPE_VECTOR) {
        return true;
    }

    return rc_wgsl_type_width_of(c->types.infos.view, condition) == rc_wgsl_type_width_of(c->types.infos.view, type);
}


// The result of select(f, t, condition), or 0.
static uint32_t rc_wgsl_select_rule(rc_wgsl_checker *c, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t type = rc_wgsl_check_common_arg_type(c, args, 0, 2, false);
    uint32_t condition = rc_wgsl_check_arg_type(c, args, 2);

    if (type == 0 || !rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, type)) {
        return 0;
    }

    if (!rc_wgsl_is_select_condition(c, condition, type)) {
        return 0;
    }

    rc_wgsl_check_convert_args(c, args, 0, 2, type, arena);
    return type;
}


// The result of a built-in with one fixed argument type and result: the packs and unpacks.
static uint32_t rc_wgsl_fixed_rule(rc_wgsl_checker *c, rc_wgsl_rule rule, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t vec2f = rc_wgsl_type_vector(&c->types, RC_WGSL_TYPE_ID_F32, 2, arena);
    uint32_t from = rule == RC_WGSL_RULE_PACK ? vec2f : RC_WGSL_TYPE_ID_U32;

    if (!rc_wgsl_type_converts(c->types.infos.view, rc_wgsl_check_arg_type(c, args, 0), from)) {
        return 0;
    }

    rc_wgsl_check_convert_args(c, args, 0, 1, from, arena);
    return rule == RC_WGSL_RULE_PACK ? RC_WGSL_TYPE_ID_U32 : vec2f;
}


// True for i32, u32 and f32.
static bool rc_wgsl_is_concrete_numeric_scalar(const rc_wgsl_checker *c, uint32_t type)
{
    switch (rc_wgsl_check_typeinfo_type(c, type)) {
    case RC_WGSL_TYPEINFO_TYPE_I32:
    case RC_WGSL_TYPEINFO_TYPE_U32:
    case RC_WGSL_TYPEINFO_TYPE_F32:
        return true;
    default:
        return false;
    }
}


// True for i32, u32 and f32, and vectors of them: the types bitcast takes.
static bool rc_wgsl_is_bitcastable(const rc_wgsl_checker *c, uint32_t type)
{
    if (!rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, type)) {
        return false;
    }

    return rc_wgsl_is_concrete_numeric_scalar(c, rc_wgsl_type_scalar_of(c->types.infos.view, type));
}


// The type of argument i of args, given its default concrete type if it is abstract.
static uint32_t rc_wgsl_concrete_arg_type(rc_wgsl_checker *c, rc_view_wgsl_index args, uint32_t i, rc_arena *arena)
{
    rc_wgsl_check_concretise_default(c, rc_view_wgsl_index_get(args, i), arena);
    return rc_wgsl_check_arg_type(c, args, i);
}


// The result of bitcast<T>(e), with T the template argument target, or 0.
static uint32_t rc_wgsl_bitcast_to(rc_wgsl_checker *c, uint32_t target, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t type = rc_wgsl_check_type_expr(c, target, arena);
    uint32_t from = rc_wgsl_concrete_arg_type(c, args, 0, arena);

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }

    if (!rc_wgsl_is_bitcastable(c, type) || !rc_wgsl_is_bitcastable(c, from)) {
        return 0;
    }

    if (rc_wgsl_type_width_of(c->types.infos.view, type) != rc_wgsl_type_width_of(c->types.infos.view, from)) {
        return 0;
    }

    return type;
}


// The result of bitcast<T>(e), or 0.
static uint32_t rc_wgsl_bitcast_rule(rc_wgsl_checker *c, uint32_t node, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t callee = rc_wgsl_ast_node(c->ast, node).lhs;
    rc_view_wgsl_index templates = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, callee).lhs);

    if (templates.num != 1) {
        rc_wgsl_check_fail(c, callee, RC_STR("bitcast needs its type, as bitcast<u32>(x)"));
        return 0;
    }

    return rc_wgsl_bitcast_to(c, rc_view_wgsl_index_get(templates, 0), args, arena);
}


// The result of arrayLength(p), or 0.
static uint32_t rc_wgsl_array_length_rule(rc_wgsl_checker *c, rc_view_wgsl_index args)
{
    rc_wgsl_typeinfo pointer = rc_wgsl_check_typeinfo(c, rc_wgsl_check_arg_type(c, args, 0));
    rc_wgsl_typeinfo array = rc_wgsl_check_typeinfo(c, pointer.store_type);

    if (pointer.type != RC_WGSL_TYPEINFO_TYPE_POINTER || pointer.space != RC_WGSL_SPACE_STORAGE) {
        return 0;
    }

    if (array.type != RC_WGSL_TYPEINFO_TYPE_ARRAY || array.count != 0) {
        return 0;
    }

    return RC_WGSL_TYPE_ID_U32;
}

// ---- textures ----

// Reading a texture call's arguments in turn.
typedef struct rc_wgsl_texture_args {
    rc_view_wgsl_index args;
    uint32_t next;
    uint32_t node;
} rc_wgsl_texture_args;

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
static uint32_t rc_wgsl_texture_vector(rc_wgsl_checker *c, uint32_t element, uint32_t width, rc_arena *arena)
{
    return width == 1 ? element : rc_wgsl_type_vector(&c->types, element, width, arena);
}


// True if texture's class is one of classes, a bit set of rc_wgsl_texture_class.
static bool rc_wgsl_texture_class_in(rc_wgsl_typeinfo texture, uint32_t classes)
{
    return (classes & (1u << texture.texture_class)) != 0;
}


// True if texture is a texture of one of classes, holding f32 if float_only.
static bool rc_wgsl_texture_fits(rc_wgsl_typeinfo texture, uint32_t classes, bool float_only)
{
    if (texture.type != RC_WGSL_TYPEINFO_TYPE_TEXTURE || !rc_wgsl_texture_class_in(texture, classes)) {
        return false;
    }

    return !float_only || texture.sampled_type == RC_WGSL_TYPE_ID_F32;
}


// True for an i32 or u32, or a vector of them, holding width of them.
static bool rc_wgsl_is_integers(const rc_wgsl_checker *c, uint32_t type, uint32_t width)
{
    if (!rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, type)) {
        return false;
    }

    if (rc_wgsl_type_width_of(c->types.infos.view, type) != width) {
        return false;
    }

    return rc_wgsl_type_is_integer_scalar(c->types.infos.view, rc_wgsl_type_scalar_of(c->types.infos.view, type));
}


// True if a texture of dim takes an offset when sampled.
static bool rc_wgsl_dimension_takes_offset(rc_shader_dimension dim)
{
    switch (dim) {
    case RC_SHADER_DIMENSION_CUBE:
    case RC_SHADER_DIMENSION_CUBE_ARRAY:
    case RC_SHADER_DIMENSION_1D:
        return false;
    default:
        return true;
    }
}


// The next argument, which must convert to type.
static void rc_wgsl_texture_take(rc_wgsl_checker *c, rc_wgsl_texture_args *t, uint32_t type, rc_arena *arena)
{
    uint32_t arg = rc_wgsl_texture_next(c, t);

    if (arg != 0) {
        rc_wgsl_check_convert(c, arg, type, arena);
    }
}


// The next argument, which must be an i32 or u32, or a vector of width of them.
static void rc_wgsl_texture_take_int(rc_wgsl_checker *c, rc_wgsl_texture_args *t, uint32_t width, rc_arena *arena)
{
    uint32_t arg = rc_wgsl_texture_next(c, t);
    uint32_t type = rc_wgsl_check_sema(c, arg).type;

    if (arg == 0) {
        return;
    }

    if (!rc_wgsl_is_integers(c, type, width)) {
        rc_wgsl_check_fail_type(c, arg, RC_STR("expected i32 or u32 here, not "), type, RC_STR(""), arena);
        return;
    }

    rc_wgsl_check_concretise_default(c, arg, arena);
}


// The next argument, which there must be, which must convert to type and be known before the
// shader runs.
static void rc_wgsl_texture_take_const(rc_wgsl_checker *c, rc_wgsl_texture_args *t, uint32_t type, rc_arena *arena)
{
    uint32_t arg = rc_view_wgsl_index_get(t->args, t->next);

    rc_wgsl_texture_take(c, t, type, arena);
    if (!(rc_wgsl_check_sema(c, arg).flags & RC_WGSL_SEMA_CONST)) {
        rc_wgsl_check_fail(c, arg, RC_STR("an offset must be known before the shader runs"));
    }
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

    if (!rc_wgsl_texture_fits(texture, classes, float_only)) {
        rc_wgsl_check_fail_type(c, arg, RC_STR("this can't take "), type, RC_STR(""), arena);
    }

    return texture;
}


// The sampler argument, a comparison sampler if comparison.
static void rc_wgsl_texture_take_sampler(rc_wgsl_checker *c, rc_wgsl_texture_args *t, bool comparison, rc_arena *arena)
{
    uint32_t arg = rc_wgsl_texture_next(c, t);
    uint32_t type = rc_wgsl_check_sema(c, arg).type;
    uint32_t expected = comparison ? RC_WGSL_TYPE_ID_SAMPLER_COMPARISON : RC_WGSL_TYPE_ID_SAMPLER;

    if (arg != 0 && type != expected) {
        rc_wgsl_check_fail_types(c, arg, RC_STR("expected "), expected, RC_STR(", got "), type, arena);
    }
}


// The optional offset argument last, a constant vector of i32, if the texture's dim takes one.
static void rc_wgsl_texture_take_offset(rc_wgsl_checker *c, rc_wgsl_texture_args *t, rc_shader_dimension dim, rc_arena *arena)
{
    if (t->next >= t->args.num || !rc_wgsl_dimension_takes_offset(dim)) {
        return;
    }

    rc_wgsl_texture_take_const(c, t, rc_wgsl_type_vector(&c->types, RC_WGSL_TYPE_ID_I32, rc_wgsl_dimension_coords(dim), arena), arena);
}


// The coordinates, and the layer for an arrayed texture, for sampling.
static void rc_wgsl_texture_take_place(rc_wgsl_checker *c, rc_wgsl_texture_args *t, rc_wgsl_typeinfo texture, rc_arena *arena)
{
    rc_shader_dimension dim = (rc_shader_dimension)texture.dim;

    rc_wgsl_texture_take(c, t, rc_wgsl_texture_vector(c, RC_WGSL_TYPE_ID_F32, rc_wgsl_dimension_coords(dim), arena), arena);
    if (rc_wgsl_dimension_is_arrayed(dim)) {
        rc_wgsl_texture_take_int(c, t, 1, arena);
    }
}


// The texture classes as bits, for the sets of them each texture function takes.
enum {
    RC_WGSL_SAMPLED_BIT = 1u << RC_WGSL_TEXTURE_CLASS_SAMPLED,
    RC_WGSL_MULTISAMPLED_BIT = 1u << RC_WGSL_TEXTURE_CLASS_MULTISAMPLED,
    RC_WGSL_DEPTH_BIT = 1u << RC_WGSL_TEXTURE_CLASS_DEPTH,
    RC_WGSL_DEPTH_MULTISAMPLED_BIT = 1u << RC_WGSL_TEXTURE_CLASS_DEPTH_MULTISAMPLED,
    RC_WGSL_ANY_CLASS_BITS = RC_WGSL_SAMPLED_BIT | RC_WGSL_MULTISAMPLED_BIT | RC_WGSL_DEPTH_BIT | RC_WGSL_DEPTH_MULTISAMPLED_BIT
};

// textureSample, textureSampleBias, textureSampleLevel and textureSampleGrad.
static uint32_t rc_wgsl_check_texture_sample(rc_wgsl_checker *c, rc_wgsl_builtin builtin, rc_wgsl_texture_args *t, rc_arena *arena)
{
    bool depth_ok = builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE || builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL;
    rc_wgsl_typeinfo texture = rc_wgsl_texture_take_texture(c, t, RC_WGSL_SAMPLED_BIT | (depth_ok ? RC_WGSL_DEPTH_BIT : 0u), true, arena);
    bool depth = texture.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH;
    uint32_t coords = rc_wgsl_texture_vector(c, RC_WGSL_TYPE_ID_F32, rc_wgsl_dimension_coords((rc_shader_dimension)texture.dim), arena);

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

    rc_wgsl_texture_take_offset(c, t, (rc_shader_dimension)texture.dim, arena);
    return depth ? RC_WGSL_TYPE_ID_F32 : rc_wgsl_type_vector(&c->types, RC_WGSL_TYPE_ID_F32, 4, arena);
}


// textureSampleCompare and textureSampleCompareLevel.
static uint32_t rc_wgsl_check_texture_compare(rc_wgsl_checker *c, rc_wgsl_builtin builtin, rc_wgsl_texture_args *t, rc_arena *arena)
{
    rc_wgsl_typeinfo texture = rc_wgsl_texture_take_texture(c, t, RC_WGSL_DEPTH_BIT, false, arena);

    rc_wgsl_texture_take_sampler(c, t, true, arena);
    rc_wgsl_texture_take_place(c, t, texture, arena);
    rc_wgsl_texture_take(c, t, RC_WGSL_TYPE_ID_F32, arena);
    rc_wgsl_texture_take_offset(c, t, (rc_shader_dimension)texture.dim, arena);
    if (builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE) {
        rc_wgsl_check_note_fragment_only(c, t->node);
    }

    return RC_WGSL_TYPE_ID_F32;
}


// textureLoad of a sampled, multisampled or depth texture.
static uint32_t rc_wgsl_check_texture_load(rc_wgsl_checker *c, rc_wgsl_texture_args *t, rc_arena *arena)
{
    rc_wgsl_typeinfo texture = rc_wgsl_texture_take_texture(c, t, RC_WGSL_ANY_CLASS_BITS, false, arena);
    rc_shader_dimension dim = (rc_shader_dimension)texture.dim;
    bool depth = rc_wgsl_texture_class_in(texture, RC_WGSL_DEPTH_BIT | RC_WGSL_DEPTH_MULTISAMPLED_BIT);

    if (dim == RC_SHADER_DIMENSION_CUBE || dim == RC_SHADER_DIMENSION_CUBE_ARRAY) {
        rc_wgsl_check_fail(c, t->node, RC_STR("textureLoad can't read a cube texture"));
        return 0;
    }

    rc_wgsl_texture_take_int(c, t, rc_wgsl_dimension_coords(dim), arena);
    if (rc_wgsl_dimension_is_arrayed(dim)) {
        rc_wgsl_texture_take_int(c, t, 1, arena);
    }

    // The level, or for a multisampled texture the sample
    rc_wgsl_texture_take_int(c, t, 1, arena);
    return depth ? RC_WGSL_TYPE_ID_F32 : rc_wgsl_type_vector(&c->types, texture.sampled_type, 4, arena);
}


// textureDimensions, with its optional level.
static uint32_t rc_wgsl_check_texture_dimensions(rc_wgsl_checker *c, rc_wgsl_texture_args *t, rc_arena *arena)
{
    rc_wgsl_typeinfo texture = rc_wgsl_texture_take_texture(c, t, RC_WGSL_ANY_CLASS_BITS, false, arena);
    rc_shader_dimension dim = (rc_shader_dimension)texture.dim;
    bool multisampled = rc_wgsl_texture_class_in(texture, RC_WGSL_MULTISAMPLED_BIT | RC_WGSL_DEPTH_MULTISAMPLED_BIT);

    if (t->next < t->args.num && !multisampled) {
        rc_wgsl_texture_take_int(c, t, 1, arena);
    }

    return rc_wgsl_texture_vector(c, RC_WGSL_TYPE_ID_U32, dim == RC_SHADER_DIMENSION_1D ? 1 : dim == RC_SHADER_DIMENSION_3D ? 3 : 2, arena);
}


// textureNumLayers, textureNumLevels and textureNumSamples.
static uint32_t rc_wgsl_check_texture_count(rc_wgsl_checker *c, rc_wgsl_builtin builtin, rc_wgsl_texture_args *t, rc_arena *arena)
{
    bool samples = builtin == RC_WGSL_BUILTIN_TEXTURE_NUM_SAMPLES;
    uint32_t classes = samples ? RC_WGSL_MULTISAMPLED_BIT | RC_WGSL_DEPTH_MULTISAMPLED_BIT : RC_WGSL_SAMPLED_BIT | RC_WGSL_DEPTH_BIT;
    rc_wgsl_typeinfo texture = rc_wgsl_texture_take_texture(c, t, classes, false, arena);

    if (builtin == RC_WGSL_BUILTIN_TEXTURE_NUM_LAYERS && !rc_wgsl_dimension_is_arrayed((rc_shader_dimension)texture.dim)) {
        rc_wgsl_check_fail(c, t->node, RC_STR("only an arrayed texture has layers"));
    }

    return RC_WGSL_TYPE_ID_U32;
}


// A texture function's result, after checking its arguments one by one.
static uint32_t rc_wgsl_texture_result(rc_wgsl_checker *c, rc_wgsl_builtin builtin, rc_wgsl_texture_args *t, rc_arena *arena)
{
    switch (builtin) {
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE:
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE_LEVEL:
        return rc_wgsl_check_texture_compare(c, builtin, t, arena);
    case RC_WGSL_BUILTIN_TEXTURE_LOAD:
        return rc_wgsl_check_texture_load(c, t, arena);
    case RC_WGSL_BUILTIN_TEXTURE_DIMENSIONS:
        return rc_wgsl_check_texture_dimensions(c, t, arena);
    case RC_WGSL_BUILTIN_TEXTURE_NUM_LAYERS:
    case RC_WGSL_BUILTIN_TEXTURE_NUM_LEVELS:
    case RC_WGSL_BUILTIN_TEXTURE_NUM_SAMPLES:
        return rc_wgsl_check_texture_count(c, builtin, t, arena);
    default:
        return rc_wgsl_check_texture_sample(c, builtin, t, arena);
    }
}


// A texture function's result, failing if it is given more arguments than it takes.
static uint32_t rc_wgsl_check_texture_call(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_builtin builtin, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_texture_args t = {.args = args, .node = node};
    uint32_t result = rc_wgsl_texture_result(c, builtin, &t, arena);

    if (t.next < args.num) {
        rc_wgsl_check_fail(c, rc_view_wgsl_index_get(args, t.next), RC_STR("this is one argument too many"));
    }

    return result;
}

// ---- calls ----

// The result type of a call to builtin by its rule, or 0 if the arguments don't fit it.
static uint32_t rc_wgsl_builtin_result(rc_wgsl_checker *c, uint32_t node, rc_wgsl_builtin builtin, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_rule rule = (rc_wgsl_rule)rc_wgsl_builtin_info_of(builtin).rule;

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
        return rc_wgsl_mix_rule(c, args, arena);
    case RC_WGSL_RULE_DETERMINANT:
    case RC_WGSL_RULE_TRANSPOSE:
        return rc_wgsl_matrix_rule(c, rule, args, arena);
    case RC_WGSL_RULE_REDUCE:
        return rc_wgsl_is_bool_scalar_or_vector(c, rc_wgsl_check_arg_type(c, args, 0)) ? RC_WGSL_TYPE_ID_BOOL : 0;
    case RC_WGSL_RULE_SELECT:
        return rc_wgsl_select_rule(c, args, arena);
    case RC_WGSL_RULE_PACK:
    case RC_WGSL_RULE_UNPACK:
        return rc_wgsl_fixed_rule(c, rule, args, arena);
    case RC_WGSL_RULE_BITCAST:
        return rc_wgsl_bitcast_rule(c, node, args, arena);
    case RC_WGSL_RULE_ARRAY_LENGTH:
        return rc_wgsl_array_length_rule(c, args);
    case RC_WGSL_RULE_TEXTURE:
        return rc_wgsl_check_texture_call(c, node, builtin, args, arena);
    default:
        RC_UNREACHABLE();
        return 0;
    }
}


// Note which module-scope texture and sampler the texture function call with args uses, for the
// program of each entry point that reaches the function it is in. A texture or sampler passed in
// as a parameter is no pair that a target combining them could write.
static void rc_wgsl_note_texture_use(rc_wgsl_checker *c, rc_wgsl_builtin builtin, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_texture_use use = {
        .pair = {
            .texture = rc_wgsl_check_sema(c, rc_view_wgsl_index_get(args, 0)).target,
            .sampler = rc_wgsl_builtin_is_sampled(builtin) ? rc_wgsl_check_sema(c, rc_view_wgsl_index_get(args, 1)).target : 0
        },
        .function = c->function
    };

    if (rc_wgsl_ast_type(c->ast, use.pair.texture) != RC_WGSL_NODE_TYPE_VAR) {
        return;
    }

    if (use.pair.sampler != 0 && rc_wgsl_ast_type(c->ast, use.pair.sampler) != RC_WGSL_NODE_TYPE_VAR) {
        return;
    }

    rc_array_wgsl_texture_use_push(&c->texture_uses, use, arena);
}


// True for the rules of the built-ins that read the shader's state, and so are never const.
static bool rc_wgsl_rule_reads_state(rc_wgsl_rule rule)
{
    switch (rule) {
    case RC_WGSL_RULE_TEXTURE:
    case RC_WGSL_RULE_DERIVATIVE:
    case RC_WGSL_RULE_ARRAY_LENGTH:
        return true;
    default:
        return false;
    }
}


// What a call to builtin with args gives: a value of type, const when its arguments are, unless
// the built-in reads the shader's state.
static rc_wgsl_sema rc_wgsl_builtin_sema(const rc_wgsl_checker *c, rc_wgsl_builtin builtin, uint32_t type, rc_view_wgsl_index args)
{
    rc_wgsl_rule rule = (rc_wgsl_rule)rc_wgsl_builtin_info_of(builtin).rule;
    uint8_t flags = rc_wgsl_rule_reads_state(rule) ? 0 : rc_wgsl_check_args_flags(c, args);

    return (rc_wgsl_sema) {
        .type = type,
        .flags = (uint8_t)(flags & RC_WGSL_SEMA_CONST),
        .call = rule == RC_WGSL_RULE_BITCAST ? RC_WGSL_CALL_BITCAST : RC_WGSL_CALL_BUILTIN,
        .builtin = (uint8_t)builtin
    };
}


// A call to builtin with as many arguments as it takes, or any number for a texture function.
static rc_wgsl_sema rc_wgsl_check_builtin_args(rc_wgsl_checker *c, uint32_t node, rc_wgsl_builtin builtin,
    rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t type = rc_wgsl_builtin_result(c, node, builtin, args, arena);

    if (type == 0 && !rc_wgsl_check_failed(c)) {
        rc_wgsl_builtin_mismatch(c, node, builtin, args, arena);
    }

    if (rc_wgsl_check_failed(c)) {
        return (rc_wgsl_sema) {0};
    }

    if (rc_wgsl_builtin_is_texture(builtin)) {
        rc_wgsl_note_texture_use(c, builtin, args, arena);
    }

    return rc_wgsl_builtin_sema(c, builtin, type, args);
}


// Check a call to builtin with its checked arguments args.
rc_wgsl_sema rc_wgsl_check_builtin_call(rc_wgsl_checker *c, uint32_t node, rc_wgsl_builtin builtin,
    rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_builtin_info info = rc_wgsl_builtin_info_of(builtin);
    rc_wgsl_rule rule = (rc_wgsl_rule)info.rule;
    uint32_t callee = rc_wgsl_ast_node(c->ast, node).lhs;

    if (rule != RC_WGSL_RULE_BITCAST && rc_wgsl_ast_node(c->ast, callee).lhs != 0) {
        return rc_wgsl_check_fail(c, callee, RC_STR("this function takes no template list"));
    }

    if (rule != RC_WGSL_RULE_TEXTURE && args.num != info.args) {
        rc_wgsl_builtin_mismatch(c, node, builtin, args, arena);
        return (rc_wgsl_sema) {0};
    }

    return rc_wgsl_check_builtin_args(c, node, builtin, args, arena);
}
