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

bool rc_wgsl_check_failed(const rc_wgsl_checker *c)
{
    return rc_wgsl_error_is_set(c->error);
}


void rc_wgsl_check_fail(rc_wgsl_checker *c, uint32_t node, rc_str message)
{
    uint32_t token = rc_wgsl_ast_node(c->ast, node).token;

    if (!rc_wgsl_check_failed(c)) {
        uint32_t offset = rc_view_wgsl_token_get(c->ast.tokens, token).offset;

        c->error = rc_wgsl_error_at(c->ast.source, offset, message);
    }
}


void rc_wgsl_check_fail_type(rc_wgsl_checker *c, uint32_t node, rc_str before, uint32_t type,
    rc_str after, rc_arena *arena)
{
    rc_mstr message = rc_mstr_from_str(before, 64, arena);

    rc_wgsl_type_append_name(&message, c->types.view, type, c->ast, arena);
    rc_mstr_append(&message, after, arena);
    rc_wgsl_check_fail(c, node, message.view);
}


void rc_wgsl_check_fail_types(rc_wgsl_checker *c, uint32_t node, rc_str before, uint32_t a,
    rc_str between, uint32_t b, rc_arena *arena)
{
    rc_mstr message = rc_mstr_from_str(before, 64, arena);

    rc_wgsl_type_append_name(&message, c->types.view, a, c->ast, arena);
    rc_mstr_append(&message, between, arena);
    rc_wgsl_type_append_name(&message, c->types.view, b, c->ast, arena);
    rc_wgsl_check_fail(c, node, message.view);
}


void rc_wgsl_check_fail_name(rc_wgsl_checker *c, uint32_t node, rc_str before, rc_str after,
    rc_arena *arena)
{
    rc_mstr message = rc_mstr_from_str(before, 64, arena);

    rc_mstr_append(&message, rc_wgsl_ast_text(c->ast, node), arena);
    rc_mstr_append(&message, after, arena);
    rc_wgsl_check_fail(c, node, message.view);
}

// ---- what is known ----

rc_wgsl_sema rc_wgsl_check_sema(const rc_wgsl_checker *c, uint32_t node)
{
    return rc_span_wgsl_sema_get(c->sema, node);
}


void rc_wgsl_check_set(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema sema)
{
    rc_span_wgsl_sema_set(c->sema, node, sema);
}


void rc_wgsl_check_set_type(rc_wgsl_checker *c, uint32_t node, uint32_t type)
{
    rc_span_wgsl_sema_at(c->sema, node)->type = type;
}


rc_wgsl_typeinfo rc_wgsl_check_typeinfo(const rc_wgsl_checker *c, uint32_t id)
{
    return rc_array_wgsl_typeinfo_get(&c->types, id);
}


rc_wgsl_typeinfo_type rc_wgsl_check_typeinfo_type(const rc_wgsl_checker *c, uint32_t id)
{
    return (rc_wgsl_typeinfo_type)rc_wgsl_check_typeinfo(c, id).type;
}

// ---- names ----

rc_wgsl_found rc_wgsl_check_lookup(const rc_wgsl_checker *c, rc_str name)
{
    uint32_t global;

    for (uint32_t i = c->locals.num; i-- > 0;) {
        rc_wgsl_name local = rc_array_wgsl_name_get(&c->locals, i);

        if (rc_str_is_equal(local.text, name)) {
            return (rc_wgsl_found) {.node = local.node};
        }
    }
    global = rc_hash_index_wgsl_name_find(c->global_index, c->globals.view, name);
    if (global != RC_INDEX_NONE) {
        return (rc_wgsl_found) {
            .node = rc_array_wgsl_name_get(&c->globals, global).node,
            .global = true
        };
    }
    return (rc_wgsl_found) {.predeclared = rc_wgsl_predeclared_find(name)};
}


void rc_wgsl_check_declare_local(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_str name = rc_wgsl_ast_text(c->ast, node);

    for (uint32_t i = c->scope; i < c->locals.num; i++) {
        if (rc_str_is_equal(rc_array_wgsl_name_get(&c->locals, i).text, name)) {
            rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is already declared here"),
                arena);
            return;
        }
    }
    rc_array_wgsl_name_push(&c->locals, (rc_wgsl_name) {.text = name, .node = node}, arena);
}


void rc_wgsl_check_note_fragment_only(rc_wgsl_checker *c, uint32_t node)
{
    if (c->function != 0 && rc_wgsl_check_sema(c, c->function).fragment_only_at == 0) {
        rc_span_wgsl_sema_at(c->sema, c->function)->fragment_only_at = node;
    }
}


// Add every named module-scope declaration to the module's names, failing on a repeat.
static void rc_wgsl_check_declare_globals(rc_wgsl_checker *c, rc_arena *arena)
{
    rc_view_wgsl_index decls = rc_wgsl_ast_list(c->ast, c->ast.decls);

    for (uint32_t i = 0; i < decls.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t node = rc_view_wgsl_index_get(decls, i);
        rc_wgsl_name name = {.text = rc_wgsl_ast_text(c->ast, node), .node = node};
        uint32_t found;

        if (rc_wgsl_ast_type(c->ast, node) == RC_WGSL_NODE_TYPE_CONST_ASSERT) {
            continue;
        }
        found = rc_hash_index_wgsl_name_find_or_add(&c->global_index, c->globals.view, name.text,
            c->globals.num, arena);
        if (found != c->globals.num) {
            rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' is already declared"), arena);
        }
        else {
            rc_array_wgsl_name_push(&c->globals, name, arena);
        }
    }
}

// ---- declarations ----

void rc_wgsl_check_global(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_sema *sema = rc_span_wgsl_sema_at(c->sema, node);
    uint32_t function = c->function;
    uint32_t locals = c->locals.num;
    uint32_t scope = c->scope;

    if (sema->flags & RC_WGSL_SEMA_CHECKED) {
        return;
    }
    if (sema->flags & RC_WGSL_SEMA_CHECKING) {
        rc_wgsl_check_fail_name(c, node, RC_STR("'"), RC_STR("' depends on itself"), arena);
        return;
    }
    if (c->depth >= RC_WGSL_MAX_CHECK_DEPTH) {
        rc_wgsl_check_fail(c, node, RC_STR("declarations depend on each other too deeply"));
        return;
    }

    // A module-scope declaration sees only module-scope names, wherever it was first needed
    sema->flags |= RC_WGSL_SEMA_CHECKING;
    c->depth++;
    c->function = 0;
    c->locals.num = 0;
    c->scope = 0;
    rc_wgsl_check_declaration(c, node, arena);
    c->scope = scope;
    c->locals.num = locals;
    c->function = function;
    c->depth--;
    sema = rc_span_wgsl_sema_at(c->sema, node);
    sema->flags = (uint8_t)((sema->flags & ~RC_WGSL_SEMA_CHECKING) | RC_WGSL_SEMA_CHECKED);
}

// ---- the whole module ----

// Where the walk through the calls has been with a function: nowhere yet, on the current path, or
// finished with it.
enum {
    RC_WGSL_WALK_NONE = 0,
    RC_WGSL_WALK_ON_PATH,
    RC_WGSL_WALK_DONE
};

#define RC_ARRAY_TYPE uint8_t
#define RC_ARRAY_NAME wgsl_walk
#include "richc/array.template.h"

// The index of the first call fn makes at or after edge, or the number of calls if none.
static uint32_t rc_wgsl_next_call(const rc_wgsl_checker *c, uint32_t fn, uint32_t edge)
{
    while (edge < c->calls.num && rc_array_wgsl_edge_get(&c->calls, edge).caller != fn) {
        edge++;
    }
    return edge;
}


// Walk the calls from start depth first, failing at a call back onto the current path. The path
// is kept in arrays rather than on the call stack: fns holds each function on it, and cursors the
// next of its calls to follow.
static void rc_wgsl_walk_calls(rc_wgsl_checker *c, uint32_t start, rc_span_wgsl_walk state,
    rc_arena *arena)
{
    rc_array_wgsl_index fns = rc_array_wgsl_index_make(16, arena);
    rc_array_wgsl_index cursors = rc_array_wgsl_index_make(16, arena);

    rc_span_wgsl_walk_set(state, start, RC_WGSL_WALK_ON_PATH);
    rc_array_wgsl_index_push(&fns, start, arena);
    rc_array_wgsl_index_push(&cursors, 0, arena);
    while (fns.num > 0 && !rc_wgsl_check_failed(c)) {
        uint32_t fn = *rc_array_wgsl_index_last_at(&fns);
        uint32_t next = rc_wgsl_next_call(c, fn, *rc_array_wgsl_index_last_at(&cursors));
        rc_wgsl_edge edge;

        if (next == c->calls.num) {
            rc_span_wgsl_walk_set(state, fn, RC_WGSL_WALK_DONE);
            rc_array_wgsl_index_pop(&fns);
            rc_array_wgsl_index_pop(&cursors);
            continue;
        }
        *rc_array_wgsl_index_last_at(&cursors) = next + 1;
        edge = rc_array_wgsl_edge_get(&c->calls, next);
        if (rc_span_wgsl_walk_get(state, edge.callee) == RC_WGSL_WALK_ON_PATH) {
            rc_wgsl_check_fail(c, edge.call,
                RC_STR("functions can't call themselves, even through others"));
        }
        else if (rc_span_wgsl_walk_get(state, edge.callee) == RC_WGSL_WALK_NONE) {
            rc_span_wgsl_walk_set(state, edge.callee, RC_WGSL_WALK_ON_PATH);
            rc_array_wgsl_index_push(&fns, edge.callee, arena);
            rc_array_wgsl_index_push(&cursors, 0, arena);
        }
    }
}


// Fail if any function calls itself, directly or through others: WGSL has no recursion.
static void rc_wgsl_check_recursion(rc_wgsl_checker *c, rc_arena *arena)
{
    rc_view_wgsl_index decls = rc_wgsl_ast_list(c->ast, c->ast.decls);
    rc_span_wgsl_walk state = rc_span_wgsl_walk_make_zero(c->ast.nodes.num, arena);

    for (uint32_t i = 0; i < decls.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t node = rc_view_wgsl_index_get(decls, i);

        if (rc_wgsl_ast_type(c->ast, node) == RC_WGSL_NODE_TYPE_FN
            && rc_span_wgsl_walk_get(state, node) == RC_WGSL_WALK_NONE) {
            rc_wgsl_walk_calls(c, node, state, arena);
        }
    }
}


rc_wgsl_check_result rc_wgsl_check(rc_wgsl_ast ast, rc_arena *arena)
{
    rc_wgsl_checker c = {
        .ast = ast,
        .types = rc_wgsl_types_make(arena),
        .sema = rc_span_wgsl_sema_make_zero(ast.nodes.num, arena),
        .globals = rc_array_wgsl_name_make(64, arena),
        .global_index = rc_hash_index_make(64, arena),
        .locals = rc_array_wgsl_name_make(64, arena),
        .calls = rc_array_wgsl_edge_make(32, arena),
        .entry_points = rc_array_wgsl_index_make(4, arena)
    };
    rc_view_wgsl_index decls = rc_wgsl_ast_list(ast, ast.decls);

    // Every declaration's type first, then the function bodies, then the entry points
    rc_wgsl_check_declare_globals(&c, arena);
    for (uint32_t i = 0; i < decls.num && !rc_wgsl_check_failed(&c); i++) {
        rc_wgsl_check_global(&c, rc_view_wgsl_index_get(decls, i), arena);
    }
    for (uint32_t i = 0; i < decls.num && !rc_wgsl_check_failed(&c); i++) {
        uint32_t node = rc_view_wgsl_index_get(decls, i);

        if (rc_wgsl_ast_type(ast, node) == RC_WGSL_NODE_TYPE_FN) {
            rc_wgsl_check_body(&c, node, arena);
        }
    }
    rc_wgsl_check_recursion(&c, arena);
    for (uint32_t i = 0; i < c.entry_points.num && !rc_wgsl_check_failed(&c); i++) {
        rc_wgsl_check_entry_point(&c, rc_array_wgsl_index_get(&c.entry_points, i), arena);
    }
    if (rc_wgsl_check_failed(&c)) {
        return (rc_wgsl_check_result) {.error = c.error};
    }

    return (rc_wgsl_check_result) {
        .module = {
            .ast = ast,
            .types = c.types.view,
            .sema = c.sema.view,
            .entry_points = c.entry_points.view
        }
    };
}
