// check_stmt.c
//
// Statements, and the function bodies made of them.
//
// Each statement's check says whether control can carry on past it, as WGSL's behaviour analysis
// does, so a function that returns a value can be held to returning on every path. A loop carries
// on only if something breaks out of it, so the checker notes each break that reaches the
// innermost loop or switch.

#include "checker.h"

#include "richc/mstr.h"

static bool rc_wgsl_check_statement(rc_wgsl_checker *c, uint32_t node, rc_arena *arena);

// ---- blocks and scopes ----

// Check the statements in list in turn, returning true if control can reach the end.
static bool rc_wgsl_check_statements(rc_wgsl_checker *c, uint32_t list, rc_arena *arena)
{
    rc_view_wgsl_index statements = rc_wgsl_ast_list(c->ast, list);
    bool falls = true;

    // Code after a return is fine, but control never reaches it
    for (uint32_t i = 0; i < statements.num && !rc_wgsl_check_failed(c); i++) {
        falls = rc_wgsl_check_statement(c, rc_view_wgsl_index_get(statements, i), arena) && falls;
    }
    return falls;
}


// Where a new scope starts, for rc_wgsl_leave_scope to go back to.
typedef struct rc_wgsl_scope {
    uint32_t locals;
    uint32_t scope;
} rc_wgsl_scope;

// Open a scope: names declared from now on belong to it.
static rc_wgsl_scope rc_wgsl_enter_scope(rc_wgsl_checker *c)
{
    rc_wgsl_scope saved = {.locals = c->locals.num, .scope = c->scope};

    c->scope = c->locals.num;
    return saved;
}


// Close the scope opened as saved, forgetting its names.
static void rc_wgsl_leave_scope(rc_wgsl_checker *c, rc_wgsl_scope saved)
{
    c->locals.num = saved.locals;
    c->scope = saved.scope;
}


// The statements of the BLOCK node, in a scope of their own.
static bool rc_wgsl_check_block(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_scope saved = rc_wgsl_enter_scope(c);
    bool falls = rc_wgsl_check_statements(c, rc_wgsl_ast_node(c->ast, node).lhs, arena);

    rc_wgsl_leave_scope(c, saved);
    return falls;
}


// What a loop or switch saves of the checker's state, to put back when it ends.
typedef struct rc_wgsl_breakable {
    bool broke;
    bool in_continuing;
} rc_wgsl_breakable;

// Enter a loop, or a switch if not loop: a statement that break leaves.
static rc_wgsl_breakable rc_wgsl_enter_breakable(rc_wgsl_checker *c, bool loop)
{
    rc_wgsl_breakable saved = {.broke = c->broke, .in_continuing = c->in_continuing};

    c->broke = false;
    c->in_continuing = false;
    c->breakables++;
    c->loops += loop ? 1 : 0;
    return saved;
}


// Leave the loop or switch entered as saved, returning whether a break left it.
static bool rc_wgsl_leave_breakable(rc_wgsl_checker *c, rc_wgsl_breakable saved, bool loop)
{
    bool broke = c->broke;

    c->broke = saved.broke;
    c->in_continuing = saved.in_continuing;
    c->breakables--;
    c->loops -= loop ? 1 : 0;
    return broke;
}

// ---- simple statements ----

// Check the expression node, which must be a bool.
static void rc_wgsl_check_condition(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    uint32_t type = rc_wgsl_check_value(c, node, arena);

    if (!rc_wgsl_check_failed(c) && type != RC_WGSL_TYPE_ID_BOOL) {
        rc_wgsl_check_fail_type(c, node, RC_STR("expected bool, got "), type, RC_STR(""), arena);
    }
}


// A var in a function, which lives in function memory.
static void rc_wgsl_check_local_var(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_var_parts parts = rc_wgsl_ast_var_parts(c->ast, node);
    rc_view_wgsl_index templates = rc_wgsl_ast_list(c->ast, parts.templates);
    uint32_t type = parts.type != 0 ? rc_wgsl_check_type_expr(c, parts.type, arena) : 0;

    if (templates.num > 1 || (templates.num == 1 && rc_wgsl_check_enumerant(c,
        rc_view_wgsl_index_get(templates, 0), RC_WGSL_ENUMERANT_SPACE) != RC_WGSL_SPACE_FUNCTION)) {
        rc_wgsl_check_fail(c, node, RC_STR("a var in a function lives in function memory"));
    }
    if (parts.initialiser != 0) {
        rc_wgsl_check_value(c, parts.initialiser, arena);
        if (type != 0) {
            rc_wgsl_check_convert(c, parts.initialiser, type, arena);
        }
        else {
            rc_wgsl_check_concretise_default(c, parts.initialiser, arena);
            type = rc_wgsl_check_sema(c, parts.initialiser).type;
        }
    }
    if (rc_wgsl_check_failed(c)) {
        return;
    }
    if (type == 0) {
        rc_wgsl_check_fail(c, node, RC_STR("this var needs a type or a value"));
    }
    else if (!rc_wgsl_type_is_constructible(c->types.view, type)) {
        rc_wgsl_check_fail_type(c, node, RC_STR("a var can't hold "), type, RC_STR(""), arena);
    }

    rc_wgsl_check_set(c, node, (rc_wgsl_sema) {
        .type = type,
        .space = RC_WGSL_SPACE_FUNCTION,
        .access = RC_WGSL_ACCESS_READ_WRITE
    });
}


// A var, let or const in a function, which its scope sees from here on.
static void rc_wgsl_check_local_decl(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    if (rc_wgsl_ast_type(c->ast, node) == RC_WGSL_NODE_TYPE_VAR) {
        rc_wgsl_check_local_var(c, node, arena);
    }
    else {
        rc_wgsl_check_value_decl(c, node, arena);
    }
    rc_wgsl_check_declare_local(c, node, arena);
}


// A return, and its value, which must fit the function.
static bool rc_wgsl_check_return(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    uint32_t value = rc_wgsl_ast_node(c->ast, node).lhs;
    uint32_t type = rc_wgsl_check_sema(c, c->function).type;

    if (c->in_continuing) {
        rc_wgsl_check_fail(c, node, RC_STR("a continuing block can't return"));
    }
    else if (value != 0 && type == 0) {
        rc_wgsl_check_fail(c, value, RC_STR("this function returns nothing"));
    }
    else if (value == 0 && type != 0) {
        rc_wgsl_check_fail_type(c, node, RC_STR("this function must return "), type, RC_STR(""),
            arena);
    }
    else if (value != 0) {
        rc_wgsl_check_value(c, value, arena);
        rc_wgsl_check_convert(c, value, type, arena);
    }
    return false;
}


// A break, which leaves the innermost loop or switch.
static bool rc_wgsl_check_break(rc_wgsl_checker *c, uint32_t node)
{
    if (c->breakables == 0) {
        rc_wgsl_check_fail(c, node, RC_STR("a break must be in a loop or switch"));
    }
    else if (c->in_continuing) {
        rc_wgsl_check_fail(c, node, RC_STR("a continuing block leaves its loop by 'break if'"));
    }
    c->broke = true;
    return false;
}


// A continue, which goes round the innermost loop again.
static bool rc_wgsl_check_continue(rc_wgsl_checker *c, uint32_t node)
{
    if (c->loops == 0) {
        rc_wgsl_check_fail(c, node, RC_STR("a continue must be in a loop"));
    }
    else if (c->in_continuing) {
        rc_wgsl_check_fail(c, node, RC_STR("a continuing block can't continue"));
    }
    return false;
}


// Fail for a target that isn't memory, saying what it is if it has a name.
static void rc_wgsl_fail_not_memory(rc_wgsl_checker *c, uint32_t target, rc_arena *arena)
{
    uint32_t decl = rc_wgsl_ast_type(c->ast, target) == RC_WGSL_NODE_TYPE_IDENT
        ? rc_wgsl_check_sema(c, target).target : 0;
    rc_wgsl_node_type node_type = rc_wgsl_ast_type(c->ast, decl);

    if (node_type == RC_WGSL_NODE_TYPE_LET || node_type == RC_WGSL_NODE_TYPE_CONST
        || node_type == RC_WGSL_NODE_TYPE_PARAM) {
        rc_wgsl_check_fail_name(c, target, RC_STR("'"),
            node_type == RC_WGSL_NODE_TYPE_PARAM ? RC_STR("' is a parameter, which can't change")
                                       : RC_STR("' can't change: make it a var"), arena);
        return;
    }
    rc_wgsl_check_fail(c, target, RC_STR("can only change a var, or part of one"));
}


// Check that the expression target names memory that can be written, returning what it is.
static rc_wgsl_sema rc_wgsl_check_target(rc_wgsl_checker *c, uint32_t target, rc_arena *arena)
{
    rc_wgsl_sema sema = rc_wgsl_check_expr(c, target, arena);

    if (rc_wgsl_check_failed(c)) {
        return sema;
    }
    if (!(sema.flags & RC_WGSL_SEMA_REF)) {
        rc_wgsl_fail_not_memory(c, target, arena);
    }
    else if (sema.access == RC_WGSL_ACCESS_READ) {
        rc_wgsl_check_fail(c, target, RC_STR("this is read-only"));
    }
    return sema;
}


// The plain operator a compound assignment applies: + for +=.
static rc_wgsl_token_type rc_wgsl_compound_operator(rc_wgsl_token_type op)
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


// target op= value, which must give back target's own type.
static void rc_wgsl_check_compound(rc_wgsl_checker *c, uint32_t node, rc_wgsl_sema target,
    rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    uint32_t token = n.token;
    rc_wgsl_token_type op = rc_wgsl_compound_operator(
        (rc_wgsl_token_type)rc_view_wgsl_token_get(c->ast.tokens, token).type);
    uint32_t value = rc_wgsl_check_sema(c, n.rhs).type;
    uint32_t result = rc_wgsl_check_operation_type(c, op, target.type, value, arena);
    bool shift = op == RC_WGSL_TOKEN_TYPE_SHIFT_LEFT || op == RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT;
    uint32_t scalar = shift ? RC_WGSL_TYPE_ID_U32
                            : rc_wgsl_type_scalar_of(c->types.view, target.type);

    if (result != target.type) {
        rc_wgsl_check_fail_types(c, node, RC_STR("can't apply this to "), target.type,
            RC_STR(" with "), value, arena);
        return;
    }
    rc_wgsl_check_concretise(c, n.rhs, rc_wgsl_type_with_scalar(&c->types, value, scalar, arena),
        arena);
}


// An assignment: to memory, or to _, which takes anything.
static void rc_wgsl_check_assign(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_token_type op = (rc_wgsl_token_type)rc_view_wgsl_token_get(c->ast.tokens, n.token).type;
    rc_wgsl_sema target;

    if (rc_wgsl_ast_type(c->ast, n.lhs) == RC_WGSL_NODE_TYPE_PHONY) {
        rc_wgsl_check_value(c, n.rhs, arena);
        rc_wgsl_check_concretise_default(c, n.rhs, arena);
        return;
    }
    target = rc_wgsl_check_target(c, n.lhs, arena);
    rc_wgsl_check_value(c, n.rhs, arena);
    if (rc_wgsl_check_failed(c)) {
        return;
    }
    if (op != RC_WGSL_TOKEN_TYPE_EQUAL) {
        rc_wgsl_check_compound(c, node, target, arena);
    }
    else if (!rc_wgsl_type_is_constructible(c->types.view, target.type)) {
        rc_wgsl_check_fail_type(c, n.lhs, RC_STR("can't assign to "), target.type, RC_STR(""),
            arena);
    }
    else {
        rc_wgsl_check_convert(c, n.rhs, target.type, arena);
    }
}


// target++ or target--, on an integer.
static void rc_wgsl_check_step(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    uint32_t target = rc_wgsl_ast_node(c->ast, node).lhs;
    rc_wgsl_sema sema = rc_wgsl_check_target(c, target, arena);

    if (!rc_wgsl_check_failed(c) && sema.type != RC_WGSL_TYPE_ID_I32
        && sema.type != RC_WGSL_TYPE_ID_U32) {
        rc_wgsl_check_fail_type(c, target, RC_STR("only an i32 or u32 counts up or down, not "),
            sema.type, RC_STR(""), arena);
    }
}


// A call made for what it does, whose result goes unused.
static void rc_wgsl_check_call_statement(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_sema sema = rc_wgsl_check_expr(c, node, arena);

    if (rc_wgsl_check_failed(c) || sema.call == RC_WGSL_CALL_FUNCTION) {
        return;
    }
    if (sema.call == RC_WGSL_CALL_BUILTIN && sema.type != 0) {
        rc_wgsl_check_fail_name(c, node, RC_STR("the result of '"), RC_STR("' must be used"),
            arena);
    }
    else if (sema.call != RC_WGSL_CALL_BUILTIN) {
        rc_wgsl_check_fail(c, node, RC_STR("a value made here must be used"));
    }
}

// ---- compound statements ----

// An if, its else-ifs and its else.
static bool rc_wgsl_check_if(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_if_parts parts = rc_wgsl_ast_if_parts(c->ast, node);
    bool then_falls;
    bool other_falls = true;

    rc_wgsl_check_condition(c, rc_wgsl_ast_node(c->ast, node).lhs, arena);
    then_falls = rc_wgsl_check_block(c, parts.then, arena);
    if (parts.other != 0) {
        other_falls = rc_wgsl_ast_type(c->ast, parts.other) == RC_WGSL_NODE_TYPE_IF
            ? rc_wgsl_check_if(c, parts.other, arena) : rc_wgsl_check_block(c, parts.other, arena);
    }
    return then_falls || other_falls;
}


// The type a switch's selector and case values settle on together, all checked.
static uint32_t rc_wgsl_check_switch_type(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_view_wgsl_index clauses = rc_wgsl_ast_list(c->ast, n.rhs);
    uint32_t type = rc_wgsl_check_value(c, n.lhs, arena);

    for (uint32_t i = 0; i < clauses.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t clause = rc_view_wgsl_index_get(clauses, i);
        rc_view_wgsl_index selectors = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, clause).lhs);

        for (uint32_t j = 0; j < selectors.num && !rc_wgsl_check_failed(c); j++) {
            uint32_t selector = rc_view_wgsl_index_get(selectors, j);
            uint32_t case_type;

            if (rc_wgsl_ast_type(c->ast, selector) == RC_WGSL_NODE_TYPE_DEFAULT) {
                continue;
            }
            case_type = rc_wgsl_check_value(c, selector, arena);
            type = rc_wgsl_type_converts(c->types.view, type, case_type) ? case_type : type;
        }
    }
    if (!rc_wgsl_check_failed(c) && !rc_wgsl_type_is_integer_scalar(c->types.view, type)) {
        rc_wgsl_check_fail_type(c, n.lhs, RC_STR("a switch needs an integer, not "), type,
            RC_STR(""), arena);
    }
    return rc_wgsl_type_concretise(&c->types, type, arena);
}


// Check one case selector of a switch: a constant of type, unique among those before it.
static void rc_wgsl_check_case_value(rc_wgsl_checker *c, uint32_t selector, uint32_t type,
    rc_array_wgsl_index *seen, rc_arena *arena)
{
    rc_wgsl_sema sema;

    rc_wgsl_check_convert(c, selector, type, arena);
    sema = rc_wgsl_check_sema(c, selector);
    if (rc_wgsl_check_failed(c)) {
        return;
    }
    if (!(sema.flags & RC_WGSL_SEMA_VALUE)) {
        rc_wgsl_check_fail(c, selector, RC_STR("a case must be known before the shader runs"));
        return;
    }
    for (uint32_t i = 0; i < seen->num; i++) {
        if (rc_wgsl_check_sema(c, rc_array_wgsl_index_get(seen, i)).value == sema.value) {
            rc_wgsl_check_fail(c, selector, RC_STR("an earlier case has this value"));
            return;
        }
    }
    rc_array_wgsl_index_push(seen, selector, arena);
}


// Check every case selector of a switch, which needs exactly one default.
static void rc_wgsl_check_case_values(rc_wgsl_checker *c, uint32_t node, uint32_t type,
    rc_arena *arena)
{
    rc_view_wgsl_index clauses = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, node).rhs);
    rc_array_wgsl_index seen = rc_array_wgsl_index_make(16, arena);
    uint32_t defaults = 0;

    rc_wgsl_check_convert(c, rc_wgsl_ast_node(c->ast, node).lhs, type, arena);
    for (uint32_t i = 0; i < clauses.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t clause = rc_view_wgsl_index_get(clauses, i);
        rc_view_wgsl_index selectors = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, clause).lhs);

        for (uint32_t j = 0; j < selectors.num && !rc_wgsl_check_failed(c); j++) {
            uint32_t selector = rc_view_wgsl_index_get(selectors, j);

            if (rc_wgsl_ast_type(c->ast, selector) == RC_WGSL_NODE_TYPE_DEFAULT) {
                defaults++;
            }
            else {
                rc_wgsl_check_case_value(c, selector, type, &seen, arena);
            }
        }
    }
    if (!rc_wgsl_check_failed(c) && defaults != 1) {
        rc_wgsl_check_fail(c, node, defaults == 0 ? RC_STR("a switch needs a default case")
                                                  : RC_STR("a switch has only one default case"));
    }
}


// A switch and its clauses.
static bool rc_wgsl_check_switch(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_view_wgsl_index clauses = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, node).rhs);
    uint32_t type = rc_wgsl_check_switch_type(c, node, arena);
    rc_wgsl_breakable saved;
    bool falls = false;

    rc_wgsl_check_case_values(c, node, type, arena);
    saved = rc_wgsl_enter_breakable(c, false);
    for (uint32_t i = 0; i < clauses.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t clause = rc_view_wgsl_index_get(clauses, i);

        falls = rc_wgsl_check_block(c, rc_wgsl_ast_node(c->ast, clause).rhs, arena) || falls;
    }
    return rc_wgsl_leave_breakable(c, saved, false) || falls;
}


// A loop's continuing block, which sees the body's names, and its break-if. Returns whether it
// has a break-if, which can end the loop.
static bool rc_wgsl_check_continuing(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_scope scope = rc_wgsl_enter_scope(c);
    bool in_continuing = c->in_continuing;

    c->in_continuing = true;
    rc_wgsl_check_statements(c, rc_wgsl_ast_node(c->ast, n.lhs).lhs, arena);
    if (n.rhs != 0) {
        rc_wgsl_check_condition(c, n.rhs, arena);
    }
    c->in_continuing = in_continuing;
    rc_wgsl_leave_scope(c, scope);
    return n.rhs != 0;
}


// A loop, which carries on past only by breaking.
static bool rc_wgsl_check_loop(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_breakable saved = rc_wgsl_enter_breakable(c, true);
    rc_wgsl_scope scope = rc_wgsl_enter_scope(c);
    bool breaks_if = false;

    rc_wgsl_check_statements(c, rc_wgsl_ast_node(c->ast, n.lhs).lhs, arena);
    if (n.rhs != 0) {
        breaks_if = rc_wgsl_check_continuing(c, n.rhs, arena);
    }
    rc_wgsl_leave_scope(c, scope);
    return rc_wgsl_leave_breakable(c, saved, true) || breaks_if;
}


// A statement that can start or update a for loop: a declaration, assignment, step or call.
static void rc_wgsl_check_for_part(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    switch (rc_wgsl_ast_type(c->ast, node)) {
    case RC_WGSL_NODE_TYPE_NONE:
        break;
    case RC_WGSL_NODE_TYPE_VAR:
    case RC_WGSL_NODE_TYPE_LET:
    case RC_WGSL_NODE_TYPE_CONST:
        rc_wgsl_check_local_decl(c, node, arena);
        break;
    default:
        rc_wgsl_check_statement(c, node, arena);
        break;
    }
}


// A for loop, which carries on past whenever its condition can end it.
static bool rc_wgsl_check_for(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_for_parts parts = rc_wgsl_ast_for_parts(c->ast, node);
    rc_wgsl_scope scope = rc_wgsl_enter_scope(c);
    rc_wgsl_breakable saved;
    bool broke;

    rc_wgsl_check_for_part(c, parts.init, arena);
    if (parts.condition != 0) {
        rc_wgsl_check_condition(c, parts.condition, arena);
    }
    saved = rc_wgsl_enter_breakable(c, true);
    rc_wgsl_check_for_part(c, parts.update, arena);
    rc_wgsl_check_block(c, rc_wgsl_ast_node(c->ast, node).rhs, arena);
    broke = rc_wgsl_leave_breakable(c, saved, true);
    rc_wgsl_leave_scope(c, scope);
    return parts.condition != 0 || broke;
}


// A while loop.
static bool rc_wgsl_check_while(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
    rc_wgsl_breakable saved;

    rc_wgsl_check_condition(c, n.lhs, arena);
    saved = rc_wgsl_enter_breakable(c, true);
    rc_wgsl_check_block(c, n.rhs, arena);
    rc_wgsl_leave_breakable(c, saved, true);
    return true;
}

// ---- statements ----

// One statement, returning whether control can carry on past it.
static bool rc_wgsl_check_statement(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    switch (rc_wgsl_ast_type(c->ast, node)) {
    case RC_WGSL_NODE_TYPE_BLOCK:
        return rc_wgsl_check_block(c, node, arena);
    case RC_WGSL_NODE_TYPE_VAR:
    case RC_WGSL_NODE_TYPE_LET:
    case RC_WGSL_NODE_TYPE_CONST:
        rc_wgsl_check_local_decl(c, node, arena);
        return true;
    case RC_WGSL_NODE_TYPE_CONST_ASSERT:
        rc_wgsl_check_declaration(c, node, arena);
        return true;
    case RC_WGSL_NODE_TYPE_RETURN:
        return rc_wgsl_check_return(c, node, arena);
    case RC_WGSL_NODE_TYPE_IF:
        return rc_wgsl_check_if(c, node, arena);
    case RC_WGSL_NODE_TYPE_SWITCH:
        return rc_wgsl_check_switch(c, node, arena);
    case RC_WGSL_NODE_TYPE_LOOP:
        return rc_wgsl_check_loop(c, node, arena);
    case RC_WGSL_NODE_TYPE_FOR:
        return rc_wgsl_check_for(c, node, arena);
    case RC_WGSL_NODE_TYPE_WHILE:
        return rc_wgsl_check_while(c, node, arena);
    case RC_WGSL_NODE_TYPE_BREAK:
        return rc_wgsl_check_break(c, node);
    case RC_WGSL_NODE_TYPE_CONTINUE:
        return rc_wgsl_check_continue(c, node);
    case RC_WGSL_NODE_TYPE_DISCARD:
        rc_wgsl_check_note_fragment_only(c, node);
        return false;
    case RC_WGSL_NODE_TYPE_ASSIGN:
        rc_wgsl_check_assign(c, node, arena);
        return true;
    case RC_WGSL_NODE_TYPE_INCREMENT:
    case RC_WGSL_NODE_TYPE_DECREMENT:
        rc_wgsl_check_step(c, node, arena);
        return true;
    case RC_WGSL_NODE_TYPE_CALL:
        rc_wgsl_check_call_statement(c, node, arena);
        return true;
    default:
        rc_wgsl_check_fail(c, node, RC_STR("this can't be a statement"));
        return true;
    }
}


void rc_wgsl_check_body(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_fn_parts parts = rc_wgsl_ast_fn_parts(c->ast, node);
    rc_view_wgsl_index params = rc_wgsl_ast_list(c->ast, parts.params);
    bool falls;

    // The parameters share the scope of the body's own declarations
    c->function = node;
    c->scope = 0;
    for (uint32_t i = 0; i < params.num; i++) {
        rc_wgsl_check_declare_local(c, rc_view_wgsl_index_get(params, i), arena);
    }
    falls = rc_wgsl_check_statements(c, rc_wgsl_ast_node(c->ast, parts.body).lhs, arena);
    if (falls && rc_wgsl_check_sema(c, node).type != 0) {
        rc_wgsl_check_fail(c, node, RC_STR("this function can end without returning a value"));
    }
    c->function = 0;
    c->locals.num = 0;
}
