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

static void rc_glsl_write_statement(rc_glsl_writer *w, uint32_t node, rc_arena *arena);

// ---- blocks ----

// Write each statement of list on its own line.
static void rc_glsl_write_statements(rc_glsl_writer *w, uint32_t list, rc_arena *arena)
{
    rc_view_wgsl_index statements = rc_wgsl_ast_list(w->module.ast, list);

    for (uint32_t i = 0; i < statements.num && !rc_glsl_failed(w); i++) {
        rc_glsl_write_statement(w, rc_view_wgsl_index_get(statements, i), arena);
    }
}


// Write " {", the statements of list indented, and "}" on a line of its own.
static void rc_glsl_write_braced(rc_glsl_writer *w, uint32_t list, rc_arena *arena)
{
    rc_glsl_put(w, RC_STR(" {"), arena);
    w->indent++;
    rc_glsl_write_statements(w, list, arena);
    w->indent--;
    rc_glsl_line(w, arena);
    rc_glsl_put(w, RC_STR("}"), arena);
}

// ---- declarations ----

// True if node is a let of pointer type, which is written out at each use.
static bool rc_glsl_is_pointer_let(const rc_glsl_writer *w, uint32_t node)
{
    return rc_wgsl_ast_type(w->module.ast, node) == RC_WGSL_NODE_TYPE_LET
        && rc_glsl_typeinfo(w, rc_glsl_sema(w, node).type).type == RC_WGSL_TYPEINFO_TYPE_POINTER;
}


// True if node is a const of abstract type, which is written out at each use.
static bool rc_glsl_is_abstract_const(const rc_glsl_writer *w, uint32_t node)
{
    return rc_wgsl_ast_type(w->module.ast, node) == RC_WGSL_NODE_TYPE_CONST
        && rc_wgsl_type_is_abstract(w->types.view, rc_glsl_sema(w, node).type);
}


// True if every index in the reference expression node stays the same once evaluated, so
// writing it out again at each use means the same memory.
static bool rc_glsl_is_stable_reference(const rc_glsl_writer *w, uint32_t node)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    rc_wgsl_sema index;
    rc_wgsl_node_type decl;

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_UNARY:
    case RC_WGSL_NODE_TYPE_MEMBER:
        return rc_glsl_is_stable_reference(w, n.lhs);
    case RC_WGSL_NODE_TYPE_INDEX:
        index = rc_glsl_sema(w, n.rhs);
        decl = rc_wgsl_ast_type(w->module.ast, index.target);
        if (!(index.flags & RC_WGSL_SEMA_CONST) && (rc_wgsl_ast_type(w->module.ast, n.rhs)
            != RC_WGSL_NODE_TYPE_IDENT
            || (decl != RC_WGSL_NODE_TYPE_LET && decl != RC_WGSL_NODE_TYPE_PARAM))) {
            return false;
        }
        return rc_glsl_is_stable_reference(w, n.lhs);
    default:
        return true;
    }
}


// True if the local declaration node writes any code.
static bool rc_glsl_local_writes_code(const rc_glsl_writer *w, uint32_t node)
{
    return !rc_glsl_is_pointer_let(w, node) && !rc_glsl_is_abstract_const(w, node);
}


// Write a var, let or const in a function, without its ';'. A const is a plain variable: WGSL
// keeps it from changing, and GLSL's const won't take a polyfill's call.
static void rc_glsl_write_local(rc_glsl_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    uint32_t type = rc_glsl_sema(w, node).type;
    uint32_t initialiser = n.type == RC_WGSL_NODE_TYPE_VAR
        ? rc_wgsl_ast_var_parts(w->module.ast, node).initialiser : n.rhs;

    rc_glsl_write_declaration(w, &w->code, type, rc_wgsl_ast_text(w->module.ast, node), node,
        arena);
    rc_glsl_put(w, RC_STR(" = "), arena);
    if (initialiser != 0) {
        rc_glsl_write_expr(w, initialiser, 0, arena);
    }
    else {
        rc_glsl_write_zero(w, &w->code, type, node, arena);
    }
}

// ---- updates ----

// True if the update statement node writes any code: _ = e only does when e calls something.
static bool rc_glsl_update_writes_code(const rc_glsl_writer *w, uint32_t node)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);

    return n.type != RC_WGSL_NODE_TYPE_ASSIGN
        || rc_wgsl_ast_type(w->module.ast, n.lhs) != RC_WGSL_NODE_TYPE_PHONY
        || rc_glsl_has_calls(w, n.rhs);
}


// The plain operator a compound assignment's token applies.
static rc_wgsl_token_type rc_glsl_compound_operator(rc_wgsl_token_type op)
{
    switch (op) {
    case RC_WGSL_TOKEN_TYPE_PLUS_EQUAL:
        return RC_WGSL_TOKEN_TYPE_PLUS;
    case RC_WGSL_TOKEN_TYPE_MINUS_EQUAL:
        return RC_WGSL_TOKEN_TYPE_MINUS;
    case RC_WGSL_TOKEN_TYPE_STAR_EQUAL:
        return RC_WGSL_TOKEN_TYPE_STAR;
    case RC_WGSL_TOKEN_TYPE_SLASH_EQUAL:
        return RC_WGSL_TOKEN_TYPE_SLASH;
    case RC_WGSL_TOKEN_TYPE_PERCENT_EQUAL:
        return RC_WGSL_TOKEN_TYPE_PERCENT;
    case RC_WGSL_TOKEN_TYPE_AND_EQUAL:
        return RC_WGSL_TOKEN_TYPE_AND;
    case RC_WGSL_TOKEN_TYPE_OR_EQUAL:
        return RC_WGSL_TOKEN_TYPE_OR;
    case RC_WGSL_TOKEN_TYPE_XOR_EQUAL:
        return RC_WGSL_TOKEN_TYPE_XOR;
    case RC_WGSL_TOKEN_TYPE_SHIFT_LEFT_EQUAL:
        return RC_WGSL_TOKEN_TYPE_SHIFT_LEFT;
    default:
        return RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT;
    }
}


// Write an assignment without its ';'. A compound one is written out in full, target = target op
// value, which needs a target that is the same memory each time it is evaluated.
static void rc_glsl_write_assign(rc_glsl_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    rc_wgsl_token_type op = (rc_wgsl_token_type)rc_view_wgsl_token_get(w->module.ast.tokens,
        n.token).type;

    if (rc_wgsl_ast_type(w->module.ast, n.lhs) == RC_WGSL_NODE_TYPE_PHONY) {
        rc_glsl_write_expr(w, n.rhs, 0, arena);
        return;
    }
    rc_glsl_write_expr(w, n.lhs, 0, arena);
    rc_glsl_put(w, RC_STR(" = "), arena);
    if (op == RC_WGSL_TOKEN_TYPE_EQUAL) {
        rc_glsl_write_expr(w, n.rhs, 0, arena);
        return;
    }
    if (rc_glsl_has_calls(w, n.lhs)) {
        rc_glsl_fail(w, n.lhs,
            RC_STR("this target calls a function, so it can't take a compound assignment"));
        return;
    }
    rc_glsl_write_operation(w, rc_glsl_compound_operator(op), n.lhs, n.rhs,
        rc_glsl_sema(w, n.lhs).type, 0, arena);
}


// Write an assignment, step or call, without its ';'.
static void rc_glsl_write_update(rc_glsl_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_ASSIGN:
        rc_glsl_write_assign(w, node, arena);
        break;
    case RC_WGSL_NODE_TYPE_INCREMENT:
    case RC_WGSL_NODE_TYPE_DECREMENT:
        rc_glsl_write_expr(w, n.lhs, 0, arena);
        rc_glsl_put(w, n.type == RC_WGSL_NODE_TYPE_INCREMENT ? RC_STR("++") : RC_STR("--"), arena);
        break;
    default:
        rc_glsl_write_expr(w, node, 0, arena);
        break;
    }
}

// ---- control flow ----

// Write an if, its else-ifs and its else.
static void rc_glsl_write_if(rc_glsl_writer *w, uint32_t node, rc_arena *arena)
{
    rc_glsl_line(w, arena);
    rc_glsl_put(w, RC_STR("if ("), arena);
    while (!rc_glsl_failed(w)) {
        rc_wgsl_if_parts parts = rc_wgsl_ast_if_parts(w->module.ast, node);

        rc_glsl_write_expr(w, rc_wgsl_ast_node(w->module.ast, node).lhs, 0, arena);
        rc_glsl_put(w, RC_STR(")"), arena);
        rc_glsl_write_braced(w, rc_wgsl_ast_node(w->module.ast, parts.then).lhs, arena);
        if (parts.other == 0) {
            break;
        }
        if (rc_wgsl_ast_type(w->module.ast, parts.other) != RC_WGSL_NODE_TYPE_IF) {
            rc_glsl_put(w, RC_STR(" else"), arena);
            rc_glsl_write_braced(w, rc_wgsl_ast_node(w->module.ast, parts.other).lhs, arena);
            break;
        }
        rc_glsl_put(w, RC_STR(" else if ("), arena);
        node = parts.other;
    }
}


// Write a switch. Each clause ends with a break, since WGSL clauses never fall through.
static void rc_glsl_write_switch(rc_glsl_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    rc_view_wgsl_index clauses = rc_wgsl_ast_list(w->module.ast, n.rhs);

    rc_glsl_line(w, arena);
    rc_glsl_put(w, RC_STR("switch ("), arena);
    rc_glsl_write_expr(w, n.lhs, 0, arena);
    rc_glsl_put(w, RC_STR(") {"), arena);
    for (uint32_t i = 0; i < clauses.num && !rc_glsl_failed(w); i++) {
        rc_wgsl_node clause = rc_wgsl_ast_node(w->module.ast, rc_view_wgsl_index_get(clauses, i));
        rc_view_wgsl_index selectors = rc_wgsl_ast_list(w->module.ast, clause.lhs);

        for (uint32_t j = 0; j < selectors.num; j++) {
            uint32_t selector = rc_view_wgsl_index_get(selectors, j);

            rc_glsl_line(w, arena);
            if (rc_wgsl_ast_type(w->module.ast, selector) == RC_WGSL_NODE_TYPE_DEFAULT) {
                rc_glsl_put(w, RC_STR("default:"), arena);
                continue;
            }
            rc_glsl_put(w, RC_STR("case "), arena);
            rc_glsl_write_expr(w, selector, 0, arena);
            rc_glsl_put(w, RC_STR(":"), arena);
        }
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("{"), arena);
        w->indent++;
        rc_glsl_write_statements(w, rc_wgsl_ast_node(w->module.ast, clause.rhs).lhs, arena);
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("break;"), arena);
        w->indent--;
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("}"), arena);
    }
    rc_glsl_line(w, arena);
    rc_glsl_put(w, RC_STR("}"), arena);
}


// The number of continues in the statement node that go round the loop it is in, rather than a
// loop inside it.
static uint32_t rc_glsl_count_continues(const rc_glsl_writer *w, uint32_t node)
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
static bool rc_glsl_mentions(const rc_glsl_writer *w, uint32_t root, rc_view_wgsl_index decls,
    rc_arena *arena)
{
    rc_array_wgsl_index pending = rc_array_wgsl_index_make(16, arena);

    rc_array_wgsl_index_push(&pending, root, arena);
    while (pending.num > 0) {
        uint32_t node = rc_array_wgsl_index_pop(&pending);
        uint32_t target = rc_glsl_sema(w, node).target;

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


// Write a loop. See the top of this file.
static void rc_glsl_write_loop(rc_glsl_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);
    uint32_t body = rc_wgsl_ast_node(w->module.ast, n.lhs).lhs;
    rc_wgsl_node continuing = rc_wgsl_ast_node(w->module.ast, n.rhs);
    uint32_t continuing_list = rc_wgsl_ast_node(w->module.ast, continuing.lhs).lhs;
    bool flag = n.rhs != 0 && rc_glsl_count_continues(w, n.lhs) > 0;

    // Behind the flag, the continuing block is outside the body's scope
    if (flag && rc_glsl_mentions(w, n.rhs, rc_wgsl_ast_list(w->module.ast, body), arena)) {
        rc_glsl_fail(w, n.rhs, RC_STR("a continuing block can only use names from its loop's body "
            "when the body doesn't use continue"));
        return;
    }
    if (flag) {
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("bool rc_loop_"), arena);
        rc_mstr_append_u32(&w->code, node, arena);
        rc_glsl_put(w, RC_STR(" = true;"), arena);
    }
    rc_glsl_line(w, arena);
    rc_glsl_put(w, RC_STR("while (true) {"), arena);
    w->indent++;
    if (flag) {
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("if (!rc_loop_"), arena);
        rc_mstr_append_u32(&w->code, node, arena);
        rc_glsl_put(w, RC_STR(") {"), arena);
        w->indent++;
    }
    if (!flag) {
        rc_glsl_write_statements(w, body, arena);
    }
    if (n.rhs != 0) {
        rc_glsl_write_statements(w, continuing_list, arena);
    }
    if (continuing.rhs != 0) {
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("if ("), arena);
        rc_glsl_write_expr(w, continuing.rhs, 0, arena);
        rc_glsl_put(w, RC_STR(") {"), arena);
        w->indent++;
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("break;"), arena);
        w->indent--;
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("}"), arena);
    }
    if (flag) {
        w->indent--;
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("}"), arena);
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("rc_loop_"), arena);
        rc_mstr_append_u32(&w->code, node, arena);
        rc_glsl_put(w, RC_STR(" = false;"), arena);
        rc_glsl_write_statements(w, body, arena);
    }
    w->indent--;
    rc_glsl_line(w, arena);
    rc_glsl_put(w, RC_STR("}"), arena);
}


// Write a for loop.
static void rc_glsl_write_for(rc_glsl_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_for_parts parts = rc_wgsl_ast_for_parts(w->module.ast, node);
    rc_wgsl_node_type init = rc_wgsl_ast_type(w->module.ast, parts.init);
    bool declares = init == RC_WGSL_NODE_TYPE_VAR || init == RC_WGSL_NODE_TYPE_LET
        || init == RC_WGSL_NODE_TYPE_CONST;

    rc_glsl_line(w, arena);
    rc_glsl_put(w, RC_STR("for ("), arena);
    if (declares && rc_glsl_local_writes_code(w, parts.init)) {
        rc_glsl_write_local(w, parts.init, arena);
    }
    else if (!declares && parts.init != 0 && rc_glsl_update_writes_code(w, parts.init)) {
        rc_glsl_write_update(w, parts.init, arena);
    }
    rc_glsl_put(w, RC_STR("; "), arena);
    if (parts.condition != 0) {
        rc_glsl_write_expr(w, parts.condition, 0, arena);
    }
    rc_glsl_put(w, RC_STR("; "), arena);
    if (parts.update != 0 && rc_glsl_update_writes_code(w, parts.update)) {
        rc_glsl_write_update(w, parts.update, arena);
    }
    rc_glsl_put(w, RC_STR(")"), arena);
    rc_glsl_write_braced(w, rc_wgsl_ast_node(w->module.ast,
        rc_wgsl_ast_node(w->module.ast, node).rhs).lhs, arena);
}

// ---- statements ----

// Write a statement on a line of its own: just its text and a ';'.
static void rc_glsl_write_simple(rc_glsl_writer *w, rc_str text, rc_arena *arena)
{
    rc_glsl_line(w, arena);
    rc_glsl_put(w, text, arena);
}


static void rc_glsl_write_statement(rc_glsl_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_BLOCK:
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("{"), arena);
        w->indent++;
        rc_glsl_write_statements(w, n.lhs, arena);
        w->indent--;
        rc_glsl_write_simple(w, RC_STR("}"), arena);
        break;
    case RC_WGSL_NODE_TYPE_VAR:
    case RC_WGSL_NODE_TYPE_LET:
    case RC_WGSL_NODE_TYPE_CONST:
        if (rc_glsl_is_pointer_let(w, node) && !rc_glsl_is_stable_reference(w, n.rhs)) {
            rc_glsl_fail(w, n.rhs,
                RC_STR("this pointer's index can change: give the index a let of its own"));
        }
        else if (rc_glsl_local_writes_code(w, node)) {
            rc_glsl_line(w, arena);
            rc_glsl_write_local(w, node, arena);
            rc_glsl_put(w, RC_STR(";"), arena);
        }
        break;
    case RC_WGSL_NODE_TYPE_CONST_ASSERT:
        break;
    case RC_WGSL_NODE_TYPE_RETURN:
        rc_glsl_line(w, arena);
        rc_glsl_put(w, n.lhs != 0 ? RC_STR("return ") : RC_STR("return"), arena);
        if (n.lhs != 0) {
            rc_glsl_write_expr(w, n.lhs, 0, arena);
        }
        rc_glsl_put(w, RC_STR(";"), arena);
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
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("while ("), arena);
        rc_glsl_write_expr(w, n.lhs, 0, arena);
        rc_glsl_put(w, RC_STR(")"), arena);
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
            rc_glsl_line(w, arena);
            rc_glsl_write_update(w, node, arena);
            rc_glsl_put(w, RC_STR(";"), arena);
        }
        break;
    }
}

// ---- functions ----

// Write a function's parameters: pointers as inout, since GLSL has none.
static void rc_glsl_write_params(rc_glsl_writer *w, uint32_t params, rc_arena *arena)
{
    rc_view_wgsl_index list = rc_wgsl_ast_list(w->module.ast, params);

    for (uint32_t i = 0; i < list.num; i++) {
        uint32_t param = rc_view_wgsl_index_get(list, i);
        rc_wgsl_typeinfo type = rc_glsl_typeinfo(w, rc_glsl_sema(w, param).type);

        rc_glsl_put(w, i > 0 ? RC_STR(", ") : RC_STR(""), arena);
        if (type.type == RC_WGSL_TYPEINFO_TYPE_POINTER) {
            rc_glsl_put(w, RC_STR("inout "), arena);
            rc_glsl_write_declaration(w, &w->code, type.element,
                rc_wgsl_ast_text(w->module.ast, param), param, arena);
        }
        else {
            rc_glsl_write_declaration(w, &w->code, rc_glsl_sema(w, param).type,
                rc_wgsl_ast_text(w->module.ast, param), param, arena);
        }
    }
}


void rc_glsl_write_function(rc_glsl_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_fn_parts parts = rc_wgsl_ast_fn_parts(w->module.ast, node);
    uint32_t type = rc_glsl_sema(w, node).type;

    rc_glsl_line(w, arena);
    if (type != 0) {
        rc_glsl_write_type(w, &w->code, type, node, arena);
    }
    else {
        rc_glsl_put(w, RC_STR("void"), arena);
    }
    rc_glsl_put(w, RC_STR(" "), arena);
    rc_glsl_append_name(&w->code, rc_wgsl_ast_text(w->module.ast, node), arena);
    rc_glsl_put(w, RC_STR("("), arena);
    rc_glsl_write_params(w, parts.params, arena);
    rc_glsl_put(w, RC_STR(")"), arena);
    rc_glsl_write_braced(w, rc_wgsl_ast_node(w->module.ast, parts.body).lhs, arena);
    rc_glsl_put(w, RC_STR("\n"), arena);
}
