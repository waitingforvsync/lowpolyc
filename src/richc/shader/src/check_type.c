// check_type.c
//
// Types written in the source, the integer constants they and attributes need, and attributes.

#include "checker.h"

#include "richc/mstr.h"

// ---- names in lists ----

// True if word is one of the space-separated words.
static bool rc_wgsl_is_one_of(rc_str words, rc_str word)
{
    while (!rc_str_is_empty(words)) {
        rc_str_pair split = rc_str_first_split(words, RC_STR(" "));

        if (rc_str_is_equal(split.first, word)) {
            return true;
        }
        words = split.second;
    }
    return false;
}


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

// ---- enumerants ----

uint32_t rc_wgsl_check_enumerant(rc_wgsl_checker *c, uint32_t node, rc_wgsl_enumerant set)
{
    static const rc_str expected[] = {
        [RC_WGSL_ENUMERANT_NONE] = RC_STR_INIT(""),
        [RC_WGSL_ENUMERANT_SPACE] = RC_STR_INIT("expected an address space, such as 'private' or 'uniform'"),
        [RC_WGSL_ENUMERANT_ACCESS] = RC_STR_INIT("expected an access mode: 'read', 'write' or 'read_write'"),
        [RC_WGSL_ENUMERANT_FORMAT] = RC_STR_INIT("expected a texel format")
    };
    rc_wgsl_found found = {0};

    if (rc_wgsl_ast_type(c->ast, node) == RC_WGSL_NODE_TYPE_IDENT
        && rc_wgsl_ast_node(c->ast, node).lhs == 0) {
        found = rc_wgsl_check_lookup(c, rc_wgsl_ast_text(c->ast, node));
    }
    if (found.node != 0 || found.predeclared.type != RC_WGSL_PREDECLARED_TYPE_ENUMERANT
        || found.predeclared.enumerant != set) {
        RC_ASSERT(set < RC_COUNT(expected));
        rc_wgsl_check_fail(c, node, expected[set]);
        return 0;
    }
    return found.predeclared.value;
}

// ---- generated types ----

// The template arguments in the list templates, which must number from min to max.
static rc_view_wgsl_index rc_wgsl_check_templates(rc_wgsl_checker *c, uint32_t node,
    uint32_t templates, uint32_t min, uint32_t max, rc_arena *arena)
{
    rc_view_wgsl_index args = rc_wgsl_ast_list(c->ast, templates);

    if (args.num < min || args.num > max) {
        rc_mstr message = rc_mstr_from_str(RC_STR("'"), 64, arena);

        rc_mstr_append(&message, rc_wgsl_ast_text(c->ast, node), arena);
        rc_mstr_append(&message, max == 0 ? RC_STR("' takes no template list")
                                          : RC_STR("' takes "), arena);
        if (max > 0) {
            rc_mstr_append_u32(&message, min, arena);
            if (max > min) {
                rc_mstr_append(&message, RC_STR(" or "), arena);
                rc_mstr_append_u32(&message, max, arena);
            }
            rc_mstr_append(&message, max == 1 ? RC_STR(" template argument")
                                              : RC_STR(" template arguments"), arena);
        }
        rc_wgsl_check_fail(c, node, message.view);
    }
    return args;
}


// The element type named by the template argument node: a concrete scalar, and f32 if
// float_only.
static uint32_t rc_wgsl_check_element(rc_wgsl_checker *c, uint32_t node, bool float_only,
    rc_arena *arena)
{
    uint32_t type = rc_wgsl_check_type_expr(c, node, arena);

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }
    if (float_only && type != RC_WGSL_TYPE_ID_F32) {
        rc_wgsl_check_fail_type(c, node, RC_STR("expected f32, got "), type, RC_STR(""), arena);
    }
    else if (!rc_wgsl_type_is_scalar(c->types.view, type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("expected a scalar type, got "), type, RC_STR(""),
            arena);
    }
    return type;
}


// The element of a vector or matrix: its shorthand's, or its template argument's.
static uint32_t rc_wgsl_check_shape_element(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_predeclared predeclared, uint32_t templates, bool float_only, rc_arena *arena)
{
    rc_view_wgsl_index args;

    if (predeclared.element != 0) {
        rc_wgsl_check_templates(c, node, templates, 0, 0, arena);
        return predeclared.element;
    }
    args = rc_wgsl_check_templates(c, node, templates, 1, 1, arena);
    if (rc_wgsl_check_failed(c)) {
        return 0;
    }
    return rc_wgsl_check_element(c, rc_view_wgsl_index_get(args, 0), float_only, arena);
}


// An array type: array<T> sized at run time, or array<T, N>.
static uint32_t rc_wgsl_check_array_type(rc_wgsl_checker *c, uint32_t node, uint32_t templates,
    rc_arena *arena)
{
    rc_view_wgsl_index args = rc_wgsl_check_templates(c, node, templates, 1, 2, arena);
    uint32_t element;
    int64_t count = 0;

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }
    element = rc_wgsl_check_type_expr(c, rc_view_wgsl_index_get(args, 0), arena);
    if (!rc_wgsl_check_failed(c) && !rc_wgsl_type_is_constructible(c->types.view, element)) {
        rc_wgsl_check_fail_type(c, rc_view_wgsl_index_get(args, 0), RC_STR("an array can't hold "),
            element, RC_STR(""), arena);
    }
    if (args.num == 2) {
        count = rc_wgsl_check_const_int(c, rc_view_wgsl_index_get(args, 1), 1, arena);
    }
    if (rc_wgsl_check_failed(c)) {
        return 0;
    }
    return rc_wgsl_type_array(&c->types, element, (uint32_t)count, arena);
}


// A pointer type: ptr<space, T> or ptr<space, T, access>.
static uint32_t rc_wgsl_check_pointer_type(rc_wgsl_checker *c, uint32_t node, uint32_t templates,
    rc_arena *arena)
{
    rc_view_wgsl_index args = rc_wgsl_check_templates(c, node, templates, 2, 3, arena);
    rc_wgsl_space space;
    uint32_t element;

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }
    space = (rc_wgsl_space)rc_wgsl_check_enumerant(c, rc_view_wgsl_index_get(args, 0),
        RC_WGSL_ENUMERANT_SPACE);
    element = rc_wgsl_check_type_expr(c, rc_view_wgsl_index_get(args, 1), arena);
    if (args.num == 3) {
        rc_wgsl_check_enumerant(c, rc_view_wgsl_index_get(args, 2), RC_WGSL_ENUMERANT_ACCESS);
        rc_wgsl_check_fail(c, rc_view_wgsl_index_get(args, 2),
            RC_STR("only pointers to storage take an access mode"));
    }
    if (!rc_wgsl_check_failed(c) && space != RC_WGSL_SPACE_FUNCTION
        && space != RC_WGSL_SPACE_PRIVATE) {
        rc_wgsl_check_fail(c, rc_view_wgsl_index_get(args, 0),
            RC_STR("only pointers to function and private memory are supported yet"));
    }
    if (rc_wgsl_check_failed(c)) {
        return 0;
    }
    return rc_wgsl_type_pointer(&c->types, element, space, RC_WGSL_ACCESS_READ_WRITE, arena);
}


// A sampled, multisampled or depth texture type.
static uint32_t rc_wgsl_check_texture_type(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_predeclared predeclared, uint32_t templates, rc_arena *arena)
{
    rc_wgsl_texture_class class = (rc_wgsl_texture_class)predeclared.texture_class;
    uint32_t element = RC_WGSL_TYPE_ID_F32;
    rc_wgsl_typeinfo type;

    if (class == RC_WGSL_TEXTURE_CLASS_SAMPLED || class == RC_WGSL_TEXTURE_CLASS_MULTISAMPLED) {
        rc_view_wgsl_index args = rc_wgsl_check_templates(c, node, templates, 1, 1, arena);

        if (rc_wgsl_check_failed(c)) {
            return 0;
        }
        element = rc_wgsl_check_element(c, rc_view_wgsl_index_get(args, 0), false, arena);
        if (!rc_wgsl_check_failed(c) && element == RC_WGSL_TYPE_ID_BOOL) {
            rc_wgsl_check_fail(c, rc_view_wgsl_index_get(args, 0),
                RC_STR("a texture holds f32, i32 or u32"));
        }
    }
    else {
        rc_wgsl_check_templates(c, node, templates, 0, 0, arena);
    }
    type = (rc_wgsl_typeinfo) {
        .element = element,
        .type = RC_WGSL_TYPEINFO_TYPE_TEXTURE,
        .dim = predeclared.dim,
        .texture_class = (uint8_t)class
    };
    return rc_wgsl_check_failed(c) ? 0 : rc_wgsl_type_intern(&c->types, type, arena);
}


uint32_t rc_wgsl_check_generated_type(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_predeclared predeclared, uint32_t templates, rc_arena *arena)
{
    uint32_t element;

    switch ((rc_wgsl_generator)predeclared.generator) {
    case RC_WGSL_GENERATOR_SCALAR:
        rc_wgsl_check_templates(c, node, templates, 0, 0, arena);
        return predeclared.element;
    case RC_WGSL_GENERATOR_VECTOR:
        element = rc_wgsl_check_shape_element(c, node, predeclared, templates, false, arena);
        return rc_wgsl_check_failed(c) ? 0
            : rc_wgsl_type_vector(&c->types, element, predeclared.width, arena);
    case RC_WGSL_GENERATOR_MATRIX:
        element = rc_wgsl_check_shape_element(c, node, predeclared, templates, true, arena);
        return rc_wgsl_check_failed(c) ? 0
            : rc_wgsl_type_matrix(&c->types, element, predeclared.columns,
                predeclared.rows, arena);
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
        return 0;
    }
}

// ---- type expressions ----

// The type a shader's own declaration found names, which must be a struct or alias.
static uint32_t rc_wgsl_check_declared_type(rc_wgsl_checker *c, uint32_t node, uint32_t found,
    rc_arena *arena)
{
    rc_wgsl_node_type node_type = rc_wgsl_ast_type(c->ast, found);

    if (node_type != RC_WGSL_NODE_TYPE_STRUCT && node_type != RC_WGSL_NODE_TYPE_ALIAS) {
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is not a type"), arena);
        return 0;
    }
    rc_wgsl_check_templates(c, node, rc_wgsl_ast_node(c->ast, node).lhs, 0, 0, arena);
    rc_wgsl_check_global(c, found, arena);
    return rc_wgsl_check_sema(c, found).type;
}


uint32_t rc_wgsl_check_type_expr(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_found found;
    uint32_t type = 0;

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }
    if (rc_wgsl_ast_type(c->ast, node) != RC_WGSL_NODE_TYPE_IDENT) {
        rc_wgsl_check_fail(c, node, RC_STR("expected a type"));
        return 0;
    }
    found = rc_wgsl_check_lookup(c, rc_wgsl_ast_text(c->ast, node));
    if (found.node != 0) {
        type = rc_wgsl_check_declared_type(c, node, found.node, arena);
    }
    else if (found.predeclared.type == RC_WGSL_PREDECLARED_TYPE_GENERATOR) {
        type = rc_wgsl_check_generated_type(c, node, found.predeclared,
            rc_wgsl_ast_node(c->ast, node).lhs, arena);
    }
    else if (found.predeclared.type == RC_WGSL_PREDECLARED_TYPE_UNSUPPORTED) {
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' isn't supported yet"), arena);
    }
    else if (found.predeclared.type == RC_WGSL_PREDECLARED_TYPE_NONE) {
        rc_wgsl_check_fail_name(c, node, RC_STR("there is no type called '"), RC_STR("'"), arena);
    }
    else {
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is not a type"), arena);
    }
    if (rc_wgsl_check_failed(c)) {
        return 0;
    }
    rc_wgsl_check_set(c, node, (rc_wgsl_sema) {.type = type, .target = found.node});
    return type;
}

// ---- constants ----

int64_t rc_wgsl_check_const_int(rc_wgsl_checker *c, uint32_t node, int64_t minimum,
    rc_arena *arena)
{
    rc_wgsl_sema sema = rc_wgsl_check_expr(c, node, arena);

    if (rc_wgsl_check_failed(c)) {
        return 0;
    }
    if (!rc_wgsl_type_is_integer_scalar(c->types.view, sema.type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("expected an integer, got "), sema.type,
            RC_STR(""), arena);
    }
    else if (!(sema.flags & RC_WGSL_SEMA_VALUE)) {
        rc_wgsl_check_fail(c, node, RC_STR("this must be an integer known before the shader runs"));
    }
    else if (sema.value < minimum || sema.value > UINT32_MAX) {
        rc_mstr message = rc_mstr_from_str(RC_STR("this must be from "), 64, arena);

        rc_mstr_append_i64(&message, minimum, arena);
        rc_mstr_append(&message, RC_STR(" to 4294967295"), arena);
        rc_wgsl_check_fail(c, node, message.view);
    }
    return sema.value;
}

// ---- attributes ----

// The value of the name argument node, from names, recorded as its value; or fail with message.
static void rc_wgsl_check_named_arg(rc_wgsl_checker *c, uint32_t node, const rc_str *names,
    uint32_t num, rc_str message)
{
    uint32_t i = RC_INDEX_NONE;

    if (rc_wgsl_ast_type(c->ast, node) == RC_WGSL_NODE_TYPE_IDENT
        && rc_wgsl_ast_node(c->ast, node).lhs == 0) {
        i = rc_wgsl_index_of(names, num, rc_wgsl_ast_text(c->ast, node));
    }
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
        [RC_WGSL_VALUE_NONE] = RC_STR_INIT(""),
        [RC_WGSL_VALUE_POSITION] = RC_STR_INIT("position"),
        [RC_WGSL_VALUE_VERTEX_INDEX] = RC_STR_INIT("vertex_index"),
        [RC_WGSL_VALUE_INSTANCE_INDEX] = RC_STR_INIT("instance_index"),
        [RC_WGSL_VALUE_FRONT_FACING] = RC_STR_INIT("front_facing"),
        [RC_WGSL_VALUE_FRAG_DEPTH] = RC_STR_INIT("frag_depth"),
        [RC_WGSL_VALUE_SAMPLE_INDEX] = RC_STR_INIT("sample_index"),
        [RC_WGSL_VALUE_SAMPLE_MASK] = RC_STR_INIT("sample_mask")
    };

    rc_wgsl_check_named_arg(c, node, names, RC_COUNT(names),
        RC_STR("expected a built-in value, such as 'position'"));
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


// Check an attribute's arguments: how many, and what they hold.
static void rc_wgsl_check_attribute_args(rc_wgsl_checker *c, uint32_t attribute, rc_arena *arena)
{
    rc_str name = rc_wgsl_ast_text(c->ast, attribute);
    rc_view_wgsl_index args = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, attribute).lhs);
    bool none = rc_wgsl_is_one_of(RC_STR("vertex fragment compute must_use invariant"), name);
    uint32_t min = none ? 0 : 1;
    uint32_t max = none ? 0 : rc_str_is_equal(name, RC_STR("workgroup_size")) ? 3
                 : rc_str_is_equal(name, RC_STR("interpolate")) ? 2 : 1;

    if (args.num < min || args.num > max) {
        rc_wgsl_check_fail_name(c, attribute, RC_STR("@"),
            none ? RC_STR(" takes no arguments") : RC_STR(" has the wrong number of arguments"),
            arena);
        return;
    }
    if (rc_str_is_equal(name, RC_STR("builtin"))) {
        rc_wgsl_check_builtin_arg(c, rc_view_wgsl_index_get(args, 0));
    }
    else if (rc_str_is_equal(name, RC_STR("interpolate"))) {
        rc_wgsl_check_interpolate_args(c, args);
    }
    else {
        int64_t minimum = rc_wgsl_is_one_of(RC_STR("location group binding"), name) ? 0 : 1;

        for (uint32_t i = 0; i < args.num; i++) {
            rc_wgsl_check_const_int(c, rc_view_wgsl_index_get(args, i), minimum, arena);
        }
    }
}


// Fail if the attribute at index i of list repeats one before it.
static void rc_wgsl_check_attribute_repeat(rc_wgsl_checker *c, rc_view_wgsl_index list,
    uint32_t i, rc_arena *arena)
{
    uint32_t attribute = rc_view_wgsl_index_get(list, i);

    for (uint32_t j = 0; j < i; j++) {
        if (rc_str_is_equal(rc_wgsl_ast_text(c->ast, rc_view_wgsl_index_get(list, j)),
            rc_wgsl_ast_text(c->ast, attribute))) {
            rc_wgsl_check_fail_name(c, attribute, RC_STR("@"), RC_STR(" is here already"), arena);
        }
    }
}


void rc_wgsl_check_attributes(rc_wgsl_checker *c, uint32_t attributes, rc_str allowed,
    rc_arena *arena)
{
    static const rc_str known = RC_STR_INIT("align binding blend_src builtin compute const "
        "diagnostic fragment group id interpolate invariant location must_use size vertex "
        "workgroup_size");
    rc_view_wgsl_index list = rc_wgsl_ast_list(c->ast, attributes);

    for (uint32_t i = 0; i < list.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t attribute = rc_view_wgsl_index_get(list, i);
        rc_str name = rc_wgsl_ast_text(c->ast, attribute);

        if (!rc_wgsl_is_one_of(known, name)) {
            rc_wgsl_check_fail_name(c, attribute, RC_STR("there is no attribute @"), RC_STR(""),
                arena);
        }
        else if (!rc_wgsl_is_one_of(allowed, name)) {
            rc_wgsl_check_fail_name(c, attribute, RC_STR("@"), RC_STR(" can't go here"), arena);
        }
        else {
            rc_wgsl_check_attribute_repeat(c, list, i, arena);
            rc_wgsl_check_attribute_args(c, attribute, arena);
        }
    }
}


uint32_t rc_wgsl_attribute_find(rc_wgsl_ast ast, uint32_t attributes, rc_str name)
{
    rc_view_wgsl_index list = rc_wgsl_ast_list(ast, attributes);

    for (uint32_t i = 0; i < list.num; i++) {
        uint32_t attribute = rc_view_wgsl_index_get(list, i);

        if (rc_str_is_equal(rc_wgsl_ast_text(ast, attribute), name)) {
            return attribute;
        }
    }
    return 0;
}


// The value of argument i of the attribute name in attributes, or fallback if there is no such
// attribute or argument.
static int64_t rc_wgsl_attribute_value(rc_wgsl_ast ast, rc_view_wgsl_sema sema,
    uint32_t attributes, rc_str name, uint32_t i, int64_t fallback)
{
    uint32_t attribute = rc_wgsl_attribute_find(ast, attributes, name);
    rc_view_wgsl_index args = rc_wgsl_ast_list(ast, rc_wgsl_ast_node(ast, attribute).lhs);

    if (attribute == 0 || i >= args.num) {
        return fallback;
    }
    return rc_view_wgsl_sema_get(sema, rc_view_wgsl_index_get(args, i)).value;
}


rc_wgsl_io rc_wgsl_io_of(rc_wgsl_ast ast, rc_view_wgsl_sema sema, uint32_t attributes)
{
    int64_t location = rc_wgsl_attribute_value(ast, sema, attributes, RC_STR("location"), 0, -1);

    return (rc_wgsl_io) {
        .location = location < 0 ? RC_INDEX_NONE : (uint32_t)location,
        .builtin = (uint8_t)rc_wgsl_attribute_value(ast, sema, attributes, RC_STR("builtin"), 0,
            0),
        .interpolation = (uint8_t)rc_wgsl_attribute_value(ast, sema, attributes,
            RC_STR("interpolate"), 0, 0),
        .sampling = (uint8_t)rc_wgsl_attribute_value(ast, sema, attributes, RC_STR("interpolate"),
            1, 0),
        .invariant = rc_wgsl_attribute_find(ast, attributes, RC_STR("invariant")) != 0
    };
}


rc_wgsl_binding rc_wgsl_binding_of(rc_wgsl_ast ast, rc_view_wgsl_sema sema, uint32_t var)
{
    uint32_t attributes = rc_wgsl_ast_var_parts(ast, var).attributes;

    return (rc_wgsl_binding) {
        .group = (uint32_t)rc_wgsl_attribute_value(ast, sema, attributes, RC_STR("group"), 0, 0),
        .binding = (uint32_t)rc_wgsl_attribute_value(ast, sema, attributes, RC_STR("binding"), 0, 0)
    };
}
