// check_type.c
//
// Types written in the source, the integer constants they and attributes need, and attributes.

#include "checker.h"

#include "richc/mstr.h"

// ---- names in lists ----

// The index of name among names, or RC_INDEX_NONE.
static uint32_t rc_wgsl_index_of(const rc_str *names, uint32_t num, rc_str name)
{
    for (uint32_t i = 0; i < num; i++) {
        if (rc_str_is_equal(names[i], name)) {
            return i;
        }
    }
    return RC_INDEX_NONE;
}


// True if node is a plain name, with no template list.
static bool rc_wgsl_is_plain_name(const rc_wgsl_checker *c, uint32_t node)
{
    return rc_wgsl_ast_type(c->ast, node) == RC_WGSL_NODE_TYPE_IDENT && rc_wgsl_ast_node(c->ast, node).lhs == 0;
}

// ---- enumerants ----

// True if found is a predeclared enumerant of set.
static bool rc_wgsl_is_enumerant_of(rc_wgsl_found found, rc_wgsl_enumerant set)
{
    if (found.node != 0 || found.predeclared.type != RC_WGSL_PREDECLARED_TYPE_ENUMERANT) {
        return false;
    }

    return found.predeclared.enumerant == set;
}


// The value of the enumerant of set that node names, or 0 having failed.
uint32_t rc_wgsl_check_enumerant(rc_wgsl_checker *c, uint32_t node, rc_wgsl_enumerant set)
{
    static const rc_str expected[] = {
        [RC_WGSL_ENUMERANT_NONE] = RC_STR_INIT(""),
        [RC_WGSL_ENUMERANT_SPACE] = RC_STR_INIT("expected an address space, such as 'private' or 'uniform'"),
        [RC_WGSL_ENUMERANT_ACCESS] = RC_STR_INIT("expected an access mode: 'read', 'write' or 'read_write'"),
        [RC_WGSL_ENUMERANT_FORMAT] = RC_STR_INIT("expected a texel format")
    };
    rc_wgsl_found found = rc_wgsl_is_plain_name(c, node) ? rc_wgsl_check_lookup(c, rc_wgsl_ast_text(c->ast, node)) : (rc_wgsl_found) {0};

    if (!rc_wgsl_is_enumerant_of(found, set)) {
        RC_ASSERT((uint32_t)set < RC_COUNT(expected));
        rc_wgsl_check_fail(c, node, expected[set]);
        return 0;
    }

    return found.predeclared.value;
}

// ---- generated types ----

// Fail at node, which takes from min to max template arguments but was given another number.
static void rc_wgsl_fail_template_count(rc_wgsl_checker *c, uint32_t node, uint32_t min, uint32_t max, rc_arena *arena)
{
    rc_mstr message = rc_mstr_from_str(RC_STR("'"), 64, arena);

    rc_mstr_append(&message, rc_wgsl_ast_text(c->ast, node), arena);
    rc_mstr_append(&message, max == 0 ? RC_STR("' takes no template list") : RC_STR("' takes "), arena);
    if (max > 0) {
        rc_mstr_append_u32(&message, min, arena);
        if (max > min) {
            rc_mstr_append(&message, RC_STR(" or "), arena);
            rc_mstr_append_u32(&message, max, arena);
        }

        rc_mstr_append(&message, max == 1 ? RC_STR(" template argument") : RC_STR(" template arguments"), arena);
    }

    rc_wgsl_check_fail(c, node, message.view);
}


// The template arguments in the list templates, which must number from min to max.
static rc_view_wgsl_index rc_wgsl_check_templates(rc_wgsl_checker *c, uint32_t node,
    uint32_t templates, uint32_t min, uint32_t max, rc_arena *arena)
{
    rc_view_wgsl_index args = rc_wgsl_ast_list(c->ast, templates);

    if (args.num < min || args.num > max) {
        rc_wgsl_fail_template_count(c, node, min, max, arena);
    }

    return args;
}


// The element type the template argument node names: a concrete scalar, and f32 if float_only.
static uint32_t rc_wgsl_check_element(rc_wgsl_checker *c, uint32_t node, bool float_only, rc_arena *arena)
{
    uint32_t type = rc_wgsl_check_type_expr(c, node, arena);

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }

    if (float_only && type != RC_WGSL_TYPE_ID_F32) {
        rc_wgsl_check_fail_type(c, node, RC_STR("expected f32, got "), type, RC_STR(""), arena);
    }
    else if (!rc_wgsl_type_is_scalar(c->types.infos.view, type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("expected a scalar type, got "), type, RC_STR(""), arena);
    }

    return type;
}


// The element type named by the one template argument in the list templates.
static uint32_t rc_wgsl_check_template_element(rc_wgsl_checker *c, uint32_t node, uint32_t templates, bool float_only, rc_arena *arena)
{
    rc_view_wgsl_index args = rc_wgsl_check_templates(c, node, templates, 1, 1, arena);

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }

    return rc_wgsl_check_element(c, rc_view_wgsl_index_get(args, 0), float_only, arena);
}


// The element of a vector or matrix: its shorthand's, or its template argument's.
static uint32_t rc_wgsl_check_shape_element(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_predeclared predeclared, uint32_t templates, bool float_only, rc_arena *arena)
{
    if (predeclared.element != 0) {
        rc_wgsl_check_templates(c, node, templates, 0, 0, arena);
        return predeclared.element;
    }

    return rc_wgsl_check_template_element(c, node, templates, float_only, arena);
}


// A vector type: vec3<T>, or a shorthand such as vec3f.
static uint32_t rc_wgsl_check_vector_type(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_predeclared predeclared, uint32_t templates, rc_arena *arena)
{
    uint32_t element = rc_wgsl_check_shape_element(c, node, predeclared, templates, false, arena);

    return rc_wgsl_check_failed(c) ? 0 : rc_wgsl_type_vector(&c->types, element, predeclared.width, arena);
}


// A matrix type: mat2x3<T>, or a shorthand such as mat2x3f.
static uint32_t rc_wgsl_check_matrix_type(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_predeclared predeclared, uint32_t templates, rc_arena *arena)
{
    uint32_t element = rc_wgsl_check_shape_element(c, node, predeclared, templates, true, arena);

    return rc_wgsl_check_failed(c) ? 0 : rc_wgsl_type_matrix(&c->types, element, predeclared.columns, predeclared.rows, arena);
}


// The element type of an array, named by node, which must be constructible.
static uint32_t rc_wgsl_check_array_element(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    uint32_t element = rc_wgsl_check_type_expr(c, node, arena);

    if (!rc_wgsl_check_failed(c) && !rc_wgsl_type_is_constructible(c->types.infos.view, element)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("an array can't hold "), element, RC_STR(""), arena);
    }

    return element;
}


// The array type its template arguments args make, of which there are one or two: the element
// type, and the count if it is sized before the shader runs.
static uint32_t rc_wgsl_check_array_args(rc_wgsl_checker *c, rc_view_wgsl_index args, rc_arena *arena)
{
    uint32_t element = rc_wgsl_check_array_element(c, rc_view_wgsl_index_get(args, 0), arena);
    int64_t count = args.num == 2 ? rc_wgsl_check_const_int(c, rc_view_wgsl_index_get(args, 1), 1, UINT32_MAX, arena) : 0;

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }

    return rc_wgsl_type_array(&c->types, element, (uint32_t)count, arena);
}


// An array type: array<T> sized at run time, or array<T, N>.
static uint32_t rc_wgsl_check_array_type(rc_wgsl_checker *c, uint32_t node, uint32_t templates, rc_arena *arena)
{
    rc_view_wgsl_index args = rc_wgsl_check_templates(c, node, templates, 1, 2, arena);

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }

    return rc_wgsl_check_array_args(c, args, arena);
}


// The pointer type its template arguments args make, of which there are two or three: the address
// space, the store type and the access mode, which we don't support yet.
static uint32_t rc_wgsl_check_pointer_args(rc_wgsl_checker *c, rc_view_wgsl_index args, rc_arena *arena)
{
    rc_wgsl_space space = (rc_wgsl_space)rc_wgsl_check_enumerant(c, rc_view_wgsl_index_get(args, 0), RC_WGSL_ENUMERANT_SPACE);
    uint32_t element = rc_wgsl_check_type_expr(c, rc_view_wgsl_index_get(args, 1), arena);

    if (args.num == 3) {
        rc_wgsl_check_enumerant(c, rc_view_wgsl_index_get(args, 2), RC_WGSL_ENUMERANT_ACCESS);
        rc_wgsl_check_fail(c, rc_view_wgsl_index_get(args, 2), RC_STR("only pointers to storage take an access mode"));
    }

    if (space != RC_WGSL_SPACE_FUNCTION && space != RC_WGSL_SPACE_PRIVATE) {
        rc_wgsl_check_fail(c, rc_view_wgsl_index_get(args, 0), RC_STR("only pointers to function and private memory are supported yet"));
    }

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }

    return rc_wgsl_type_pointer(&c->types, element, space, RC_WGSL_ACCESS_READ_WRITE, arena);
}


// A pointer type: ptr<space, T> or ptr<space, T, access>.
static uint32_t rc_wgsl_check_pointer_type(rc_wgsl_checker *c, uint32_t node, uint32_t templates, rc_arena *arena)
{
    rc_view_wgsl_index args = rc_wgsl_check_templates(c, node, templates, 2, 3, arena);

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }

    return rc_wgsl_check_pointer_args(c, args, arena);
}


// The type a sampled or multisampled texture holds, its one template argument: f32, i32 or u32.
static uint32_t rc_wgsl_check_sampled_type(rc_wgsl_checker *c, uint32_t node, uint32_t templates, rc_arena *arena)
{
    rc_view_wgsl_index args = rc_wgsl_check_templates(c, node, templates, 1, 1, arena);
    uint32_t element = rc_wgsl_check_failed(c) ? 0 : rc_wgsl_check_element(c, rc_view_wgsl_index_get(args, 0), false, arena);

    if (element == RC_WGSL_TYPE_ID_BOOL) {
        rc_wgsl_check_fail(c, rc_view_wgsl_index_get(args, 0), RC_STR("a texture holds f32, i32 or u32"));
    }

    return element;
}


// The type a texture of class holds: its template argument's for a sampled or multisampled one, and
// f32 for a depth one, which takes none.
static uint32_t rc_wgsl_check_texel_type(rc_wgsl_checker *c, uint32_t node, rc_wgsl_texture_class class,
    uint32_t templates, rc_arena *arena)
{
    if (class != RC_WGSL_TEXTURE_CLASS_SAMPLED && class != RC_WGSL_TEXTURE_CLASS_MULTISAMPLED) {
        rc_wgsl_check_templates(c, node, templates, 0, 0, arena);
        return RC_WGSL_TYPE_ID_F32;
    }

    return rc_wgsl_check_sampled_type(c, node, templates, arena);
}


// A sampled, multisampled or depth texture type.
static uint32_t rc_wgsl_check_texture_type(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_predeclared predeclared, uint32_t templates, rc_arena *arena)
{
    rc_wgsl_texture_class class = (rc_wgsl_texture_class)predeclared.texture_class;
    rc_wgsl_typeinfo type = {
        .sampled_type = rc_wgsl_check_texel_type(c, node, class, templates, arena),
        .type = RC_WGSL_TYPEINFO_TYPE_TEXTURE,
        .dim = predeclared.dim,
        .texture_class = (uint8_t)class
    };

    return rc_wgsl_check_failed(c) ? 0 : rc_wgsl_type_intern(&c->types, type, arena);
}


// The type the predeclared type generator node names, with the template list templates.
uint32_t rc_wgsl_check_generated_type(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_predeclared predeclared, uint32_t templates, rc_arena *arena)
{
    switch ((rc_wgsl_generator)predeclared.generator) {
    case RC_WGSL_GENERATOR_SCALAR:
        rc_wgsl_check_templates(c, node, templates, 0, 0, arena);
        return predeclared.element;
    case RC_WGSL_GENERATOR_VECTOR:
        return rc_wgsl_check_vector_type(c, node, predeclared, templates, arena);
    case RC_WGSL_GENERATOR_MATRIX:
        return rc_wgsl_check_matrix_type(c, node, predeclared, templates, arena);
    case RC_WGSL_GENERATOR_ARRAY:
        return rc_wgsl_check_array_type(c, node, templates, arena);
    case RC_WGSL_GENERATOR_POINTER:
        return rc_wgsl_check_pointer_type(c, node, templates, arena);
    case RC_WGSL_GENERATOR_TEXTURE:
        return rc_wgsl_check_texture_type(c, node, predeclared, templates, arena);
    case RC_WGSL_GENERATOR_ATOMIC:
        rc_wgsl_check_fail(c, node, RC_STR("atomics aren't supported yet"));
        return 0;
    case RC_WGSL_GENERATOR_STORAGE_TEXTURE:
        rc_wgsl_check_fail(c, node, RC_STR("storage textures aren't supported yet"));
        return 0;
    default:
        RC_UNREACHABLE();
    }
}

// ---- type expressions ----

// The type a shader's own declaration found names, which must be a struct or alias.
static uint32_t rc_wgsl_check_declared_type(rc_wgsl_checker *c, uint32_t node, uint32_t found, rc_arena *arena)
{
    rc_wgsl_node_type node_type = rc_wgsl_ast_type(c->ast, found);

    if (node_type != RC_WGSL_NODE_TYPE_STRUCT && node_type != RC_WGSL_NODE_TYPE_ALIAS) {
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is not a type"), arena);
        return 0;
    }

    rc_wgsl_check_templates(c, node, rc_wgsl_ast_node(c->ast, node).lhs, 0, 0, arena);
    rc_wgsl_check_global(c, found, node, arena);
    return rc_wgsl_check_sema(c, found).type;
}


// The type the name node names, found by looking it up; or 0 having failed, unless it is a type.
static uint32_t rc_wgsl_found_type(rc_wgsl_checker *c, uint32_t node, rc_wgsl_found found, rc_arena *arena)
{
    if (found.node != 0) {
        return rc_wgsl_check_declared_type(c, node, found.node, arena);
    }

    if (found.predeclared.type == RC_WGSL_PREDECLARED_TYPE_GENERATOR) {
        return rc_wgsl_check_generated_type(c, node, found.predeclared, rc_wgsl_ast_node(c->ast, node).lhs, arena);
    }

    if (found.predeclared.type == RC_WGSL_PREDECLARED_TYPE_UNSUPPORTED) {
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' isn't supported yet"), arena);
    }
    else if (found.predeclared.type == RC_WGSL_PREDECLARED_TYPE_NONE) {
        rc_wgsl_check_fail_name(c, node, RC_STR("there is no type called '"), RC_STR("'"), arena);
    }
    else {
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is not a type"), arena);
    }

    return 0;
}


// The type the name node names, which we record on it.
static uint32_t rc_wgsl_check_type_name(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_found found = rc_wgsl_check_lookup(c, rc_wgsl_ast_text(c->ast, node));
    uint32_t type = rc_wgsl_found_type(c, node, found, arena);

    if (!rc_wgsl_check_failed(c) && rc_wgsl_check_typeinfo(c, type).depth > RC_WGSL_MAX_TYPE_DEPTH) {
        rc_wgsl_check_fail(c, node, RC_STR("this type nests other types too deeply"));
    }

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }

    rc_wgsl_check_set(c, node, (rc_wgsl_sema) {.type = type, .target = found.node});
    return type;
}


// The type the type expression node names, or 0 having failed.
uint32_t rc_wgsl_check_type_expr(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    if (rc_wgsl_check_failed(c)) {
        return 0;
    }

    if (rc_wgsl_ast_type(c->ast, node) != RC_WGSL_NODE_TYPE_IDENT) {
        rc_wgsl_check_fail(c, node, RC_STR("expected a type"));
        return 0;
    }

    return rc_wgsl_check_type_name(c, node, arena);
}

// ---- constants ----

// The value of node, which must be an integer known before the shader runs, at least minimum
// and at most maximum.
int64_t rc_wgsl_check_const_int(rc_wgsl_checker *c, uint32_t node, int64_t minimum, int64_t maximum, rc_arena *arena)
{
    rc_wgsl_sema sema = rc_wgsl_check_expr(c, node, arena);

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }

    if (!rc_wgsl_type_is_integer_scalar(c->types.infos.view, sema.type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("expected an integer, got "), sema.type, RC_STR(""), arena);
    }
    else if (!(sema.flags & RC_WGSL_SEMA_VALUE)) {
        rc_wgsl_check_fail(c, node, RC_STR("this must be an integer known before the shader runs"));
    }
    else if (sema.value < minimum || sema.value > maximum) {
        rc_mstr message = rc_mstr_from_str(RC_STR("this must be from "), 64, arena);

        rc_mstr_append_i64(&message, minimum, arena);
        rc_mstr_append(&message, RC_STR(" to "), arena);
        rc_mstr_append_i64(&message, maximum, arena);
        rc_wgsl_check_fail(c, node, message.view);
    }

    return sema.value;
}

// ---- attributes ----

// The value of the name argument node, from names, recorded as its value; or fail with message.
static void rc_wgsl_check_named_arg(rc_wgsl_checker *c, uint32_t node, const rc_str *names, uint32_t num, rc_str message)
{
    uint32_t i = rc_wgsl_is_plain_name(c, node) ? rc_wgsl_index_of(names, num, rc_wgsl_ast_text(c->ast, node)) : RC_INDEX_NONE;

    if (i == RC_INDEX_NONE || rc_str_is_empty(names[i])) {
        rc_wgsl_check_fail(c, node, message);
        return;
    }

    rc_wgsl_check_set(c, node, (rc_wgsl_sema) {.value = i, .flags = RC_WGSL_SEMA_VALUE});
}


// Check @builtin's argument, naming a built-in value.
static void rc_wgsl_check_builtin_arg(rc_wgsl_checker *c, uint32_t node)
{
    static const rc_str names[] = {
        [RC_WGSL_BUILTIN_VALUE_NONE] = RC_STR_INIT(""),
        [RC_WGSL_BUILTIN_VALUE_POSITION] = RC_STR_INIT("position"),
        [RC_WGSL_BUILTIN_VALUE_VERTEX_INDEX] = RC_STR_INIT("vertex_index"),
        [RC_WGSL_BUILTIN_VALUE_INSTANCE_INDEX] = RC_STR_INIT("instance_index"),
        [RC_WGSL_BUILTIN_VALUE_FRONT_FACING] = RC_STR_INIT("front_facing"),
        [RC_WGSL_BUILTIN_VALUE_FRAG_DEPTH] = RC_STR_INIT("frag_depth"),
        [RC_WGSL_BUILTIN_VALUE_SAMPLE_INDEX] = RC_STR_INIT("sample_index"),
        [RC_WGSL_BUILTIN_VALUE_SAMPLE_MASK] = RC_STR_INIT("sample_mask")
    };

    rc_wgsl_check_named_arg(c, node, names, RC_COUNT(names), RC_STR("expected a built-in value, such as 'position'"));
}


// Check @interpolate's arguments: a type, and maybe a sampling.
static void rc_wgsl_check_interpolate_args(rc_wgsl_checker *c, rc_view_wgsl_index args)
{
    static const rc_str types[] = {
        [RC_WGSL_INTERPOLATE_NONE] = RC_STR_INIT(""),
        [RC_WGSL_INTERPOLATE_PERSPECTIVE] = RC_STR_INIT("perspective"),
        [RC_WGSL_INTERPOLATE_LINEAR] = RC_STR_INIT("linear"),
        [RC_WGSL_INTERPOLATE_FLAT] = RC_STR_INIT("flat")
    };
    static const rc_str samplings[] = {
        [RC_WGSL_SAMPLING_NONE] = RC_STR_INIT(""),
        [RC_WGSL_SAMPLING_CENTER] = RC_STR_INIT("center"),
        [RC_WGSL_SAMPLING_CENTROID] = RC_STR_INIT("centroid"),
        [RC_WGSL_SAMPLING_SAMPLE] = RC_STR_INIT("sample"),
        [RC_WGSL_SAMPLING_FIRST] = RC_STR_INIT("first"),
        [RC_WGSL_SAMPLING_EITHER] = RC_STR_INIT("either")
    };

    rc_wgsl_check_named_arg(c, rc_view_wgsl_index_get(args, 0), types, RC_COUNT(types),
        RC_STR("expected 'perspective', 'linear' or 'flat'"));
    if (args.num > 1) {
        rc_wgsl_check_named_arg(c, rc_view_wgsl_index_get(args, 1), samplings, RC_COUNT(samplings),
            RC_STR("expected a sampling, such as 'centroid'"));
    }
}


// The range an attribute's integer arguments must be in.
typedef struct rc_wgsl_int_range {
    int64_t minimum;
    int64_t maximum;
} rc_wgsl_int_range;

// The range of attribute's integer arguments.
static rc_wgsl_int_range rc_wgsl_attribute_range(rc_wgsl_attribute attribute)
{
    switch (attribute) {
    case RC_WGSL_ATTRIBUTE_GROUP:
        return (rc_wgsl_int_range) {0, RC_WGSL_MAX_GROUPS - 1};
    case RC_WGSL_ATTRIBUTE_BINDING:
        return (rc_wgsl_int_range) {0, RC_WGSL_MAX_BINDINGS - 1};
    case RC_WGSL_ATTRIBUTE_LOCATION:
        return (rc_wgsl_int_range) {0, RC_WGSL_MAX_LOCATIONS - 1};
    default:
        return (rc_wgsl_int_range) {1, UINT32_MAX};
    }
}


// Check the integer arguments args of attribute, each a constant in its range.
static void rc_wgsl_check_int_args(rc_wgsl_checker *c, rc_view_wgsl_index args, rc_wgsl_attribute attribute, rc_arena *arena)
{
    rc_wgsl_int_range range = rc_wgsl_attribute_range(attribute);

    for (uint32_t i = 0; i < args.num; i++) {
        uint32_t arg = rc_view_wgsl_index_get(args, i);
        int64_t value = rc_wgsl_check_const_int(c, arg, range.minimum, range.maximum, arena);

        if (attribute == RC_WGSL_ATTRIBUTE_ALIGN && (value & (value - 1)) != 0) {
            rc_wgsl_check_fail(c, arg, RC_STR("@align must be a power of 2"));
        }
    }
}


// Check an attribute's arguments: how many, and what they hold.
static void rc_wgsl_check_attribute_args(rc_wgsl_checker *c, uint32_t node, rc_wgsl_attribute attribute, rc_arena *arena)
{
    // The fewest and most arguments each attribute takes
    static const uint8_t counts[][2] = {
        [RC_WGSL_ATTRIBUTE_NONE] = {0, 0},
        [RC_WGSL_ATTRIBUTE_ALIGN] = {1, 1},
        [RC_WGSL_ATTRIBUTE_BINDING] = {1, 1},
        [RC_WGSL_ATTRIBUTE_BLEND_SRC] = {1, 1},
        [RC_WGSL_ATTRIBUTE_BUILTIN] = {1, 1},
        [RC_WGSL_ATTRIBUTE_COMPUTE] = {0, 0},
        [RC_WGSL_ATTRIBUTE_CONST] = {0, 0},
        [RC_WGSL_ATTRIBUTE_DIAGNOSTIC] = {2, 2},
        [RC_WGSL_ATTRIBUTE_FRAGMENT] = {0, 0},
        [RC_WGSL_ATTRIBUTE_GROUP] = {1, 1},
        [RC_WGSL_ATTRIBUTE_ID] = {1, 1},
        [RC_WGSL_ATTRIBUTE_INTERPOLATE] = {1, 2},
        [RC_WGSL_ATTRIBUTE_INVARIANT] = {0, 0},
        [RC_WGSL_ATTRIBUTE_LOCATION] = {1, 1},
        [RC_WGSL_ATTRIBUTE_MUST_USE] = {0, 0},
        [RC_WGSL_ATTRIBUTE_SIZE] = {1, 1},
        [RC_WGSL_ATTRIBUTE_VERTEX] = {0, 0},
        [RC_WGSL_ATTRIBUTE_WORKGROUP_SIZE] = {1, 3}
    };
    rc_view_wgsl_index args = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, node).lhs);

    RC_ASSERT((uint32_t)attribute < RC_COUNT(counts));
    if (args.num < counts[attribute][0] || args.num > counts[attribute][1]) {
        rc_wgsl_check_fail_name(c, node, RC_STR("@"),
            counts[attribute][1] == 0 ? RC_STR(" takes no arguments") : RC_STR(" has the wrong number of arguments"), arena);
    }
    else if (attribute == RC_WGSL_ATTRIBUTE_BUILTIN) {
        rc_wgsl_check_builtin_arg(c, rc_view_wgsl_index_get(args, 0));
    }
    else if (attribute == RC_WGSL_ATTRIBUTE_INTERPOLATE) {
        rc_wgsl_check_interpolate_args(c, args);
    }
    else {
        rc_wgsl_check_int_args(c, args, attribute, arena);
    }
}


// The attribute node names, or none if WGSL has no such attribute.
static rc_wgsl_attribute rc_wgsl_attribute_named(const rc_wgsl_checker *c, uint32_t node)
{
    static const rc_str names[] = {
        [RC_WGSL_ATTRIBUTE_NONE] = RC_STR_INIT(""),
        [RC_WGSL_ATTRIBUTE_ALIGN] = RC_STR_INIT("align"),
        [RC_WGSL_ATTRIBUTE_BINDING] = RC_STR_INIT("binding"),
        [RC_WGSL_ATTRIBUTE_BLEND_SRC] = RC_STR_INIT("blend_src"),
        [RC_WGSL_ATTRIBUTE_BUILTIN] = RC_STR_INIT("builtin"),
        [RC_WGSL_ATTRIBUTE_COMPUTE] = RC_STR_INIT("compute"),
        [RC_WGSL_ATTRIBUTE_CONST] = RC_STR_INIT("const"),
        [RC_WGSL_ATTRIBUTE_DIAGNOSTIC] = RC_STR_INIT("diagnostic"),
        [RC_WGSL_ATTRIBUTE_FRAGMENT] = RC_STR_INIT("fragment"),
        [RC_WGSL_ATTRIBUTE_GROUP] = RC_STR_INIT("group"),
        [RC_WGSL_ATTRIBUTE_ID] = RC_STR_INIT("id"),
        [RC_WGSL_ATTRIBUTE_INTERPOLATE] = RC_STR_INIT("interpolate"),
        [RC_WGSL_ATTRIBUTE_INVARIANT] = RC_STR_INIT("invariant"),
        [RC_WGSL_ATTRIBUTE_LOCATION] = RC_STR_INIT("location"),
        [RC_WGSL_ATTRIBUTE_MUST_USE] = RC_STR_INIT("must_use"),
        [RC_WGSL_ATTRIBUTE_SIZE] = RC_STR_INIT("size"),
        [RC_WGSL_ATTRIBUTE_VERTEX] = RC_STR_INIT("vertex"),
        [RC_WGSL_ATTRIBUTE_WORKGROUP_SIZE] = RC_STR_INIT("workgroup_size")
    };
    uint32_t i = rc_wgsl_index_of(names, RC_COUNT(names), rc_wgsl_ast_text(c->ast, node));

    return i == RC_INDEX_NONE ? RC_WGSL_ATTRIBUTE_NONE : (rc_wgsl_attribute)i;
}


// Check the list attributes, each of which must be one of the allowed bits, and appear once.
void rc_wgsl_check_attributes(rc_wgsl_checker *c, uint32_t attributes, uint32_t allowed, rc_arena *arena)
{
    rc_view_wgsl_index list = rc_wgsl_ast_list(c->ast, attributes);
    uint32_t seen = 0;

    for (uint32_t i = 0; i < list.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t node = rc_view_wgsl_index_get(list, i);
        rc_wgsl_attribute attribute = rc_wgsl_attribute_named(c, node);
        uint32_t bit = 1u << attribute;

        if (attribute == RC_WGSL_ATTRIBUTE_NONE) {
            rc_wgsl_check_fail_name(c, node, RC_STR("there is no attribute @"), RC_STR(""), arena);
        }
        else if (!(allowed & bit)) {
            rc_wgsl_check_fail_name(c, node, RC_STR("@"), RC_STR(" can't go here"), arena);
        }
        else if (seen & bit) {
            rc_wgsl_check_fail_name(c, node, RC_STR("@"), RC_STR(" is here already"), arena);
        }
        else {
            rc_wgsl_check_set(c, node, (rc_wgsl_sema) {.attribute = (uint8_t)attribute});
            rc_wgsl_check_attribute_args(c, node, attribute, arena);
        }

        seen |= bit;
    }
}


// The node of attribute in the checked list attributes, or 0.
static uint32_t rc_wgsl_attribute_find(rc_wgsl_ast ast, rc_view_wgsl_sema sema, uint32_t attributes, rc_wgsl_attribute attribute)
{
    rc_view_wgsl_index list = rc_wgsl_ast_list(ast, attributes);

    for (uint32_t i = 0; i < list.num; i++) {
        uint32_t node = rc_view_wgsl_index_get(list, i);

        if (rc_view_wgsl_sema_get(sema, node).attribute == attribute) {
            return node;
        }
    }
    return 0;
}


// The value of argument i of the attribute node, or fallback if it has no such argument.
static int64_t rc_wgsl_attribute_arg(rc_wgsl_ast ast, rc_view_wgsl_sema sema, uint32_t node, uint32_t i, int64_t fallback)
{
    rc_view_wgsl_index args = rc_wgsl_ast_list(ast, rc_wgsl_ast_node(ast, node).lhs);

    return i < args.num ? rc_view_wgsl_sema_get(sema, rc_view_wgsl_index_get(args, i)).value : fallback;
}


// True if the checked list attributes holds attribute.
bool rc_wgsl_has_attribute(rc_wgsl_ast ast, rc_view_wgsl_sema sema, uint32_t attributes, rc_wgsl_attribute attribute)
{
    return rc_wgsl_attribute_find(ast, sema, attributes, attribute) != 0;
}


// The first argument of attribute in the checked list attributes, or fallback if it isn't there.
int64_t rc_wgsl_attribute_value(rc_wgsl_ast ast, rc_view_wgsl_sema sema, uint32_t attributes, rc_wgsl_attribute attribute, int64_t fallback)
{
    uint32_t node = rc_wgsl_attribute_find(ast, sema, attributes, attribute);

    return node != 0 ? rc_wgsl_attribute_arg(ast, sema, node, 0, fallback) : fallback;
}


// What the checked list attributes says about an input or output.
rc_wgsl_io rc_wgsl_io_of(rc_wgsl_ast ast, rc_view_wgsl_sema sema, uint32_t attributes)
{
    rc_view_wgsl_index list = rc_wgsl_ast_list(ast, attributes);
    rc_wgsl_io io = {.location = RC_INDEX_NONE};

    for (uint32_t i = 0; i < list.num; i++) {
        uint32_t node = rc_view_wgsl_index_get(list, i);

        switch ((rc_wgsl_attribute)rc_view_wgsl_sema_get(sema, node).attribute) {
        case RC_WGSL_ATTRIBUTE_LOCATION:
            io.location = (uint32_t)rc_wgsl_attribute_arg(ast, sema, node, 0, 0);
            break;
        case RC_WGSL_ATTRIBUTE_BUILTIN:
            io.builtin_value = (uint8_t)rc_wgsl_attribute_arg(ast, sema, node, 0, 0);
            break;
        case RC_WGSL_ATTRIBUTE_INTERPOLATE:
            io.interpolation = (uint8_t)rc_wgsl_attribute_arg(ast, sema, node, 0, 0);
            io.sampling = (uint8_t)rc_wgsl_attribute_arg(ast, sema, node, 1, 0);
            break;
        case RC_WGSL_ATTRIBUTE_INVARIANT:
            io.invariant = true;
            break;
        default:
            break;
        }
    }
    return io;
}


// The @group and @binding of the checked module-scope var.
rc_wgsl_binding rc_wgsl_binding_of(rc_wgsl_ast ast, rc_view_wgsl_sema sema, uint32_t var)
{
    uint32_t attributes = rc_wgsl_ast_var_parts(ast, var).attributes;

    return (rc_wgsl_binding) {
        .group = (uint32_t)rc_wgsl_attribute_value(ast, sema, attributes, RC_WGSL_ATTRIBUTE_GROUP, 0),
        .binding = (uint32_t)rc_wgsl_attribute_value(ast, sema, attributes, RC_WGSL_ATTRIBUTE_BINDING, 0)
    };
}
