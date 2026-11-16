// parser.c
//
// Recursive descent over the tokens, one function per piece of grammar. Each returns the index of
// the node it made, or 0 once there is an error, after which everything unwinds without adding
// more.
//
// List items are gathered on a stack while their children are parsed, then copied into extra in
// one run when the list ends, so lists nested inside lists stay contiguous.

#include "parser.h"

#include "error.h"

enum {
    // How deeply expressions, blocks and else-if chains may nest
    RC_WGSL_MAX_DEPTH = 64
};

typedef struct rc_wgsl_parser {
    rc_str source;
    rc_view_wgsl_token tokens;
    uint32_t next;
    uint32_t depth;
    rc_array_wgsl_node nodes;
    rc_array_wgsl_index extra;
    rc_array_wgsl_index stack;
    rc_wgsl_error error;
} rc_wgsl_parser;

static uint32_t rc_wgsl_parse_expression(rc_wgsl_parser *p, rc_arena *arena);
static uint32_t rc_wgsl_parse_statement(rc_wgsl_parser *p, rc_arena *arena);
static uint32_t rc_wgsl_parse_block(rc_wgsl_parser *p, rc_arena *arena);

// ---- tokens ----

// The type of the token ahead places past the next. The last token is END, which we never pass.
static rc_wgsl_token_type rc_wgsl_peek_at(const rc_wgsl_parser *p, uint32_t ahead)
{
    uint32_t i = p->next + ahead;

    if (i >= p->tokens.num) {
        i = p->tokens.num - 1;
    }
    return (rc_wgsl_token_type)rc_view_wgsl_token_get(p->tokens, i).type;
}


// The type of the next token.
static rc_wgsl_token_type rc_wgsl_peek(const rc_wgsl_parser *p)
{
    return rc_wgsl_peek_at(p, 0);
}


// True once there is an error.
static bool rc_wgsl_failed(const rc_wgsl_parser *p)
{
    return rc_wgsl_error_is_set(p->error);
}


// Record message as the error at the next token, unless there is one already. Returns 0, the
// node that failed parse functions give.
static uint32_t rc_wgsl_fail(rc_wgsl_parser *p, rc_str message)
{
    if (!rc_wgsl_failed(p)) {
        uint32_t offset = rc_view_wgsl_token_get(p->tokens, p->next).offset;

        p->error = rc_wgsl_error_at(p->source, offset, message);
    }
    return 0;
}


// Step past the next token, returning its index.
static uint32_t rc_wgsl_advance(rc_wgsl_parser *p)
{
    uint32_t i = p->next;

    if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_END) {
        p->next++;
    }
    return i;
}


// Step past the next token if it is of type, saying whether it was.
static bool rc_wgsl_accept(rc_wgsl_parser *p, rc_wgsl_token_type type)
{
    if (rc_wgsl_peek(p) == type) {
        rc_wgsl_advance(p);
        return true;
    }
    return false;
}


// Step past the next token, which must be of type, returning its index. Otherwise fail with
// message.
static uint32_t rc_wgsl_expect(rc_wgsl_parser *p, rc_wgsl_token_type type, rc_str message)
{
    if (rc_wgsl_peek(p) == type) {
        return rc_wgsl_advance(p);
    }
    rc_wgsl_fail(p, message);
    return p->next;
}


// Go one level deeper, failing if that is too deep.
static bool rc_wgsl_enter(rc_wgsl_parser *p)
{
    if (++p->depth > RC_WGSL_MAX_DEPTH) {
        rc_wgsl_fail(p, RC_STR("this is nested too deeply"));
        return false;
    }
    return true;
}


// Come back up a level.
static void rc_wgsl_leave(rc_wgsl_parser *p)
{
    p->depth--;
}

// ---- nodes, lists and records ----

// A new node, or 0 once there is an error.
static uint32_t rc_wgsl_add_node(rc_wgsl_parser *p, rc_wgsl_node_type type, uint32_t token,
    uint32_t lhs, uint32_t rhs, rc_arena *arena)
{
    rc_wgsl_node node = {
        .token = token,
        .lhs = lhs,
        .rhs = rhs,
        .type = (uint8_t)type
    };

    if (rc_wgsl_failed(p)) {
        return 0;
    }
    return rc_array_wgsl_node_push(&p->nodes, node, arena);
}


// The token that names node index.
static uint32_t rc_wgsl_token_of(const rc_wgsl_parser *p, uint32_t index)
{
    return rc_array_wgsl_node_get(&p->nodes, index).token;
}


// Where a list starts on the stack.
static uint32_t rc_wgsl_list_begin(const rc_wgsl_parser *p)
{
    return p->stack.num;
}


// Add node to the list being gathered.
static void rc_wgsl_list_push(rc_wgsl_parser *p, uint32_t node, rc_arena *arena)
{
    rc_array_wgsl_index_push(&p->stack, node, arena);
}


// Move the list gathered since mark into extra, returning its index; 0 for an empty list.
static uint32_t rc_wgsl_list_end(rc_wgsl_parser *p, uint32_t mark, rc_arena *arena)
{
    uint32_t num = p->stack.num - mark;
    uint32_t list;

    if (num == 0 || rc_wgsl_failed(p)) {
        rc_array_wgsl_index_pop_n(&p->stack, num);
        return 0;
    }
    list = rc_array_wgsl_index_push(&p->extra, num, arena);
    rc_array_wgsl_index_append(&p->extra, rc_view_wgsl_index_drop_first(p->stack.view, mark),
        arena);
    rc_array_wgsl_index_pop_n(&p->stack, num);
    return list;
}


// A record of the num fields given, in extra, returning its index.
static uint32_t rc_wgsl_add_record(rc_wgsl_parser *p, const uint32_t *fields, uint32_t num,
    rc_arena *arena)
{
    uint32_t record = p->extra.num;

    if (rc_wgsl_failed(p)) {
        return 0;
    }
    rc_array_wgsl_index_append(&p->extra, rc_view_wgsl_index_from_ptr(fields, num), arena);
    return record;
}

// ---- expressions ----

// The arguments in parentheses after a call or an attribute's name, as a list.
static uint32_t rc_wgsl_parse_arguments(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t mark = rc_wgsl_list_begin(p);

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_LEFT_PAREN, RC_STR("expected '('"));
    while (!rc_wgsl_failed(p) && rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_RIGHT_PAREN) {
        rc_wgsl_list_push(p, rc_wgsl_parse_expression(p, arena), arena);
        if (!rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COMMA)) {
            break;
        }
    }
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_PAREN, RC_STR("expected ',' or ')'"));
    return rc_wgsl_list_end(p, mark, arena);
}


// A template list, which the lexer has already found, as a list.
static uint32_t rc_wgsl_parse_template_list(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t mark = rc_wgsl_list_begin(p);

    rc_wgsl_advance(p);
    do {
        rc_wgsl_list_push(p, rc_wgsl_parse_expression(p, arena), arena);
    } while (rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COMMA)
        && rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_TEMPLATE_END);
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_TEMPLATE_END, RC_STR("expected ',' or '>'"));
    return rc_wgsl_list_end(p, mark, arena);
}


// A name with its template list, if it has one, as an IDENT. Fails with message if the next token
// is not a name.
static uint32_t rc_wgsl_parse_ident(rc_wgsl_parser *p, rc_str message, rc_arena *arena)
{
    uint32_t name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, message);
    uint32_t templates = 0;

    if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_TEMPLATE_START) {
        templates = rc_wgsl_parse_template_list(p, arena);
    }
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_IDENT, name, templates, 0, arena);
}


// A type, which is written as a name with a template list.
static uint32_t rc_wgsl_parse_type(rc_wgsl_parser *p, rc_arena *arena)
{
    return rc_wgsl_parse_ident(p, RC_STR("expected a type"), arena);
}


// A name, or a call if arguments follow it.
static uint32_t rc_wgsl_parse_ident_or_call(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t ident = rc_wgsl_parse_ident(p, RC_STR("expected a name"), arena);
    uint32_t args;

    if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_LEFT_PAREN) {
        return ident;
    }
    args = rc_wgsl_parse_arguments(p, arena);
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_CALL, rc_wgsl_token_of(p, ident), ident, args, arena);
}


// An expression in parentheses, which leaves no node of its own.
static uint32_t rc_wgsl_parse_paren(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t inner;

    rc_wgsl_advance(p);
    inner = rc_wgsl_parse_expression(p, arena);
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_PAREN, RC_STR("expected ')'"));
    return inner;
}


// A literal of node type, its token taken whole.
static uint32_t rc_wgsl_parse_literal(rc_wgsl_parser *p, rc_wgsl_node_type type, rc_arena *arena)
{
    return rc_wgsl_add_node(p, type, rc_wgsl_advance(p), 0, 0, arena);
}


// A name, call, literal or parenthesised expression.
static uint32_t rc_wgsl_parse_primary(rc_wgsl_parser *p, rc_arena *arena)
{
    switch (rc_wgsl_peek(p)) {
    case RC_WGSL_TOKEN_TYPE_IDENT:
        return rc_wgsl_parse_ident_or_call(p, arena);
    case RC_WGSL_TOKEN_TYPE_INT:
        return rc_wgsl_parse_literal(p, RC_WGSL_NODE_TYPE_INT, arena);
    case RC_WGSL_TOKEN_TYPE_FLOAT:
        return rc_wgsl_parse_literal(p, RC_WGSL_NODE_TYPE_FLOAT, arena);
    case RC_WGSL_TOKEN_TYPE_TRUE:
    case RC_WGSL_TOKEN_TYPE_FALSE:
        return rc_wgsl_parse_literal(p, RC_WGSL_NODE_TYPE_BOOL, arena);
    case RC_WGSL_TOKEN_TYPE_LEFT_PAREN:
        return rc_wgsl_parse_paren(p, arena);
    default:
        return rc_wgsl_fail(p, RC_STR("expected an expression"));
    }
}


// The indexing and member accesses after base.
static uint32_t rc_wgsl_parse_postfix(rc_wgsl_parser *p, uint32_t base, rc_arena *arena)
{
    while (!rc_wgsl_failed(p)) {
        if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_LEFT_BRACKET) {
            uint32_t bracket = rc_wgsl_advance(p);
            uint32_t index = rc_wgsl_parse_expression(p, arena);

            rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACKET, RC_STR("expected ']'"));
            base = rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_INDEX, bracket, base, index, arena);
        }
        else if (rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_PERIOD)) {
            uint32_t name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT,
                RC_STR("expected a member name after '.'"));

            base = rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_MEMBER, name, base, 0, arena);
        }
        else {
            break;
        }
    }
    return base;
}


// True for an operator that can come before its operand.
static bool rc_wgsl_is_unary_operator(rc_wgsl_token_type type)
{
    return type == RC_WGSL_TOKEN_TYPE_MINUS || type == RC_WGSL_TOKEN_TYPE_BANG
        || type == RC_WGSL_TOKEN_TYPE_TILDE || type == RC_WGSL_TOKEN_TYPE_STAR
        || type == RC_WGSL_TOKEN_TYPE_AND;
}


// An operand: prefix operators, then a primary expression with its indexing and members.
static uint32_t rc_wgsl_parse_unary(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t op;
    uint32_t operand;

    if (!rc_wgsl_is_unary_operator(rc_wgsl_peek(p))) {
        return rc_wgsl_parse_postfix(p, rc_wgsl_parse_primary(p, arena), arena);
    }
    if (!rc_wgsl_enter(p)) {
        return 0;
    }
    op = rc_wgsl_advance(p);
    operand = rc_wgsl_parse_unary(p, arena);
    rc_wgsl_leave(p);
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_UNARY, op, operand, 0, arena);
}


// The binary operator token op, applied to left and right.
static uint32_t rc_wgsl_add_binary(rc_wgsl_parser *p, uint32_t op, uint32_t left, uint32_t right,
    rc_arena *arena)
{
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_BINARY, op, left, right, arena);
}


// True for *, / and %.
static bool rc_wgsl_is_multiplicative(rc_wgsl_token_type type)
{
    return type == RC_WGSL_TOKEN_TYPE_STAR || type == RC_WGSL_TOKEN_TYPE_SLASH
        || type == RC_WGSL_TOKEN_TYPE_PERCENT;
}


// True for the comparisons.
static bool rc_wgsl_is_relational(rc_wgsl_token_type type)
{
    return type == RC_WGSL_TOKEN_TYPE_LESS || type == RC_WGSL_TOKEN_TYPE_LESS_EQUAL
        || type == RC_WGSL_TOKEN_TYPE_GREATER || type == RC_WGSL_TOKEN_TYPE_GREATER_EQUAL
        || type == RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL || type == RC_WGSL_TOKEN_TYPE_BANG_EQUAL;
}


// True for &, | and ^.
static bool rc_wgsl_is_bitwise(rc_wgsl_token_type type)
{
    return type == RC_WGSL_TOKEN_TYPE_AND || type == RC_WGSL_TOKEN_TYPE_OR
        || type == RC_WGSL_TOKEN_TYPE_XOR;
}


// True for any operator between two operands.
static bool rc_wgsl_is_binary_operator(rc_wgsl_token_type type)
{
    return rc_wgsl_is_multiplicative(type) || rc_wgsl_is_relational(type)
        || rc_wgsl_is_bitwise(type) || type == RC_WGSL_TOKEN_TYPE_PLUS
        || type == RC_WGSL_TOKEN_TYPE_MINUS
        || type == RC_WGSL_TOKEN_TYPE_SHIFT_LEFT || type == RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT
        || type == RC_WGSL_TOKEN_TYPE_AND_AND || type == RC_WGSL_TOKEN_TYPE_OR_OR;
}


// Products, from the operand already parsed.
static uint32_t rc_wgsl_parse_multiplicative_from(rc_wgsl_parser *p, uint32_t left,
    rc_arena *arena)
{
    while (rc_wgsl_is_multiplicative(rc_wgsl_peek(p))) {
        uint32_t op = rc_wgsl_advance(p);

        left = rc_wgsl_add_binary(p, op, left, rc_wgsl_parse_unary(p, arena), arena);
    }
    return left;
}


// Sums of products, from the first operand.
static uint32_t rc_wgsl_parse_additive_from(rc_wgsl_parser *p, uint32_t operand, rc_arena *arena)
{
    uint32_t left = rc_wgsl_parse_multiplicative_from(p, operand, arena);

    while (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_PLUS || rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_MINUS) {
        uint32_t op = rc_wgsl_advance(p);
        uint32_t right = rc_wgsl_parse_multiplicative_from(p, rc_wgsl_parse_unary(p, arena), arena);

        left = rc_wgsl_add_binary(p, op, left, right, arena);
    }
    return left;
}


// One shift of two operands, or a sum, from the first operand.
static uint32_t rc_wgsl_parse_shift_from(rc_wgsl_parser *p, uint32_t operand, rc_arena *arena)
{
    rc_wgsl_token_type type = rc_wgsl_peek(p);

    if (type == RC_WGSL_TOKEN_TYPE_SHIFT_LEFT || type == RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT) {
        uint32_t op = rc_wgsl_advance(p);

        return rc_wgsl_add_binary(p, op, operand, rc_wgsl_parse_unary(p, arena), arena);
    }
    return rc_wgsl_parse_additive_from(p, operand, arena);
}


// One comparison of two shifts, or a shift, from the first operand.
static uint32_t rc_wgsl_parse_relational_from(rc_wgsl_parser *p, uint32_t operand,
    rc_arena *arena)
{
    uint32_t left = rc_wgsl_parse_shift_from(p, operand, arena);

    if (rc_wgsl_is_relational(rc_wgsl_peek(p))) {
        uint32_t op = rc_wgsl_advance(p);
        uint32_t right = rc_wgsl_parse_shift_from(p, rc_wgsl_parse_unary(p, arena), arena);

        return rc_wgsl_add_binary(p, op, left, right, arena);
    }
    return left;
}


// A run of one bitwise operator over operands, from the first.
static uint32_t rc_wgsl_parse_bitwise_from(rc_wgsl_parser *p, uint32_t left, rc_arena *arena)
{
    rc_wgsl_token_type type = rc_wgsl_peek(p);

    while (rc_wgsl_peek(p) == type) {
        uint32_t op = rc_wgsl_advance(p);

        left = rc_wgsl_add_binary(p, op, left, rc_wgsl_parse_unary(p, arena), arena);
    }
    return left;
}


// A run of one of && and || over comparisons, from the first.
static uint32_t rc_wgsl_parse_logical_from(rc_wgsl_parser *p, uint32_t left, rc_arena *arena)
{
    rc_wgsl_token_type type = rc_wgsl_peek(p);

    while (rc_wgsl_peek(p) == type) {
        uint32_t op = rc_wgsl_advance(p);
        uint32_t right = rc_wgsl_parse_relational_from(p, rc_wgsl_parse_unary(p, arena), arena);

        left = rc_wgsl_add_binary(p, op, left, right, arena);
    }
    return left;
}


// An expression: a comparison, or a run of && or || over them, or a run of a bitwise operator.
static uint32_t rc_wgsl_parse_expression(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t operand;
    uint32_t result;

    if (!rc_wgsl_enter(p)) {
        return 0;
    }
    operand = rc_wgsl_parse_unary(p, arena);
    if (rc_wgsl_is_bitwise(rc_wgsl_peek(p))) {
        result = rc_wgsl_parse_bitwise_from(p, operand, arena);
    }
    else {
        result = rc_wgsl_parse_relational_from(p, operand, arena);
        if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_AND_AND
            || rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_OR_OR) {
            result = rc_wgsl_parse_logical_from(p, result, arena);
        }
    }
    rc_wgsl_leave(p);

    // Anything else binary here is a mix that WGSL leaves to parentheses
    if (rc_wgsl_is_binary_operator(rc_wgsl_peek(p))) {
        return rc_wgsl_fail(p, RC_STR("WGSL needs parentheses to mix these operators"));
    }
    return result;
}

// ---- attributes ----

// One attribute, as an ATTRIBUTE.
static uint32_t rc_wgsl_parse_attribute(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t name;
    uint32_t args = 0;

    // diagnostic is a keyword, but also the name of an attribute
    rc_wgsl_advance(p);
    if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_DIAGNOSTIC) {
        name = rc_wgsl_advance(p);
    }
    else {
        name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected an attribute's name"));
    }
    if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_LEFT_PAREN) {
        args = rc_wgsl_parse_arguments(p, arena);
    }
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_ATTRIBUTE, name, args, 0, arena);
}


// Any attributes ahead, as a list.
static uint32_t rc_wgsl_parse_attributes(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t mark = rc_wgsl_list_begin(p);

    while (!rc_wgsl_failed(p) && rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_AT) {
        rc_wgsl_list_push(p, rc_wgsl_parse_attribute(p, arena), arena);
    }
    return rc_wgsl_list_end(p, mark, arena);
}


// Any @diagnostic attributes ahead, which are dropped. Other attributes fail.
static void rc_wgsl_skip_diagnostics(rc_wgsl_parser *p, rc_arena *arena)
{
    while (!rc_wgsl_failed(p) && rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_AT) {
        uint32_t attribute = rc_wgsl_parse_attribute(p, arena);
        rc_wgsl_token name = rc_view_wgsl_token_get(p->tokens, rc_wgsl_token_of(p, attribute));

        if (!rc_wgsl_failed(p)
            && !rc_str_is_equal(rc_wgsl_token_text(p->source, name), RC_STR("diagnostic"))) {
            p->error = rc_wgsl_error_at(p->source, name.offset,
                RC_STR("only @diagnostic can go on a statement"));
        }
    }
}

// ---- statements ----

// The statements up to the '}' that closes a block, as a list. Stops early at stop, if it is not
// END, to let a loop find its continuing block.
static uint32_t rc_wgsl_parse_statements(rc_wgsl_parser *p, rc_wgsl_token_type stop,
    rc_arena *arena)
{
    uint32_t mark = rc_wgsl_list_begin(p);

    while (!rc_wgsl_failed(p) && rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_RIGHT_BRACE
        && rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_END && rc_wgsl_peek(p) != stop) {
        uint32_t statement = rc_wgsl_parse_statement(p, arena);

        // An empty statement leaves nothing
        if (statement != 0) {
            rc_wgsl_list_push(p, statement, arena);
        }
    }
    return rc_wgsl_list_end(p, mark, arena);
}


// A block in braces, after any @diagnostic attributes.
static uint32_t rc_wgsl_parse_block(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t brace;
    uint32_t statements;

    if (!rc_wgsl_enter(p)) {
        return 0;
    }
    rc_wgsl_skip_diagnostics(p, arena);
    brace = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_LEFT_BRACE, RC_STR("expected '{'"));
    statements = rc_wgsl_parse_statements(p, RC_WGSL_TOKEN_TYPE_END, arena);
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACE, RC_STR("expected '}'"));
    rc_wgsl_leave(p);
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_BLOCK, brace, statements, 0, arena);
}


// A var, with the attributes already parsed before it.
static uint32_t rc_wgsl_parse_var(rc_wgsl_parser *p, uint32_t attributes, rc_arena *arena)
{
    uint32_t templates = 0;
    uint32_t name;
    uint32_t type = 0;
    uint32_t initialiser = 0;
    uint32_t parts[4];

    rc_wgsl_advance(p);
    if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_TEMPLATE_START) {
        templates = rc_wgsl_parse_template_list(p, arena);
    }
    name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected the variable's name"));
    if (rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COLON)) {
        type = rc_wgsl_parse_type(p, arena);
    }
    if (rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_EQUAL)) {
        initialiser = rc_wgsl_parse_expression(p, arena);
    }
    parts[0] = attributes;
    parts[1] = templates;
    parts[2] = type;
    parts[3] = initialiser;
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_VAR, name, rc_wgsl_add_record(p, parts, 4, arena), 0,
        arena);
}


// A let or const, as node type.
static uint32_t rc_wgsl_parse_value_declaration(rc_wgsl_parser *p, rc_wgsl_node_type type,
    rc_arena *arena)
{
    uint32_t name;
    uint32_t declared = 0;
    uint32_t initialiser;

    rc_wgsl_advance(p);
    name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected a name"));
    if (rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COLON)) {
        declared = rc_wgsl_parse_type(p, arena);
    }
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_EQUAL, RC_STR("expected '=' and a value"));
    initialiser = rc_wgsl_parse_expression(p, arena);
    return rc_wgsl_add_node(p, type, name, declared, initialiser, arena);
}


// A var, let or const in a function.
static uint32_t rc_wgsl_parse_local_declaration(rc_wgsl_parser *p, rc_arena *arena)
{
    switch (rc_wgsl_peek(p)) {
    case RC_WGSL_TOKEN_TYPE_VAR:
        return rc_wgsl_parse_var(p, 0, arena);
    case RC_WGSL_TOKEN_TYPE_LET:
        return rc_wgsl_parse_value_declaration(p, RC_WGSL_NODE_TYPE_LET, arena);
    default:
        return rc_wgsl_parse_value_declaration(p, RC_WGSL_NODE_TYPE_CONST, arena);
    }
}


// A const_assert and its condition.
static uint32_t rc_wgsl_parse_const_assert(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t condition = rc_wgsl_parse_expression(p, arena);

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_CONST_ASSERT, keyword, condition, 0, arena);
}


// True for = and the compound assignments.
static bool rc_wgsl_is_assignment_operator(rc_wgsl_token_type type)
{
    switch (type) {
    case RC_WGSL_TOKEN_TYPE_EQUAL:
    case RC_WGSL_TOKEN_TYPE_PLUS_EQUAL:
    case RC_WGSL_TOKEN_TYPE_MINUS_EQUAL:
    case RC_WGSL_TOKEN_TYPE_STAR_EQUAL:
    case RC_WGSL_TOKEN_TYPE_SLASH_EQUAL:
    case RC_WGSL_TOKEN_TYPE_PERCENT_EQUAL:
    case RC_WGSL_TOKEN_TYPE_AND_EQUAL:
    case RC_WGSL_TOKEN_TYPE_OR_EQUAL:
    case RC_WGSL_TOKEN_TYPE_XOR_EQUAL:
    case RC_WGSL_TOKEN_TYPE_SHIFT_LEFT_EQUAL:
    case RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT_EQUAL:
        return true;
    default:
        return false;
    }
}


// The assignment, increment or decrement of target, already parsed.
static uint32_t rc_wgsl_parse_update(rc_wgsl_parser *p, uint32_t target, rc_arena *arena)
{
    rc_wgsl_token_type type = rc_wgsl_peek(p);
    uint32_t op;

    if (type == RC_WGSL_TOKEN_TYPE_PLUS_PLUS || type == RC_WGSL_TOKEN_TYPE_MINUS_MINUS) {
        op = rc_wgsl_advance(p);
        return rc_wgsl_add_node(p,
            type == RC_WGSL_TOKEN_TYPE_PLUS_PLUS ? RC_WGSL_NODE_TYPE_INCREMENT
                                                 : RC_WGSL_NODE_TYPE_DECREMENT,
            op, target, 0, arena);
    }
    if (!rc_wgsl_is_assignment_operator(type)) {
        return rc_wgsl_fail(p, RC_STR("expected an assignment, or a call"));
    }
    op = rc_wgsl_advance(p);
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_ASSIGN, op, target, rc_wgsl_parse_expression(p, arena),
        arena);
}


// A call, assignment, increment or decrement: the statements a for loop can update with.
static uint32_t rc_wgsl_parse_simple_statement(rc_wgsl_parser *p, rc_arena *arena)
{
    rc_wgsl_token_type after = rc_wgsl_peek_at(p, 1);

    if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_UNDERSCORE) {
        uint32_t phony = rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_PHONY, rc_wgsl_advance(p), 0, 0, arena);

        if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_EQUAL) {
            return rc_wgsl_fail(p, RC_STR("expected '=' after '_'"));
        }
        return rc_wgsl_parse_update(p, phony, arena);
    }
    if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_IDENT
        && (after == RC_WGSL_TOKEN_TYPE_LEFT_PAREN || after == RC_WGSL_TOKEN_TYPE_TEMPLATE_START)) {
        return rc_wgsl_parse_ident_or_call(p, arena);
    }
    return rc_wgsl_parse_update(p, rc_wgsl_parse_unary(p, arena), arena);
}


// A statement that needs nothing but its keyword: break, continue or discard.
static uint32_t rc_wgsl_parse_keyword_statement(rc_wgsl_parser *p, rc_wgsl_node_type type,
    rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);

    if (type == RC_WGSL_NODE_TYPE_BREAK && rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_IF) {
        return rc_wgsl_fail(p, RC_STR("'break if' can only end a continuing block"));
    }
    return rc_wgsl_add_node(p, type, keyword, 0, 0, arena);
}


// A return, with its value if it has one.
static uint32_t rc_wgsl_parse_return(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t value = 0;

    if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_SEMICOLON) {
        value = rc_wgsl_parse_expression(p, arena);
    }
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_RETURN, keyword, value, 0, arena);
}


// An if, with its else-ifs and else.
static uint32_t rc_wgsl_parse_if(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t condition = rc_wgsl_parse_expression(p, arena);
    uint32_t parts[2] = {rc_wgsl_parse_block(p, arena), 0};

    if (rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_ELSE)) {
        if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_IF) {
            parts[1] = rc_wgsl_parse_block(p, arena);
        }
        else if (rc_wgsl_enter(p)) {
            parts[1] = rc_wgsl_parse_if(p, arena);
            rc_wgsl_leave(p);
        }
    }
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_IF, keyword, condition,
        rc_wgsl_add_record(p, parts, 2, arena), arena);
}


// One case or default clause of a switch.
static uint32_t rc_wgsl_parse_case(rc_wgsl_parser *p, rc_arena *arena)
{
    bool is_default = rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_DEFAULT;
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t mark = rc_wgsl_list_begin(p);
    uint32_t selectors;

    if (is_default) {
        rc_wgsl_list_push(p, rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_DEFAULT, keyword, 0, 0, arena),
            arena);
    }
    while (!is_default && !rc_wgsl_failed(p)) {
        if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_DEFAULT) {
            uint32_t token = rc_wgsl_advance(p);

            rc_wgsl_list_push(p, rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_DEFAULT, token, 0, 0, arena),
                arena);
        }
        else {
            rc_wgsl_list_push(p, rc_wgsl_parse_expression(p, arena), arena);
        }

        // A trailing comma may come before the ':' or the body
        if (!rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COMMA) || rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_COLON
            || rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_LEFT_BRACE
            || rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_AT) {
            break;
        }
    }
    selectors = rc_wgsl_list_end(p, mark, arena);
    rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COLON);
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_CASE, keyword, selectors,
        rc_wgsl_parse_block(p, arena), arena);
}


// A switch and its clauses.
static uint32_t rc_wgsl_parse_switch(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t selector = rc_wgsl_parse_expression(p, arena);
    uint32_t mark;

    rc_wgsl_skip_diagnostics(p, arena);
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_LEFT_BRACE, RC_STR("expected '{'"));
    mark = rc_wgsl_list_begin(p);
    while (!rc_wgsl_failed(p) && (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_CASE
        || rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_DEFAULT)) {
        rc_wgsl_list_push(p, rc_wgsl_parse_case(p, arena), arena);
    }
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACE, RC_STR("expected 'case', 'default' or '}'"));
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_SWITCH, keyword, selector,
        rc_wgsl_list_end(p, mark, arena), arena);
}


// The 'break if' that ends a continuing block, and the '}' after it, returning its condition.
static uint32_t rc_wgsl_parse_break_if(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t condition;

    if (rc_wgsl_peek_at(p, 1) != RC_WGSL_TOKEN_TYPE_IF) {
        return rc_wgsl_fail(p, RC_STR("a continuing block can't break, except by 'break if'"));
    }
    rc_wgsl_advance(p);
    rc_wgsl_advance(p);
    condition = rc_wgsl_parse_expression(p, arena);
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_SEMICOLON, RC_STR("expected ';'"));
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACE,
        RC_STR("'break if' must be the last statement in a continuing block"));
    return condition;
}


// A loop's continuing block, ending with its break-if if it has one.
static uint32_t rc_wgsl_parse_continuing(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t brace;
    uint32_t statements;
    uint32_t break_if = 0;

    rc_wgsl_skip_diagnostics(p, arena);
    brace = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_LEFT_BRACE, RC_STR("expected '{'"));
    statements = rc_wgsl_parse_statements(p, RC_WGSL_TOKEN_TYPE_BREAK, arena);
    if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_BREAK) {
        break_if = rc_wgsl_parse_break_if(p, arena);
    }
    else {
        rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACE, RC_STR("expected '}'"));
    }
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_CONTINUING, keyword,
        rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_BLOCK, brace, statements, 0, arena), break_if, arena);
}


// A loop, with its continuing block if it has one.
static uint32_t rc_wgsl_parse_loop(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t brace;
    uint32_t statements;
    uint32_t continuing = 0;
    uint32_t body;

    rc_wgsl_skip_diagnostics(p, arena);
    brace = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_LEFT_BRACE, RC_STR("expected '{'"));
    statements = rc_wgsl_parse_statements(p, RC_WGSL_TOKEN_TYPE_CONTINUING, arena);
    body = rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_BLOCK, brace, statements, 0, arena);
    if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_CONTINUING) {
        continuing = rc_wgsl_parse_continuing(p, arena);
        rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACE,
            RC_STR("the continuing block must come last in a loop"));
    }
    else {
        rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACE, RC_STR("expected '}'"));
    }
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_LOOP, keyword, body, continuing, arena);
}


// A for loop.
static uint32_t rc_wgsl_parse_for(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t parts[3] = {0};
    uint32_t header;

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_LEFT_PAREN, RC_STR("expected '('"));
    if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_VAR || rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_LET
        || rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_CONST) {
        parts[0] = rc_wgsl_parse_local_declaration(p, arena);
    }
    else if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_SEMICOLON) {
        parts[0] = rc_wgsl_parse_simple_statement(p, arena);
    }
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_SEMICOLON, RC_STR("expected ';'"));
    if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_SEMICOLON) {
        parts[1] = rc_wgsl_parse_expression(p, arena);
    }
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_SEMICOLON, RC_STR("expected ';'"));
    if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_RIGHT_PAREN) {
        parts[2] = rc_wgsl_parse_simple_statement(p, arena);
    }
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_PAREN, RC_STR("expected ')'"));
    header = rc_wgsl_add_record(p, parts, 3, arena);
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_FOR, keyword, header, rc_wgsl_parse_block(p, arena),
        arena);
}


// A while loop.
static uint32_t rc_wgsl_parse_while(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t condition = rc_wgsl_parse_expression(p, arena);

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_WHILE, keyword, condition,
        rc_wgsl_parse_block(p, arena), arena);
}


// A statement after @diagnostic attributes, which must be one that takes them.
static uint32_t rc_wgsl_parse_attributed_statement(rc_wgsl_parser *p, rc_arena *arena)
{
    rc_wgsl_skip_diagnostics(p, arena);
    switch (rc_wgsl_peek(p)) {
    case RC_WGSL_TOKEN_TYPE_LEFT_BRACE:
        return rc_wgsl_parse_block(p, arena);
    case RC_WGSL_TOKEN_TYPE_IF:
        return rc_wgsl_parse_if(p, arena);
    case RC_WGSL_TOKEN_TYPE_SWITCH:
        return rc_wgsl_parse_switch(p, arena);
    case RC_WGSL_TOKEN_TYPE_LOOP:
        return rc_wgsl_parse_loop(p, arena);
    case RC_WGSL_TOKEN_TYPE_FOR:
        return rc_wgsl_parse_for(p, arena);
    case RC_WGSL_TOKEN_TYPE_WHILE:
        return rc_wgsl_parse_while(p, arena);
    default:
        return rc_wgsl_fail(p, RC_STR("attributes can't go on this statement"));
    }
}


// A statement that ends with a ';', and the ';'.
static uint32_t rc_wgsl_parse_terminated_statement(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t statement;

    switch (rc_wgsl_peek(p)) {
    case RC_WGSL_TOKEN_TYPE_RETURN:
        statement = rc_wgsl_parse_return(p, arena);
        break;
    case RC_WGSL_TOKEN_TYPE_BREAK:
        statement = rc_wgsl_parse_keyword_statement(p, RC_WGSL_NODE_TYPE_BREAK, arena);
        break;
    case RC_WGSL_TOKEN_TYPE_CONTINUE:
        statement = rc_wgsl_parse_keyword_statement(p, RC_WGSL_NODE_TYPE_CONTINUE, arena);
        break;
    case RC_WGSL_TOKEN_TYPE_DISCARD:
        statement = rc_wgsl_parse_keyword_statement(p, RC_WGSL_NODE_TYPE_DISCARD, arena);
        break;
    case RC_WGSL_TOKEN_TYPE_VAR:
    case RC_WGSL_TOKEN_TYPE_LET:
    case RC_WGSL_TOKEN_TYPE_CONST:
        statement = rc_wgsl_parse_local_declaration(p, arena);
        break;
    case RC_WGSL_TOKEN_TYPE_CONST_ASSERT:
        statement = rc_wgsl_parse_const_assert(p, arena);
        break;
    default:
        statement = rc_wgsl_parse_simple_statement(p, arena);
        break;
    }
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_SEMICOLON, RC_STR("expected ';'"));
    return statement;
}


// One statement, or 0 for an empty one.
static uint32_t rc_wgsl_parse_statement(rc_wgsl_parser *p, rc_arena *arena)
{
    switch (rc_wgsl_peek(p)) {
    case RC_WGSL_TOKEN_TYPE_SEMICOLON:
        rc_wgsl_advance(p);
        return 0;
    case RC_WGSL_TOKEN_TYPE_AT:
        return rc_wgsl_parse_attributed_statement(p, arena);
    case RC_WGSL_TOKEN_TYPE_LEFT_BRACE:
        return rc_wgsl_parse_block(p, arena);
    case RC_WGSL_TOKEN_TYPE_IF:
        return rc_wgsl_parse_if(p, arena);
    case RC_WGSL_TOKEN_TYPE_SWITCH:
        return rc_wgsl_parse_switch(p, arena);
    case RC_WGSL_TOKEN_TYPE_LOOP:
        return rc_wgsl_parse_loop(p, arena);
    case RC_WGSL_TOKEN_TYPE_FOR:
        return rc_wgsl_parse_for(p, arena);
    case RC_WGSL_TOKEN_TYPE_WHILE:
        return rc_wgsl_parse_while(p, arena);
    case RC_WGSL_TOKEN_TYPE_CONTINUING:
        return rc_wgsl_fail(p, RC_STR("a continuing block must come last in a loop"));
    default:
        return rc_wgsl_parse_terminated_statement(p, arena);
    }
}

// ---- declarations ----

// A struct field.
static uint32_t rc_wgsl_parse_field(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t attributes = rc_wgsl_parse_attributes(p, arena);
    uint32_t name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected a member's name"));

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_COLON, RC_STR("expected ':' and the member's type"));
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_FIELD, name, attributes, rc_wgsl_parse_type(p, arena),
        arena);
}


// A struct and its fields.
static uint32_t rc_wgsl_parse_struct(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t name;
    uint32_t mark;
    uint32_t fields;

    rc_wgsl_advance(p);
    name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected the struct's name"));
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_LEFT_BRACE, RC_STR("expected '{'"));
    mark = rc_wgsl_list_begin(p);
    while (!rc_wgsl_failed(p) && rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_RIGHT_BRACE) {
        rc_wgsl_list_push(p, rc_wgsl_parse_field(p, arena), arena);
        if (!rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COMMA)) {
            break;
        }
    }
    if (p->stack.num == mark) {
        rc_wgsl_fail(p, RC_STR("a struct needs at least one member"));
    }
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACE, RC_STR("expected ',' or '}'"));
    fields = rc_wgsl_list_end(p, mark, arena);
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_STRUCT, name, fields, 0, arena);
}


// A function parameter.
static uint32_t rc_wgsl_parse_param(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t attributes = rc_wgsl_parse_attributes(p, arena);
    uint32_t name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected a parameter's name"));

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_COLON, RC_STR("expected ':' and the parameter's type"));
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_PARAM, name, attributes, rc_wgsl_parse_type(p, arena),
        arena);
}


// A function's parameters in parentheses, as a list.
static uint32_t rc_wgsl_parse_params(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t mark = rc_wgsl_list_begin(p);

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_LEFT_PAREN, RC_STR("expected '('"));
    while (!rc_wgsl_failed(p) && rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_RIGHT_PAREN) {
        rc_wgsl_list_push(p, rc_wgsl_parse_param(p, arena), arena);
        if (!rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COMMA)) {
            break;
        }
    }
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_PAREN, RC_STR("expected ',' or ')'"));
    return rc_wgsl_list_end(p, mark, arena);
}


// A function, with the attributes already parsed before it.
static uint32_t rc_wgsl_parse_fn(rc_wgsl_parser *p, uint32_t attributes, rc_arena *arena)
{
    uint32_t name;
    uint32_t parts[5] = {attributes, 0, 0, 0, 0};

    rc_wgsl_advance(p);
    name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected the function's name"));
    parts[1] = rc_wgsl_parse_params(p, arena);
    if (rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_ARROW)) {
        parts[2] = rc_wgsl_parse_attributes(p, arena);
        parts[3] = rc_wgsl_parse_type(p, arena);
    }
    parts[4] = rc_wgsl_parse_block(p, arena);
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_FN, name, rc_wgsl_add_record(p, parts, 5, arena), 0,
        arena);
}


// A type alias.
static uint32_t rc_wgsl_parse_alias(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t name;

    rc_wgsl_advance(p);
    name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected the alias's name"));
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_EQUAL, RC_STR("expected '=' and a type"));
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_ALIAS, name, rc_wgsl_parse_type(p, arena), 0, arena);
}


// Fail if attributes, parsed before the next token, have nothing there to apply to.
static void rc_wgsl_refuse_attributes(rc_wgsl_parser *p, uint32_t attributes)
{
    if (attributes != 0) {
        rc_wgsl_fail(p, RC_STR("attributes can't go on this declaration"));
    }
}


// A declaration that ends with a ';', after its attributes, and the ';'.
static uint32_t rc_wgsl_parse_terminated_declaration(rc_wgsl_parser *p, uint32_t attributes,
    rc_arena *arena)
{
    uint32_t declaration;

    switch (rc_wgsl_peek(p)) {
    case RC_WGSL_TOKEN_TYPE_VAR:
        declaration = rc_wgsl_parse_var(p, attributes, arena);
        break;
    case RC_WGSL_TOKEN_TYPE_CONST:
        rc_wgsl_refuse_attributes(p, attributes);
        declaration = rc_wgsl_parse_value_declaration(p, RC_WGSL_NODE_TYPE_CONST, arena);
        break;
    case RC_WGSL_TOKEN_TYPE_ALIAS:
        rc_wgsl_refuse_attributes(p, attributes);
        declaration = rc_wgsl_parse_alias(p, arena);
        break;
    case RC_WGSL_TOKEN_TYPE_CONST_ASSERT:
        rc_wgsl_refuse_attributes(p, attributes);
        declaration = rc_wgsl_parse_const_assert(p, arena);
        break;
    case RC_WGSL_TOKEN_TYPE_OVERRIDE:
        return rc_wgsl_fail(p, RC_STR("override declarations aren't supported yet"));
    case RC_WGSL_TOKEN_TYPE_LET:
        return rc_wgsl_fail(p, RC_STR("'let' only goes in functions: use 'const' here"));
    default:
        return rc_wgsl_fail(p, RC_STR("expected a declaration"));
    }
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_SEMICOLON, RC_STR("expected ';'"));
    return declaration;
}


// A declaration at module scope, or 0 for a stray ';'.
static uint32_t rc_wgsl_parse_global_declaration(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t attributes;

    if (rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_SEMICOLON)) {
        return 0;
    }
    attributes = rc_wgsl_parse_attributes(p, arena);
    switch (rc_wgsl_peek(p)) {
    case RC_WGSL_TOKEN_TYPE_FN:
        return rc_wgsl_parse_fn(p, attributes, arena);
    case RC_WGSL_TOKEN_TYPE_STRUCT:
        rc_wgsl_refuse_attributes(p, attributes);
        return rc_wgsl_parse_struct(p, arena);
    default:
        return rc_wgsl_parse_terminated_declaration(p, attributes, arena);
    }
}


// The directives at the top of the module. Diagnostics are checked and dropped; the rest are
// refused.
static void rc_wgsl_parse_directives(rc_wgsl_parser *p, rc_arena *arena)
{
    while (!rc_wgsl_failed(p)) {
        switch (rc_wgsl_peek(p)) {
        case RC_WGSL_TOKEN_TYPE_ENABLE:
            rc_wgsl_fail(p, RC_STR("enable directives aren't supported yet"));
            break;
        case RC_WGSL_TOKEN_TYPE_REQUIRES:
            rc_wgsl_fail(p, RC_STR("requires directives aren't supported yet"));
            break;
        case RC_WGSL_TOKEN_TYPE_DIAGNOSTIC:
            rc_wgsl_advance(p);
            rc_wgsl_parse_arguments(p, arena);
            rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_SEMICOLON, RC_STR("expected ';'"));
            break;
        default:
            return;
        }
    }
}


rc_wgsl_parse_result rc_wgsl_parse(rc_str source, rc_view_wgsl_token tokens, rc_arena *arena)
{
    rc_wgsl_parser p = {
        .source = source,
        .tokens = tokens,
        .nodes = rc_array_wgsl_node_make(tokens.num + 1, arena),
        .extra = rc_array_wgsl_index_make(tokens.num + 1, arena),
        .stack = rc_array_wgsl_index_make(64, arena)
    };
    uint32_t mark;
    uint32_t decls;

    // Node 0 is none, and extra[0] the empty list
    RC_ASSERT(tokens.num > 0);
    rc_array_wgsl_node_push(&p.nodes, (rc_wgsl_node) {0}, arena);
    rc_array_wgsl_index_push(&p.extra, 0, arena);

    rc_wgsl_parse_directives(&p, arena);
    mark = rc_wgsl_list_begin(&p);
    while (!rc_wgsl_failed(&p) && rc_wgsl_peek(&p) != RC_WGSL_TOKEN_TYPE_END) {
        uint32_t declaration = rc_wgsl_parse_global_declaration(&p, arena);

        if (declaration != 0) {
            rc_wgsl_list_push(&p, declaration, arena);
        }
    }
    decls = rc_wgsl_list_end(&p, mark, arena);
    if (rc_wgsl_failed(&p)) {
        return (rc_wgsl_parse_result) {.error = p.error};
    }

    return (rc_wgsl_parse_result) {
        .ast = {
            .source = source,
            .tokens = tokens,
            .nodes = p.nodes.view,
            .extra = p.extra.view,
            .decls = decls
        }
    };
}
