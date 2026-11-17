// check.c
//
// The checker's driver: the module's names, declarations checked on demand, and the checks that
// need the whole module, such as calls never going round in a circle.

#include "checker.h"

#include "error.h"
#include "richc/hash.h"
#include "richc/mstr.h"

#define RC_HASH_INDEX_KEY_TYPE rc_str
#define RC_HASH_INDEX_DATA_TYPE rc_view_wgsl_name
#define RC_HASH_INDEX_KEY_AT(data, index) (rc_view_wgsl_name_get(data, index).text)
#define RC_HASH_INDEX_HASH(key) rc_hash_str(key)
#define RC_HASH_INDEX_EQUAL(a, b) rc_str_is_equal(a, b)
#define RC_HASH_INDEX_NAME wgsl_name
#include "richc/hash_index.template.h"

enum {
    // How deeply declarations may lean on each other before we give up checking them on demand
    RC_WGSL_MAX_CHECK_DEPTH = 64
};

// ---- errors ----

// True once the checker has failed.
bool rc_wgsl_check_failed(const rc_wgsl_checker *c)
{
    return rc_wgsl_error_is_set(c->error);
}


// Fail at node with message, unless the checker has failed already; we keep the first error.
rc_wgsl_sema rc_wgsl_check_fail(rc_wgsl_checker *c, uint32_t node, rc_str message)
{
    if (!rc_wgsl_check_failed(c)) {
        c->error = rc_wgsl_ast_error(c->ast, node, message);
    }

    return (rc_wgsl_sema) {0};
}


// Fail at node with a message naming type between before and after.
rc_wgsl_sema rc_wgsl_check_fail_type(rc_wgsl_checker *c, uint32_t node, rc_str before, uint32_t type, rc_str after, rc_arena *arena)
{
    rc_mstr message = rc_mstr_from_str(before, 64, arena);

    rc_wgsl_type_append_name(&message, c->types.infos.view, type, c->ast, arena);
    rc_mstr_append(&message, after, arena);
    return rc_wgsl_check_fail(c, node, message.view);
}


// Fail at node with a message naming the types a and b, after before and around between.
rc_wgsl_sema rc_wgsl_check_fail_types(rc_wgsl_checker *c, uint32_t node, rc_str before,
    uint32_t a, rc_str between, uint32_t b, rc_arena *arena)
{
    rc_mstr message = rc_mstr_from_str(before, 64, arena);

    rc_wgsl_type_append_name(&message, c->types.infos.view, a, c->ast, arena);
    rc_mstr_append(&message, between, arena);
    rc_wgsl_type_append_name(&message, c->types.infos.view, b, c->ast, arena);
    return rc_wgsl_check_fail(c, node, message.view);
}


// Fail at node with a message quoting its text between before and after.
rc_wgsl_sema rc_wgsl_check_fail_name(rc_wgsl_checker *c, uint32_t node, rc_str before, rc_str after, rc_arena *arena)
{
    rc_mstr message = rc_mstr_from_str(before, 64, arena);

    rc_mstr_append(&message, rc_wgsl_ast_text(c->ast, node), arena);
    rc_mstr_append(&message, after, arena);
    return rc_wgsl_check_fail(c, node, message.view);
}

// ---- what is known ----

// What the checker knows about node.
rc_wgsl_sema rc_wgsl_check_sema(const rc_wgsl_checker *c, uint32_t node)
{
    return rc_span_wgsl_sema_get(c->sema, node);
}


// Record what the checker knows about node.
void rc_wgsl_check_set(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema sema)
{
    rc_span_wgsl_sema_set(c->sema, node, sema);
}


// Record the type of node, keeping the rest of what is known about it.
void rc_wgsl_check_set_type(rc_wgsl_checker *c, uint32_t node, uint32_t type)
{
    rc_span_wgsl_sema_at(c->sema, node)->type = type;
}


// The module as checked so far.
rc_wgsl_module rc_wgsl_check_module(const rc_wgsl_checker *c)
{
    return (rc_wgsl_module) {
        .ast = c->ast,
        .types = c->types.infos.view,
        .sema = c->sema.view,
        .entries = c->entries.view
    };
}


// What we know of the type id.
rc_wgsl_typeinfo rc_wgsl_check_typeinfo(const rc_wgsl_checker *c, uint32_t id)
{
    return rc_array_wgsl_typeinfo_get(&c->types.infos, id);
}


// The sort of type that id is.
rc_wgsl_typeinfo_type rc_wgsl_check_typeinfo_type(const rc_wgsl_checker *c, uint32_t id)
{
    return (rc_wgsl_typeinfo_type)rc_wgsl_check_typeinfo(c, id).type;
}

// ---- names ----

// The innermost local called name, or 0 if there is none.
static uint32_t rc_wgsl_find_local(const rc_wgsl_checker *c, rc_str name)
{
    for (uint32_t i = c->locals.num; i-- > 0;) {
        rc_wgsl_name local = rc_array_wgsl_name_get(&c->locals, i);

        if (rc_str_is_equal(local.text, name)) {
            return local.node;
        }
    }
    return 0;
}


// What name names at module scope: one of the shader's own declarations, or a predeclared name.
static rc_wgsl_found rc_wgsl_lookup_global(const rc_wgsl_checker *c, rc_str name)
{
    uint32_t global = rc_hash_index_wgsl_name_find(c->global_index, c->globals.view, name);

    if (global != RC_INDEX_NONE) {
        return (rc_wgsl_found) {
            .node = rc_array_wgsl_name_get(&c->globals, global).node,
            .global = true
        };
    }

    return (rc_wgsl_found) {.predeclared = rc_wgsl_predeclared_find(name)};
}


// What name names where the checker is: a local, a module-scope declaration or a predeclared name.
rc_wgsl_found rc_wgsl_check_lookup(const rc_wgsl_checker *c, rc_str name)
{
    uint32_t local = rc_wgsl_find_local(c, name);

    if (local != 0) {
        return (rc_wgsl_found) {.node = local};
    }

    return rc_wgsl_lookup_global(c, name);
}


// Add the local declaration node to the names in scope, failing if the scope has it already.
void rc_wgsl_check_declare_local(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_str name = rc_wgsl_ast_text(c->ast, node);

    for (uint32_t i = c->scope; i < c->locals.num; i++) {
        if (rc_str_is_equal(rc_array_wgsl_name_get(&c->locals, i).text, name)) {
            rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is already declared here"), arena);
            return;
        }
    }
    rc_array_wgsl_name_push(&c->locals, (rc_wgsl_name) {.text = name, .node = node}, arena);
}


// Note node as the first thing the function being checked does that only a fragment shader may.
void rc_wgsl_check_note_fragment_only(rc_wgsl_checker *c, uint32_t node)
{
    if (c->function != 0 && rc_wgsl_check_sema(c, c->function).fragment_only_at == 0) {
        rc_span_wgsl_sema_at(c->sema, c->function)->fragment_only_at = node;
    }
}


// Add the named module-scope declaration node to the module's names, failing on a repeat.
static void rc_wgsl_check_declare_global(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_name name = {.text = rc_wgsl_ast_text(c->ast, node), .node = node};
    uint32_t found = rc_hash_index_wgsl_name_find_or_add(&c->global_index, c->globals.view, name.text, c->globals.num, arena);

    if (found != c->globals.num) {
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is already declared"), arena);
    }
    else {
        rc_array_wgsl_name_push(&c->globals, name, arena);
    }
}


// Add every named module-scope declaration to the module's names, failing on a repeat.
static void rc_wgsl_check_declare_globals(rc_wgsl_checker *c, rc_arena *arena)
{
    rc_view_wgsl_index decls = rc_wgsl_ast_list(c->ast, c->ast.decls);

    for (uint32_t i = 0; i < decls.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t node = rc_view_wgsl_index_get(decls, i);

        if (rc_wgsl_ast_type(c->ast, node) != RC_WGSL_NODE_TYPE_CONST_ASSERT) {
            rc_wgsl_check_declare_global(c, node, arena);
        }
    }
}

// ---- declarations ----

// Check the module-scope declaration decl, used at node by whatever is being checked, unless it is
// checked already; and note the use.
void rc_wgsl_check_global(rc_wgsl_checker *c, uint32_t decl, uint32_t node, rc_arena *arena)
{
    rc_wgsl_decl_state state = (rc_wgsl_decl_state)rc_span_wgsl_state_get(c->states, decl);
    uint32_t user = c->user;
    uint32_t function = c->function;
    uint32_t locals = c->locals.num;
    uint32_t scope = c->scope;

    if (user != 0) {
        rc_wgsl_reference reference = {.user = user, .used = decl, .node = node};

        rc_array_wgsl_reference_push(&c->references, reference, arena);
    }

    if (state == RC_WGSL_DECL_STATE_CHECKED) {
        return;
    }

    if (state == RC_WGSL_DECL_STATE_CHECKING) {
        rc_wgsl_check_fail_name(c, decl, RC_STR("'"), RC_STR("' depends on itself"), arena);
        return;
    }

    if (c->depth >= RC_WGSL_MAX_CHECK_DEPTH) {
        rc_wgsl_check_fail(c, decl, RC_STR("declarations depend on each other too deeply"));
        return;
    }

    // A module-scope declaration sees only module-scope names, wherever it was first needed
    rc_span_wgsl_state_set(c->states, decl, RC_WGSL_DECL_STATE_CHECKING);
    c->depth++;
    c->user = decl;
    c->function = 0;
    c->locals.num = 0;
    c->scope = 0;
    rc_wgsl_check_declaration(c, decl, arena);
    c->scope = scope;
    c->locals.num = locals;
    c->function = function;
    c->user = user;
    c->depth--;
    rc_span_wgsl_state_set(c->states, decl, RC_WGSL_DECL_STATE_CHECKED);
}

// ---- the whole module ----

// Check the module ast, giving what we know of it, or the first error.
rc_wgsl_check_result rc_wgsl_check(rc_wgsl_ast ast, rc_arena *arena)
{
    rc_wgsl_checker c = {
        .ast = ast,
        .types = rc_wgsl_types_make(arena),
        .sema = rc_span_wgsl_sema_make_zero(ast.nodes.num, arena),
        .states = rc_span_wgsl_state_make_zero(ast.nodes.num, arena),
        .globals = rc_array_wgsl_name_make(64, arena),
        .global_index = rc_hash_index_make(64, arena),
        .locals = rc_array_wgsl_name_make(64, arena),
        .references = rc_array_wgsl_reference_make(64, arena),
        .texture_uses = rc_array_wgsl_texture_use_make(8, arena),
        .pointer_calls = rc_array_wgsl_index_make(8, arena),
        .entry_points = rc_array_wgsl_index_make(4, arena),
        .entries = rc_array_wgsl_entry_make(4, arena)
    };
    rc_view_wgsl_index decls = rc_wgsl_ast_list(ast, ast.decls);

    // Every declaration's type first, then the function bodies, then the whole module's rules,
    // then the entry points
    rc_wgsl_check_declare_globals(&c, arena);
    for (uint32_t i = 0; i < decls.num && !rc_wgsl_check_failed(&c); i++) {
        rc_wgsl_check_global(&c, rc_view_wgsl_index_get(decls, i), 0, arena);
    }
    for (uint32_t i = 0; i < decls.num && !rc_wgsl_check_failed(&c); i++) {
        uint32_t node = rc_view_wgsl_index_get(decls, i);

        if (rc_wgsl_ast_type(ast, node) == RC_WGSL_NODE_TYPE_FN) {
            rc_wgsl_check_body(&c, node, arena);
        }
    }
    rc_wgsl_check_graph(&c, arena);
    rc_wgsl_check_recursion(&c, arena);
    rc_wgsl_check_aliasing(&c, arena);
    for (uint32_t i = 0; i < c.entry_points.num && !rc_wgsl_check_failed(&c); i++) {
        rc_wgsl_entry entry = rc_wgsl_check_entry_point(&c, rc_array_wgsl_index_get(&c.entry_points, i), arena);

        rc_array_wgsl_entry_push(&c.entries, entry, arena);
    }
    if (rc_wgsl_check_failed(&c)) {
        return (rc_wgsl_check_result) {.error = c.error};
    }

    return (rc_wgsl_check_result) {.module = rc_wgsl_check_module(&c)};
}
