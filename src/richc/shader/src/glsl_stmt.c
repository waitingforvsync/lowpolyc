// glsl_stmt.c
//
// Functions and statements in GLSL.
//
// Most statements map straight across. The exceptions: pointer lets and abstract consts write
// nothing, being written out where they are used; a compound assignment is written as a plain one,
// so its operator can take a polyfill; and WGSL's loop with a continuing block becomes a
// while (true), with the continuing block at the end, or at the top behind a flag when the body
// uses continue, which in GLSL would skip it.

#include "glsl_writer.h"

static void rc_glsl_write_statement(rc_writer *w, uint32_t node, rc_arena *arena);

// ---- blocks ----

// Write each statement of list on its own line.
static void rc_glsl_write_statements(rc_writer *w, uint32_t list, rc_arena *arena)
{
    rc_view_wgsl_index statements = rc_wgsl_ast_list(w->module.ast, list);

    for (uint32_t i = 0; i < statements.num && !rc_writer_failed(w); i++) {
        rc_glsl_write_statement(w, rc_view_wgsl_index_get(statements, i), arena);
    }
}


// Write " {", the statements of list indented, and "}" on a line of its own.
static void rc_glsl_write_braced(rc_writer *w, uint32_t list, rc_arena *arena)
{
    rc_writer_put(w, RC_STR(" {"), arena);
    w->indent++;
    rc_glsl_write_statements(w, list, arena);
    w->indent--;
    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("}"), arena);
}

// ---- declarations ----

// True if node is a let of pointer type, which is written out at each use.
static bool rc_glsl_is_pointer_let(const rc_writer *w, uint32_t node)
{
    if (rc_wgsl_ast_type(w->module.ast, node) != RC_WGSL_NODE_TYPE_LET) {
        return false;
    }

    return rc_writer_typeinfo(w, rc_writer_sema(w, node).type).type == RC_WGSL_TYPEINFO_TYPE_POINTER;
}


// True if node is a const of abstract type, which is written out at each use.
static bool rc_glsl_is_abstract_const(const rc_writer *w, uint32_t node)
{
    if (rc_wgsl_ast_type(w->module.ast, node) != RC_WGSL_NODE_TYPE_CONST) {
        return false;
    }

    return rc_wgsl_type_is_abstract(w->types.infos.view, rc_writer_sema(w, node).type);
}


// True if the local declaration node writes any code.
static bool rc_glsl_local_writes_code(const rc_writer *w, uint32_t node)
{
    return !rc_glsl_is_pointer_let(w, node) && !rc_glsl_is_abstract_const(w, node);
}


// Write a var, let or const in a function, without its ';'. A const is a plain variable: WGSL
// keeps it from changing, and GLSL's const won't take a polyfill's call.
static void rc_glsl_write_local(rc_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    uint32_t type = rc_writer_sema(w, node).type;
    uint32_t initialiser = n.type == RC_WGSL_NODE_TYPE_VAR ? rc_wgsl_ast_var_parts(w->module.ast, node).initialiser : n.rhs;

    rc_glsl_write_declaration(w, &w->code, type, rc_wgsl_ast_text(w->module.ast, node), node, arena);
    rc_writer_put(w, RC_STR(" = "), arena);
    if (initialiser != 0) {
        rc_glsl_write_expr(w, initialiser, 0, arena);
    }
    else {
        rc_glsl_write_zero(w, &w->code, type, node, arena);
    }
}

// ---- updates ----

// True if the update statement node writes any code: _ = e only does when e calls something.
static bool rc_glsl_update_writes_code(const rc_writer *w, uint32_t node)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);

    if (n.type != RC_WGSL_NODE_TYPE_ASSIGN) {
        return true;
    }

    if (rc_wgsl_ast_type(w->module.ast, n.lhs) != RC_WGSL_NODE_TYPE_PHONY) {
        return true;
    }

    return rc_writer_has_calls(w, n.rhs);
}


// Write an assignment without its ';'. A compound one is written out in full, target = target op
// value, which needs a target that is the same memory each time it is evaluated.
static void rc_glsl_write_assign(rc_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    rc_wgsl_token_type op = (rc_wgsl_token_type)rc_view_wgsl_token_get(w->module.ast.tokens, n.token).type;

    if (rc_wgsl_ast_type(w->module.ast, n.lhs) == RC_WGSL_NODE_TYPE_PHONY) {
        rc_glsl_write_expr(w, n.rhs, 0, arena);
        return;
    }

    rc_glsl_write_expr(w, n.lhs, 0, arena);
    rc_writer_put(w, RC_STR(" = "), arena);
    if (op == RC_WGSL_TOKEN_TYPE_EQUAL) {
        rc_glsl_write_expr(w, n.rhs, 0, arena);
        return;
    }

    if (rc_writer_has_calls(w, n.lhs)) {
        rc_writer_fail(w, n.lhs, RC_STR("this target calls a function, so it can't take a compound assignment"));
        return;
    }

    rc_glsl_write_operation(w, rc_wgsl_compound_operator(op), n.lhs, n.rhs, rc_writer_sema(w, n.lhs).type, 0, arena);
}


// Write an assignment, step or call, without its ';'.
static void rc_glsl_write_update(rc_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_ASSIGN:
        rc_glsl_write_assign(w, node, arena);
        break;
    case RC_WGSL_NODE_TYPE_INCREMENT:
    case RC_WGSL_NODE_TYPE_DECREMENT:
        rc_glsl_write_expr(w, n.lhs, 0, arena);
        rc_writer_put(w, n.type == RC_WGSL_NODE_TYPE_INCREMENT ? RC_STR("++") : RC_STR("--"), arena);
        break;
    default:
        rc_glsl_write_expr(w, node, 0, arena);
        break;
    }
}

// ---- control flow ----

// Write an if, its else-ifs and its else.
static void rc_glsl_write_if(rc_writer *w, uint32_t node, rc_arena *arena)
{
    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("if ("), arena);
    while (!rc_writer_failed(w)) {
        rc_wgsl_if_parts parts = rc_wgsl_ast_if_parts(w->module.ast, node);

        rc_glsl_write_expr(w, rc_wgsl_ast_node(w->module.ast, node).lhs, 0, arena);
        rc_writer_put(w, RC_STR(")"), arena);
        rc_glsl_write_braced(w, rc_wgsl_ast_node(w->module.ast, parts.then).lhs, arena);
        if (parts.other == 0) {
            break;
        }

        if (rc_wgsl_ast_type(w->module.ast, parts.other) != RC_WGSL_NODE_TYPE_IF) {
            rc_writer_put(w, RC_STR(" else"), arena);
            rc_glsl_write_braced(w, rc_wgsl_ast_node(w->module.ast, parts.other).lhs, arena);
            break;
        }

        rc_writer_put(w, RC_STR(" else if ("), arena);
        node = parts.other;
    }
}


// Write the case or default label for one of a clause's selectors.
static void rc_glsl_write_selector(rc_writer *w, uint32_t selector, rc_arena *arena)
{
    rc_writer_line(w, arena);
    if (rc_wgsl_ast_type(w->module.ast, selector) == RC_WGSL_NODE_TYPE_DEFAULT) {
        rc_writer_put(w, RC_STR("default:"), arena);
        return;
    }

    rc_writer_put(w, RC_STR("case "), arena);
    rc_glsl_write_expr(w, selector, 0, arena);
    rc_writer_put(w, RC_STR(":"), arena);
}


// Write one clause of a switch: its labels, then its statements in braces, ending with a break.
static void rc_glsl_write_clause(rc_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node clause = rc_wgsl_ast_node(w->module.ast, node);
    rc_view_wgsl_index selectors = rc_wgsl_ast_list(w->module.ast, clause.lhs);

    for (uint32_t i = 0; i < selectors.num; i++) {
        rc_glsl_write_selector(w, rc_view_wgsl_index_get(selectors, i), arena);
    }
    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("{"), arena);
    w->indent++;
    rc_glsl_write_statements(w, rc_wgsl_ast_node(w->module.ast, clause.rhs).lhs, arena);
    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("break;"), arena);
    w->indent--;
    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("}"), arena);
}


// Write a switch. Each clause ends with a break, since WGSL clauses never fall through.
static void rc_glsl_write_switch(rc_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    rc_view_wgsl_index clauses = rc_wgsl_ast_list(w->module.ast, n.rhs);

    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("switch ("), arena);
    rc_glsl_write_expr(w, n.lhs, 0, arena);
    rc_writer_put(w, RC_STR(") {"), arena);
    for (uint32_t i = 0; i < clauses.num && !rc_writer_failed(w); i++) {
        rc_glsl_write_clause(w, rc_view_wgsl_index_get(clauses, i), arena);
    }
    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("}"), arena);
}


// The number of continues in the statement node that go round the loop it is in, rather than a
// loop inside it.
static uint32_t rc_glsl_count_continues(const rc_writer *w, uint32_t node)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    uint32_t count = 0;

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_CONTINUE:
        return 1;
    case RC_WGSL_NODE_TYPE_BLOCK: {
        rc_view_wgsl_index statements = rc_wgsl_ast_list(w->module.ast, n.lhs);

        for (uint32_t i = 0; i < statements.num; i++) {
            count += rc_glsl_count_continues(w, rc_view_wgsl_index_get(statements, i));
        }
        return count;
    }
    case RC_WGSL_NODE_TYPE_IF: {
        rc_wgsl_if_parts parts = rc_wgsl_ast_if_parts(w->module.ast, node);

        return rc_glsl_count_continues(w, parts.then) + rc_glsl_count_continues(w, parts.other);
    }
    case RC_WGSL_NODE_TYPE_SWITCH: {
        rc_view_wgsl_index clauses = rc_wgsl_ast_list(w->module.ast, n.rhs);

        for (uint32_t i = 0; i < clauses.num; i++) {
            uint32_t clause = rc_view_wgsl_index_get(clauses, i);

            count += rc_glsl_count_continues(w, rc_wgsl_ast_node(w->module.ast, clause).rhs);
        }
        return count;
    }
    default:
        return 0;
    }
}


// True if anything under root names one of the declarations in decls.
static bool rc_glsl_mentions(const rc_writer *w, uint32_t root, rc_view_wgsl_index decls, rc_arena *arena)
{
    rc_array_wgsl_index pending = rc_array_wgsl_index_make(16, arena);

    rc_array_wgsl_index_push(&pending, root, arena);
    while (pending.num > 0) {
        uint32_t node = rc_array_wgsl_index_pop(&pending);
        uint32_t target = rc_writer_sema(w, node).target;

        if (node == 0) {
            continue;
        }

        if (rc_wgsl_ast_type(w->module.ast, node) == RC_WGSL_NODE_TYPE_IDENT) {
            for (uint32_t i = 0; i < decls.num; i++) {
                if (rc_view_wgsl_index_get(decls, i) == target) {
                    return true;
                }
            }
        }

        rc_wgsl_ast_push_children(w->module.ast, node, &pending, arena);
    }
    return false;
}


// Write a line of start, the name of the flag of the loop node, and end.
static void rc_glsl_write_flag_line(rc_writer *w, rc_str start, uint32_t node, rc_str end, rc_arena *arena)
{
    rc_writer_line(w, arena);
    rc_writer_put(w, start, arena);
    rc_writer_put(w, RC_STR("rc_loop_"), arena);
    rc_mstr_append_u32(&w->code, node, arena);
    rc_writer_put(w, end, arena);
}


// Write the break out of a loop when its continuing block's break-if condition holds.
static void rc_glsl_write_break_if(rc_writer *w, uint32_t condition, rc_arena *arena)
{
    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("if ("), arena);
    rc_glsl_write_expr(w, condition, 0, arena);
    rc_writer_put(w, RC_STR(") {"), arena);
    w->indent++;
    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("break;"), arena);
    w->indent--;
    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("}"), arena);
}


// Write a loop. See the top of this file.
static void rc_glsl_write_loop(rc_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    uint32_t body = rc_wgsl_ast_node(w->module.ast, n.lhs).lhs;
    rc_wgsl_node continuing = rc_wgsl_ast_node(w->module.ast, n.rhs);
    uint32_t continuing_list = rc_wgsl_ast_node(w->module.ast, continuing.lhs).lhs;
    bool flag = n.rhs != 0 && rc_glsl_count_continues(w, n.lhs) > 0;

    // Behind the flag, the continuing block is outside the body's scope
    if (flag && rc_glsl_mentions(w, n.rhs, rc_wgsl_ast_list(w->module.ast, body), arena)) {
        rc_writer_fail(w, n.rhs, RC_STR("a continuing block can only use names from its loop's body "
            "when the body doesn't use continue"));
        return;
    }

    if (flag) {
        rc_glsl_write_flag_line(w, RC_STR("bool "), node, RC_STR(" = true;"), arena);
    }

    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("while (true) {"), arena);
    w->indent++;
    if (flag) {
        rc_glsl_write_flag_line(w, RC_STR("if (!"), node, RC_STR(") {"), arena);
        w->indent++;
    }

    if (!flag) {
        rc_glsl_write_statements(w, body, arena);
    }

    if (n.rhs != 0) {
        rc_glsl_write_statements(w, continuing_list, arena);
    }

    if (continuing.rhs != 0) {
        rc_glsl_write_break_if(w, continuing.rhs, arena);
    }

    if (flag) {
        w->indent--;
        rc_writer_line(w, arena);
        rc_writer_put(w, RC_STR("}"), arena);
        rc_glsl_write_flag_line(w, RC_STR(""), node, RC_STR(" = false;"), arena);
        rc_glsl_write_statements(w, body, arena);
    }

    w->indent--;
    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("}"), arena);
}


// True for the nodes that declare a local: var, let and const.
static bool rc_glsl_is_local(rc_wgsl_node_type type)
{
    switch (type) {
    case RC_WGSL_NODE_TYPE_VAR:
    case RC_WGSL_NODE_TYPE_LET:
    case RC_WGSL_NODE_TYPE_CONST:
        return true;
    default:
        return false;
    }
}


// Write the initialiser of a for loop, a local or an update, if it writes any code.
static void rc_glsl_write_for_init(rc_writer *w, uint32_t init, rc_arena *arena)
{
    bool declares = rc_glsl_is_local(rc_wgsl_ast_type(w->module.ast, init));

    if (declares && rc_glsl_local_writes_code(w, init)) {
        rc_glsl_write_local(w, init, arena);
    }
    else if (!declares && rc_glsl_update_writes_code(w, init)) {
        rc_glsl_write_update(w, init, arena);
    }
}


// Write a for loop.
static void rc_glsl_write_for(rc_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_for_parts parts = rc_wgsl_ast_for_parts(w->module.ast, node);

    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("for ("), arena);
    if (parts.init != 0) {
        rc_glsl_write_for_init(w, parts.init, arena);
    }

    rc_writer_put(w, RC_STR("; "), arena);
    if (parts.condition != 0) {
        rc_glsl_write_expr(w, parts.condition, 0, arena);
    }

    rc_writer_put(w, RC_STR("; "), arena);
    if (parts.update != 0 && rc_glsl_update_writes_code(w, parts.update)) {
        rc_glsl_write_update(w, parts.update, arena);
    }

    rc_writer_put(w, RC_STR(")"), arena);
    rc_glsl_write_braced(w, rc_wgsl_ast_node(w->module.ast, rc_wgsl_ast_node(w->module.ast, node).rhs).lhs, arena);
}

// ---- statements ----

// Write a statement on a line of its own: just its text and a ';'.
static void rc_glsl_write_simple(rc_writer *w, rc_str text, rc_arena *arena)
{
    rc_writer_line(w, arena);
    rc_writer_put(w, text, arena);
}


// Write the statement node, on lines of its own.
static void rc_glsl_write_statement(rc_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_BLOCK:
        rc_writer_line(w, arena);
        rc_writer_put(w, RC_STR("{"), arena);
        w->indent++;
        rc_glsl_write_statements(w, n.lhs, arena);
        w->indent--;
        rc_glsl_write_simple(w, RC_STR("}"), arena);
        break;
    case RC_WGSL_NODE_TYPE_VAR:
    case RC_WGSL_NODE_TYPE_LET:
    case RC_WGSL_NODE_TYPE_CONST:
        if (rc_glsl_is_pointer_let(w, node) && !rc_writer_is_stable_reference(w, n.rhs)) {
            rc_writer_fail(w, n.rhs, RC_STR("this pointer's index can change: give the index a let of its own"));
        }
        else if (rc_glsl_local_writes_code(w, node)) {
            rc_writer_line(w, arena);
            rc_glsl_write_local(w, node, arena);
            rc_writer_put(w, RC_STR(";"), arena);
        }

        break;
    case RC_WGSL_NODE_TYPE_CONST_ASSERT:
        break;
    case RC_WGSL_NODE_TYPE_RETURN:
        rc_writer_line(w, arena);
        rc_writer_put(w, n.lhs != 0 ? RC_STR("return ") : RC_STR("return"), arena);
        if (n.lhs != 0) {
            rc_glsl_write_expr(w, n.lhs, 0, arena);
        }

        rc_writer_put(w, RC_STR(";"), arena);
        break;
    case RC_WGSL_NODE_TYPE_IF:
        rc_glsl_write_if(w, node, arena);
        break;
    case RC_WGSL_NODE_TYPE_SWITCH:
        rc_glsl_write_switch(w, node, arena);
        break;
    case RC_WGSL_NODE_TYPE_LOOP:
        rc_glsl_write_loop(w, node, arena);
        break;
    case RC_WGSL_NODE_TYPE_FOR:
        rc_glsl_write_for(w, node, arena);
        break;
    case RC_WGSL_NODE_TYPE_WHILE:
        rc_writer_line(w, arena);
        rc_writer_put(w, RC_STR("while ("), arena);
        rc_glsl_write_expr(w, n.lhs, 0, arena);
        rc_writer_put(w, RC_STR(")"), arena);
        rc_glsl_write_braced(w, rc_wgsl_ast_node(w->module.ast, n.rhs).lhs, arena);
        break;
    case RC_WGSL_NODE_TYPE_BREAK:
        rc_glsl_write_simple(w, RC_STR("break;"), arena);
        break;
    case RC_WGSL_NODE_TYPE_CONTINUE:
        rc_glsl_write_simple(w, RC_STR("continue;"), arena);
        break;
    case RC_WGSL_NODE_TYPE_DISCARD:
        rc_glsl_write_simple(w, RC_STR("discard;"), arena);
        break;
    default:
        if (rc_glsl_update_writes_code(w, node)) {
            rc_writer_line(w, arena);
            rc_glsl_write_update(w, node, arena);
            rc_writer_put(w, RC_STR(";"), arena);
        }

        break;
    }
}

// ---- functions ----

// True for the types of samplers and textures.
static bool rc_glsl_is_sampler_or_texture(rc_wgsl_typeinfo_type type)
{
    switch (type) {
    case RC_WGSL_TYPEINFO_TYPE_SAMPLER:
    case RC_WGSL_TYPEINFO_TYPE_SAMPLER_COMPARISON:
    case RC_WGSL_TYPEINFO_TYPE_TEXTURE:
        return true;
    default:
        return false;
    }
}


// Write a function's parameter: a pointer as inout, since GLSL has none. A texture or sampler can't
// be one, since GLSL combines each texture with its sampler where they are declared.
static void rc_glsl_write_param(rc_writer *w, uint32_t param, rc_arena *arena)
{
    uint32_t type = rc_writer_sema(w, param).type;
    rc_wgsl_typeinfo t = rc_writer_typeinfo(w, type);
    rc_str name = rc_wgsl_ast_text(w->module.ast, param);

    if (rc_glsl_is_sampler_or_texture((rc_wgsl_typeinfo_type)t.type)) {
        rc_writer_fail_target(w, param, RC_STR("a sampler or texture parameter"), arena);
    }
    else if (t.type == RC_WGSL_TYPEINFO_TYPE_POINTER) {
        rc_writer_put(w, RC_STR("inout "), arena);
        rc_glsl_write_declaration(w, &w->code, t.store_type, name, param, arena);
    }
    else {
        rc_glsl_write_declaration(w, &w->code, type, name, param, arena);
    }
}


// Write a function's parameters, separated by commas.
static void rc_glsl_write_params(rc_writer *w, uint32_t params, rc_arena *arena)
{
    rc_view_wgsl_index list = rc_wgsl_ast_list(w->module.ast, params);

    for (uint32_t i = 0; i < list.num; i++) {
        rc_writer_put(w, i > 0 ? RC_STR(", ") : RC_STR(""), arena);
        rc_glsl_write_param(w, rc_view_wgsl_index_get(list, i), arena);
    }
}


void rc_glsl_write_function(rc_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_fn_parts parts = rc_wgsl_ast_fn_parts(w->module.ast, node);
    uint32_t type = rc_writer_sema(w, node).type;

    rc_writer_line(w, arena);
    if (type != 0) {
        rc_glsl_write_type(w, &w->code, type, node, arena);
    }
    else {
        rc_writer_put(w, RC_STR("void"), arena);
    }

    rc_writer_put(w, RC_STR(" "), arena);
    rc_glsl_append_name(&w->code, rc_wgsl_ast_text(w->module.ast, node), arena);
    rc_writer_put(w, RC_STR("("), arena);
    rc_glsl_write_params(w, parts.params, arena);
    rc_writer_put(w, RC_STR(")"), arena);
    rc_glsl_write_braced(w, rc_wgsl_ast_node(w->module.ast, parts.body).lhs, arena);
    rc_writer_put(w, RC_STR("\n"), arena);
}
