// check_decl.c
//
// Declarations: structs, aliases, consts and lets, module-scope vars, function signatures and
// const_asserts.

#include "checker.h"

// ---- structs ----

// Fail if item i of list repeats the name of an item before it, saying after the name.
static void rc_wgsl_check_unique_name(rc_wgsl_checker *c, rc_view_wgsl_index list, uint32_t i,
    rc_str after, rc_arena *arena)
{
    uint32_t item = rc_view_wgsl_index_get(list, i);

    for (uint32_t j = 0; j < i; j++) {
        uint32_t other = rc_view_wgsl_index_get(list, j);

        if (rc_str_is_equal(rc_wgsl_ast_text(c->ast, other), rc_wgsl_ast_text(c->ast, item))) {
            rc_wgsl_check_fail_name(c, item, RC_STR("'"), after, arena);
        }
    }
}


// Check field i of a struct, returning its type.
static uint32_t rc_wgsl_check_field(rc_wgsl_checker *c, rc_view_wgsl_index fields, uint32_t i,
    rc_arena *arena)
{
    uint32_t field = rc_view_wgsl_index_get(fields, i);
    rc_wgsl_node f = rc_wgsl_ast_node(c->ast, field);
    uint32_t type = rc_wgsl_check_type_expr(c, f.rhs, arena);
    rc_wgsl_typeinfo t = rc_wgsl_check_typeinfo(c, type);
    bool runtime_array = t.type == RC_WGSL_TYPEINFO_TYPE_ARRAY && t.count == 0;

    rc_wgsl_check_attributes(c, f.lhs, RC_STR("align size location builtin interpolate invariant"),
        arena);
    rc_wgsl_check_unique_name(c, fields, i, RC_STR("' is already a member"), arena);
    if (runtime_array && i + 1 < fields.num) {
        rc_wgsl_check_fail(c, field, RC_STR("only the last member can be sized at run time"));
    }
    else if (!runtime_array && !rc_wgsl_type_is_constructible(c->types.view, type)
        && t.type != RC_WGSL_TYPEINFO_TYPE_STRUCT) {
        rc_wgsl_check_fail_type(c, f.rhs, RC_STR("a struct can't hold "), type, RC_STR(""), arena);
    }
    rc_wgsl_check_set_type(c, field, type);
    return type;
}


// Check a struct and its fields, and give it its type.
static void rc_wgsl_check_struct(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_view_wgsl_index fields = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, node).lhs);
    bool uncopyable = false;
    rc_wgsl_typeinfo type;

    for (uint32_t i = 0; i < fields.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t field_type = rc_wgsl_check_field(c, fields, i, arena);

        uncopyable |= !rc_wgsl_type_is_constructible(c->types.view, field_type);
    }
    type = (rc_wgsl_typeinfo) {
        .decl = node,
        .type = RC_WGSL_TYPEINFO_TYPE_STRUCT,
        .uncopyable = uncopyable
    };
    rc_wgsl_check_set_type(c, node, rc_wgsl_type_intern(&c->types, type, arena));
}

// ---- values ----

void rc_wgsl_check_value_decl(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    bool is_const = n.type == RC_WGSL_NODE_TYPE_CONST;
    uint32_t declared = n.lhs != 0 ? rc_wgsl_check_type_expr(c, n.lhs, arena) : 0;
    uint32_t type = rc_wgsl_check_value(c, n.rhs, arena);
    rc_wgsl_sema value = rc_wgsl_check_sema(c, n.rhs);

    if (rc_wgsl_check_failed(c)) {
        return;
    }
    if (is_const && !(value.flags & RC_WGSL_SEMA_CONST)) {
        rc_wgsl_check_fail(c, n.rhs, RC_STR("a const needs a value known before the shader runs"));
        return;
    }

    // A const keeps an abstract type, to take whatever type each use wants
    if (declared != 0) {
        rc_wgsl_check_convert(c, n.rhs, declared, arena);
        type = declared;
    }
    else if (!is_const) {
        rc_wgsl_check_concretise_default(c, n.rhs, arena);
        type = rc_wgsl_check_sema(c, n.rhs).type;
    }
    if (!rc_wgsl_type_is_constructible(c->types.view, type)
        && (is_const || rc_wgsl_check_typeinfo_type(c, type) != RC_WGSL_TYPEINFO_TYPE_POINTER)) {
        rc_wgsl_check_fail_type(c, n.rhs, RC_STR("can't keep "), type,
            is_const ? RC_STR(" in a const") : RC_STR(" in a let"), arena);
        return;
    }

    rc_wgsl_check_set(c, node, (rc_wgsl_sema) {
        .value = value.value,
        .type = type,
        .flags = (uint8_t)(value.flags & (RC_WGSL_SEMA_CONST | RC_WGSL_SEMA_VALUE))
    });
}

// ---- module-scope vars ----

// A var's address space and access mode.
typedef struct rc_wgsl_space_access {
    rc_wgsl_space space;
    rc_wgsl_access access;
} rc_wgsl_space_access;

// The address space and access mode a var's template list names, either of which may be none.
static rc_wgsl_space_access rc_wgsl_check_var_templates(rc_wgsl_checker *c, uint32_t templates)
{
    rc_view_wgsl_index args = rc_wgsl_ast_list(c->ast, templates);
    rc_wgsl_space space = RC_WGSL_SPACE_NONE;
    rc_wgsl_access access = RC_WGSL_ACCESS_NONE;

    if (args.num > 2) {
        rc_wgsl_check_fail(c, rc_view_wgsl_index_get(args, 2),
            RC_STR("a var takes an address space and an access mode, and no more"));
    }
    if (args.num > 0) {
        space = (rc_wgsl_space)rc_wgsl_check_enumerant(c, rc_view_wgsl_index_get(args, 0),
            RC_WGSL_ENUMERANT_SPACE);
    }
    if (args.num > 1) {
        access = (rc_wgsl_access)rc_wgsl_check_enumerant(c, rc_view_wgsl_index_get(args, 1),
            RC_WGSL_ENUMERANT_ACCESS);
        if (space != RC_WGSL_SPACE_STORAGE && !rc_wgsl_check_failed(c)) {
            rc_wgsl_check_fail(c, rc_view_wgsl_index_get(args, 1),
                RC_STR("only storage vars take an access mode"));
        }
    }

    return (rc_wgsl_space_access) {
        .space = space,
        .access = access
    };
}


// True if a value of type can live in a buffer shared with the host: no bools, pointers or handles.
static bool rc_wgsl_is_host_shareable(const rc_wgsl_checker *c, uint32_t type)
{
    rc_wgsl_typeinfo t = rc_wgsl_check_typeinfo(c, type);

    switch ((rc_wgsl_typeinfo_type)t.type) {
    case RC_WGSL_TYPEINFO_TYPE_I32:
    case RC_WGSL_TYPEINFO_TYPE_U32:
    case RC_WGSL_TYPEINFO_TYPE_F32:
        return true;
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
    case RC_WGSL_TYPEINFO_TYPE_ARRAY:
    case RC_WGSL_TYPEINFO_TYPE_ATOMIC:
        return rc_wgsl_is_host_shareable(c, t.element);
    case RC_WGSL_TYPEINFO_TYPE_STRUCT: {
        rc_view_wgsl_index fields = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, t.decl).lhs);

        for (uint32_t i = 0; i < fields.num; i++) {
            if (!rc_wgsl_is_host_shareable(c,
                rc_wgsl_check_sema(c, rc_view_wgsl_index_get(fields, i)).type)) {
                return false;
            }
        }
        return true;
    }
    default:
        return false;
    }
}


// True for a sampler or a texture: what lives in the handle address space.
static bool rc_wgsl_is_handle(const rc_wgsl_checker *c, uint32_t id)
{
    rc_wgsl_typeinfo_type type = rc_wgsl_check_typeinfo_type(c, id);

    return type == RC_WGSL_TYPEINFO_TYPE_SAMPLER || type == RC_WGSL_TYPEINFO_TYPE_SAMPLER_COMPARISON
        || type == RC_WGSL_TYPEINFO_TYPE_TEXTURE || type == RC_WGSL_TYPEINFO_TYPE_STORAGE_TEXTURE;
}


// Check that a module-scope var of type in space has what that space needs, failing at node.
static void rc_wgsl_check_space_rules(rc_wgsl_checker *c, uint32_t node, rc_wgsl_var_parts parts,
    rc_wgsl_space space, uint32_t type, rc_arena *arena)
{
    bool bound = rc_wgsl_attribute_find(c->ast, parts.attributes, RC_STR("group")) != 0
        && rc_wgsl_attribute_find(c->ast, parts.attributes, RC_STR("binding")) != 0;
    bool needs_binding = space == RC_WGSL_SPACE_UNIFORM || space == RC_WGSL_SPACE_STORAGE
        || space == RC_WGSL_SPACE_HANDLE;

    if (needs_binding && !bound) {
        rc_wgsl_check_fail(c, node, RC_STR("this var needs @group and @binding"));
    }
    else if (!needs_binding && parts.attributes != 0) {
        rc_wgsl_check_fail(c, node, RC_STR("only uniform, storage and handle vars take attributes"));
    }
    else if (needs_binding && parts.initialiser != 0) {
        rc_wgsl_check_fail(c, parts.initialiser, RC_STR("this var can't have a value"));
    }
    else if ((space == RC_WGSL_SPACE_UNIFORM || space == RC_WGSL_SPACE_STORAGE)
        && !rc_wgsl_is_host_shareable(c, type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("a buffer can't hold "), type, RC_STR(""), arena);
    }
    else if (space == RC_WGSL_SPACE_UNIFORM && !rc_wgsl_type_is_constructible(c->types.view, type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("a uniform buffer can't hold "), type, RC_STR(""),
            arena);
    }
    else if (space == RC_WGSL_SPACE_PRIVATE && !rc_wgsl_type_is_constructible(c->types.view, type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("a private var can't hold "), type, RC_STR(""),
            arena);
    }
}


// The address space a module-scope var lives in: the one it names, or handle for samplers and
// textures, which name none.
static rc_wgsl_space rc_wgsl_check_global_space(rc_wgsl_checker *c, uint32_t node,
    rc_wgsl_space named, uint32_t type)
{
    if (named == RC_WGSL_SPACE_NONE && rc_wgsl_is_handle(c, type)) {
        return RC_WGSL_SPACE_HANDLE;
    }
    if (named == RC_WGSL_SPACE_NONE) {
        rc_wgsl_check_fail(c, node, RC_STR("this var needs an address space, such as var<private>"));
    }
    else if (rc_wgsl_is_handle(c, type)) {
        rc_wgsl_check_fail(c, node, RC_STR("samplers and textures take no address space"));
    }
    else if (named == RC_WGSL_SPACE_FUNCTION) {
        rc_wgsl_check_fail(c, node, RC_STR("function vars only go in functions"));
    }
    else if (named == RC_WGSL_SPACE_WORKGROUP) {
        rc_wgsl_check_fail(c, node, RC_STR("workgroup vars aren't supported yet"));
    }
    return named;
}


// The type of a var from its declared type or else its initialiser, which must agree.
static uint32_t rc_wgsl_check_var_type(rc_wgsl_checker *c, uint32_t node, rc_wgsl_var_parts parts,
    rc_arena *arena)
{
    uint32_t type = parts.type != 0 ? rc_wgsl_check_type_expr(c, parts.type, arena) : 0;

    if (parts.initialiser == 0) {
        if (type == 0) {
            rc_wgsl_check_fail(c, node, RC_STR("this var needs a type or a value"));
        }
        return type;
    }
    rc_wgsl_check_value(c, parts.initialiser, arena);
    if (type != 0) {
        rc_wgsl_check_convert(c, parts.initialiser, type, arena);
        return type;
    }
    rc_wgsl_check_concretise_default(c, parts.initialiser, arena);
    return rc_wgsl_check_sema(c, parts.initialiser).type;
}


// Check a module-scope var.
static void rc_wgsl_check_global_var(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_var_parts parts = rc_wgsl_ast_var_parts(c->ast, node);
    rc_wgsl_space_access named = rc_wgsl_check_var_templates(c, parts.templates);
    uint32_t type = rc_wgsl_check_var_type(c, node, parts, arena);
    rc_wgsl_space space;
    rc_wgsl_access access = named.access;

    rc_wgsl_check_attributes(c, parts.attributes, RC_STR("group binding"), arena);
    if (rc_wgsl_check_failed(c)) {
        return;
    }
    space = rc_wgsl_check_global_space(c, node, named.space, type);
    rc_wgsl_check_space_rules(c, node, parts, space, type, arena);
    if (parts.initialiser != 0
        && !(rc_wgsl_check_sema(c, parts.initialiser).flags & RC_WGSL_SEMA_CONST)) {
        rc_wgsl_check_fail(c, parts.initialiser,
            RC_STR("a module-scope var needs a value known before the shader runs"));
    }

    // Storage defaults to read; private memory and handles are as their space makes them
    if (access == RC_WGSL_ACCESS_NONE) {
        access = space == RC_WGSL_SPACE_PRIVATE ? RC_WGSL_ACCESS_READ_WRITE : RC_WGSL_ACCESS_READ;
    }
    rc_wgsl_check_set(c, node, (rc_wgsl_sema) {
        .type = type,
        .space = (uint8_t)space,
        .access = (uint8_t)access
    });
}

// ---- functions ----

// The stage an entry point's attributes name, or none for an ordinary function.
static rc_wgsl_stage rc_wgsl_check_stage(rc_wgsl_checker *c, uint32_t node, uint32_t attributes)
{
    bool vertex = rc_wgsl_attribute_find(c->ast, attributes, RC_STR("vertex")) != 0;
    bool fragment = rc_wgsl_attribute_find(c->ast, attributes, RC_STR("fragment")) != 0;

    if (rc_wgsl_attribute_find(c->ast, attributes, RC_STR("compute")) != 0) {
        rc_wgsl_check_fail(c, node, RC_STR("compute shaders aren't supported yet"));
    }
    else if (vertex && fragment) {
        rc_wgsl_check_fail(c, node, RC_STR("an entry point has one stage"));
    }
    return vertex ? RC_WGSL_STAGE_VERTEX : fragment ? RC_WGSL_STAGE_FRAGMENT : RC_WGSL_STAGE_NONE;
}


// Check a parameter, which only an entry point may give attributes.
static void rc_wgsl_check_param(rc_wgsl_checker *c, uint32_t param, bool entry, rc_arena *arena)
{
    rc_wgsl_node p = rc_wgsl_ast_node(c->ast, param);
    uint32_t type = rc_wgsl_check_type_expr(c, p.rhs, arena);
    rc_wgsl_typeinfo t = rc_wgsl_check_typeinfo(c, type);

    rc_wgsl_check_attributes(c, p.lhs,
        entry ? RC_STR("location builtin interpolate invariant") : RC_STR(""), arena);
    if (rc_wgsl_check_failed(c)) {
        return;
    }
    if (rc_wgsl_is_handle(c, type)) {
        rc_wgsl_check_fail(c, p.rhs,
            RC_STR("passing samplers and textures to functions isn't supported yet"));
    }
    else if (t.type == RC_WGSL_TYPEINFO_TYPE_POINTER && entry) {
        rc_wgsl_check_fail(c, p.rhs, RC_STR("an entry point can't take a pointer"));
    }
    else if (t.type != RC_WGSL_TYPEINFO_TYPE_POINTER
        && !rc_wgsl_type_is_constructible(c->types.view, type)) {
        rc_wgsl_check_fail_type(c, p.rhs, RC_STR("a function can't take "), type, RC_STR(""), arena);
    }
    rc_wgsl_check_set_type(c, param, type);
}


// Check a function's parameters, which need different names.
static void rc_wgsl_check_params(rc_wgsl_checker *c, uint32_t params, bool entry, rc_arena *arena)
{
    rc_view_wgsl_index list = rc_wgsl_ast_list(c->ast, params);

    for (uint32_t i = 0; i < list.num && !rc_wgsl_check_failed(c); i++) {
        rc_wgsl_check_param(c, rc_view_wgsl_index_get(list, i), entry, arena);
        rc_wgsl_check_unique_name(c, list, i, RC_STR("' is already a parameter"), arena);
    }
}


// Check a function's attributes, parameters and return type, and remember it if it is an entry
// point.
static void rc_wgsl_check_signature(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_fn_parts parts = rc_wgsl_ast_fn_parts(c->ast, node);
    rc_wgsl_stage stage;
    uint32_t type = 0;

    rc_wgsl_check_attributes(c, parts.attributes,
        RC_STR("vertex fragment compute workgroup_size must_use"), arena);
    stage = rc_wgsl_check_stage(c, node, parts.attributes);
    rc_wgsl_check_params(c, parts.params, stage != RC_WGSL_STAGE_NONE, arena);
    if (parts.return_type != 0) {
        type = rc_wgsl_check_type_expr(c, parts.return_type, arena);
        rc_wgsl_check_attributes(c, parts.return_attributes,
            stage != RC_WGSL_STAGE_NONE ? RC_STR("location builtin interpolate invariant")
                                        : RC_STR(""), arena);
        if (!rc_wgsl_check_failed(c) && !rc_wgsl_type_is_constructible(c->types.view, type)) {
            rc_wgsl_check_fail_type(c, parts.return_type, RC_STR("a function can't return "), type,
                RC_STR(""), arena);
        }
    }
    if (rc_wgsl_check_failed(c)) {
        return;
    }
    rc_wgsl_check_set(c, node, (rc_wgsl_sema) {.type = type, .stage = (uint8_t)stage});
    if (stage != RC_WGSL_STAGE_NONE) {
        rc_array_wgsl_index_push(&c->entry_points, node, arena);
    }
}

// ---- const_assert ----

// Check a const_assert, which must hold.
static void rc_wgsl_check_const_assert(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    uint32_t condition = rc_wgsl_ast_node(c->ast, node).lhs;
    rc_wgsl_sema sema = rc_wgsl_check_expr(c, condition, arena);

    if (rc_wgsl_check_failed(c)) {
        return;
    }
    if (sema.type != RC_WGSL_TYPE_ID_BOOL) {
        rc_wgsl_check_fail_type(c, condition, RC_STR("expected bool, got "), sema.type, RC_STR(""),
            arena);
    }
    else if (!(sema.flags & RC_WGSL_SEMA_VALUE)) {
        rc_wgsl_check_fail(c, condition,
            RC_STR("this condition must be made of integers and bools we can work out"));
    }
    else if (sema.value == 0) {
        rc_wgsl_check_fail(c, node, RC_STR("this const_assert fails"));
    }
}


void rc_wgsl_check_declaration(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    switch (rc_wgsl_ast_type(c->ast, node)) {
    case RC_WGSL_NODE_TYPE_STRUCT:
        rc_wgsl_check_struct(c, node, arena);
        break;
    case RC_WGSL_NODE_TYPE_ALIAS:
        rc_wgsl_check_set_type(c, node,
            rc_wgsl_check_type_expr(c, rc_wgsl_ast_node(c->ast, node).lhs, arena));
        break;
    case RC_WGSL_NODE_TYPE_CONST:
        rc_wgsl_check_value_decl(c, node, arena);
        break;
    case RC_WGSL_NODE_TYPE_VAR:
        rc_wgsl_check_global_var(c, node, arena);
        break;
    case RC_WGSL_NODE_TYPE_FN:
        rc_wgsl_check_signature(c, node, arena);
        break;
    case RC_WGSL_NODE_TYPE_CONST_ASSERT:
        rc_wgsl_check_const_assert(c, node, arena);
        break;
    default:
        RC_UNREACHABLE();
    }
}
