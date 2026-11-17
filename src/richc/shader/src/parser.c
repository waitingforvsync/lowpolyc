// parser.c
//
// Recursive descent over the tokens, one function per piece of grammar. Each returns the index of
// the node it made, or 0 once there is an error. After that, everything unwinds and adds nothing.
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
static uint32_t rc_wgsl_parse_unary(rc_wgsl_parser *p, rc_arena *arena);
static uint32_t rc_wgsl_parse_statement(rc_wgsl_parser *p, rc_arena *arena);
static uint32_t rc_wgsl_parse_block(rc_wgsl_parser *p, rc_arena *arena);
static uint32_t rc_wgsl_parse_if(rc_wgsl_parser *p, rc_arena *arena);

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


// Record message as the error at the next token, unless there is one already. Returns 0, the node
// that failed parse functions give.
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


// Step past the next token, which must be of type, returning its index.
// Otherwise fail with message.
static uint32_t rc_wgsl_expect(rc_wgsl_parser *p, rc_wgsl_token_type type, rc_str message)
{
    if (rc_wgsl_peek(p) == type) {
        return rc_wgsl_advance(p);
    }

    rc_wgsl_fail(p, message);
    return p->next;
}


// Step past the next token, then expect one of type after it, as rc_wgsl_expect does.
static uint32_t rc_wgsl_expect_after(rc_wgsl_parser *p, rc_wgsl_token_type type, rc_str message)
{
    rc_wgsl_advance(p);
    return rc_wgsl_expect(p, type, message);
}


// Go one level deeper, failing if that is too deep. Each step of a chain of operators or of
// indexing and members counts as a level too, until its expression ends, since the tree it makes
// is as deep as it is long.
static bool rc_wgsl_enter(rc_wgsl_parser *p)
{
    if (++p->depth > RC_WGSL_MAX_DEPTH) {
        rc_wgsl_fail(p, RC_STR("this is nested too deeply, or too long a chain"));
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
static uint32_t rc_wgsl_add_node(rc_wgsl_parser *p, rc_wgsl_node_type type, uint32_t token, uint32_t lhs, uint32_t rhs, rc_arena *arena)
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


// Copy the list gathered since mark into extra, after its length, returning its index.
static uint32_t rc_wgsl_list_copy(rc_wgsl_parser *p, uint32_t mark, rc_arena *arena)
{
    uint32_t list = rc_array_wgsl_index_push(&p->extra, p->stack.num - mark, arena);

    rc_array_wgsl_index_append(&p->extra, rc_view_wgsl_index_drop_first(p->stack.view, mark), arena);
    return list;
}


// Move the list gathered since mark into extra, returning its index; 0 for an empty list.
static uint32_t rc_wgsl_list_end(rc_wgsl_parser *p, uint32_t mark, rc_arena *arena)
{
    uint32_t num = p->stack.num - mark;
    uint32_t list = num == 0 || rc_wgsl_failed(p) ? 0 : rc_wgsl_list_copy(p, mark, arena);

    rc_array_wgsl_index_pop_n(&p->stack, num);
    return list;
}


// A record of the num fields given, in extra, returning its index.
static uint32_t rc_wgsl_add_record(rc_wgsl_parser *p, const uint32_t *fields, uint32_t num, rc_arena *arena)
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


// The template list ahead, which the lexer has already found, as a list; 0 if there is none.
static uint32_t rc_wgsl_parse_template_list(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t mark = rc_wgsl_list_begin(p);

    if (!rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_TEMPLATE_START)) {
        return 0;
    }

    do {
        rc_wgsl_list_push(p, rc_wgsl_parse_expression(p, arena), arena);
    } while (rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COMMA) && rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_TEMPLATE_END);
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_TEMPLATE_END, RC_STR("expected ',' or '>'"));
    return rc_wgsl_list_end(p, mark, arena);
}


// A name with its template list, if it has one, as an IDENT. Fails with message if the next token
// is not a name.
static uint32_t rc_wgsl_parse_ident(rc_wgsl_parser *p, rc_str message, rc_arena *arena)
{
    uint32_t name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, message);
    uint32_t templates = rc_wgsl_parse_template_list(p, arena);

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

    if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_LEFT_PAREN) {
        return ident;
    }

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_CALL, rc_wgsl_token_of(p, ident), ident, rc_wgsl_parse_arguments(p, arena), arena);
}


// Step past the next token, then parse the expression after it.
static uint32_t rc_wgsl_parse_expression_after(rc_wgsl_parser *p, rc_arena *arena)
{
    rc_wgsl_advance(p);
    return rc_wgsl_parse_expression(p, arena);
}


// An expression in parentheses, which leaves no node of its own.
static uint32_t rc_wgsl_parse_paren(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t inner = rc_wgsl_parse_expression_after(p, arena);

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
        if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_LEFT_BRACKET && rc_wgsl_enter(p)) {
            uint32_t bracket = rc_wgsl_advance(p);
            uint32_t index = rc_wgsl_parse_expression(p, arena);

            rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACKET, RC_STR("expected ']'"));
            base = rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_INDEX, bracket, base, index, arena);
        }
        else if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_PERIOD && rc_wgsl_enter(p)) {
            uint32_t name = rc_wgsl_expect_after(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected a member name after '.'"));

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
    switch (type) {
    case RC_WGSL_TOKEN_TYPE_MINUS:
    case RC_WGSL_TOKEN_TYPE_BANG:
    case RC_WGSL_TOKEN_TYPE_TILDE:
    case RC_WGSL_TOKEN_TYPE_STAR:
    case RC_WGSL_TOKEN_TYPE_AND:
        return true;
    default:
        return false;
    }
}


// A prefix operator and its operand, inside the level the operator entered, which this leaves.
static uint32_t rc_wgsl_parse_prefixed(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t op = rc_wgsl_advance(p);
    uint32_t operand = rc_wgsl_parse_unary(p, arena);

    rc_wgsl_leave(p);
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_UNARY, op, operand, 0, arena);
}


// An operand: prefix operators, then a primary expression with its indexing and members.
static uint32_t rc_wgsl_parse_unary(rc_wgsl_parser *p, rc_arena *arena)
{
    if (!rc_wgsl_is_unary_operator(rc_wgsl_peek(p))) {
        return rc_wgsl_parse_postfix(p, rc_wgsl_parse_primary(p, arena), arena);
    }

    if (!rc_wgsl_enter(p)) {
        return 0;
    }

    return rc_wgsl_parse_prefixed(p, arena);
}


// The binary operator token op, applied to left and right: one more step of a chain.
static uint32_t rc_wgsl_add_binary(rc_wgsl_parser *p, uint32_t op, uint32_t left, uint32_t right, rc_arena *arena)
{
    if (!rc_wgsl_enter(p)) {
        return 0;
    }

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_BINARY, op, left, right, arena);
}


// True for *, / and %.
static bool rc_wgsl_is_multiplicative(rc_wgsl_token_type type)
{
    switch (type) {
    case RC_WGSL_TOKEN_TYPE_STAR:
    case RC_WGSL_TOKEN_TYPE_SLASH:
    case RC_WGSL_TOKEN_TYPE_PERCENT:
        return true;
    default:
        return false;
    }
}


// True for + and -.
static bool rc_wgsl_is_additive(rc_wgsl_token_type type)
{
    return type == RC_WGSL_TOKEN_TYPE_PLUS || type == RC_WGSL_TOKEN_TYPE_MINUS;
}


// True for << and >>.
static bool rc_wgsl_is_shift(rc_wgsl_token_type type)
{
    return type == RC_WGSL_TOKEN_TYPE_SHIFT_LEFT || type == RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT;
}


// True for the comparisons.
static bool rc_wgsl_is_relational(rc_wgsl_token_type type)
{
    switch (type) {
    case RC_WGSL_TOKEN_TYPE_LESS:
    case RC_WGSL_TOKEN_TYPE_LESS_EQUAL:
    case RC_WGSL_TOKEN_TYPE_GREATER:
    case RC_WGSL_TOKEN_TYPE_GREATER_EQUAL:
    case RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL:
    case RC_WGSL_TOKEN_TYPE_BANG_EQUAL:
        return true;
    default:
        return false;
    }
}


// True for &, | and ^.
static bool rc_wgsl_is_bitwise(rc_wgsl_token_type type)
{
    switch (type) {
    case RC_WGSL_TOKEN_TYPE_AND:
    case RC_WGSL_TOKEN_TYPE_OR:
    case RC_WGSL_TOKEN_TYPE_XOR:
        return true;
    default:
        return false;
    }
}


// True for && and ||.
static bool rc_wgsl_is_logical(rc_wgsl_token_type type)
{
    return type == RC_WGSL_TOKEN_TYPE_AND_AND || type == RC_WGSL_TOKEN_TYPE_OR_OR;
}


// True for any operator between two operands.
static bool rc_wgsl_is_binary_operator(rc_wgsl_token_type type)
{
    if (rc_wgsl_is_multiplicative(type) || rc_wgsl_is_additive(type)) {
        return true;
    }

    if (rc_wgsl_is_shift(type) || rc_wgsl_is_relational(type)) {
        return true;
    }

    return rc_wgsl_is_bitwise(type) || rc_wgsl_is_logical(type);
}


// Products, from the operand already parsed.
static uint32_t rc_wgsl_parse_multiplicative_from(rc_wgsl_parser *p, uint32_t left, rc_arena *arena)
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

    while (rc_wgsl_is_additive(rc_wgsl_peek(p))) {
        uint32_t op = rc_wgsl_advance(p);
        uint32_t right = rc_wgsl_parse_multiplicative_from(p, rc_wgsl_parse_unary(p, arena), arena);

        left = rc_wgsl_add_binary(p, op, left, right, arena);
    }
    return left;
}


// One shift of two operands, or a sum, from the first operand.
static uint32_t rc_wgsl_parse_shift_from(rc_wgsl_parser *p, uint32_t operand, rc_arena *arena)
{
    if (rc_wgsl_is_shift(rc_wgsl_peek(p))) {
        uint32_t op = rc_wgsl_advance(p);

        return rc_wgsl_add_binary(p, op, operand, rc_wgsl_parse_unary(p, arena), arena);
    }

    return rc_wgsl_parse_additive_from(p, operand, arena);
}


// One comparison of two shifts, or a shift, from the first operand.
static uint32_t rc_wgsl_parse_relational_from(rc_wgsl_parser *p, uint32_t operand, rc_arena *arena)
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


// A comparison from the first operand, then a run of && or || over more if one follows.
static uint32_t rc_wgsl_parse_logical_from(rc_wgsl_parser *p, uint32_t operand, rc_arena *arena)
{
    uint32_t left = rc_wgsl_parse_relational_from(p, operand, arena);
    rc_wgsl_token_type type = rc_wgsl_peek(p);

    if (!rc_wgsl_is_logical(type)) {
        return left;
    }

    while (rc_wgsl_peek(p) == type) {
        uint32_t op = rc_wgsl_advance(p);
        uint32_t right = rc_wgsl_parse_relational_from(p, rc_wgsl_parse_unary(p, arena), arena);

        left = rc_wgsl_add_binary(p, op, left, right, arena);
    }
    return left;
}


// The operands and operators of an expression, before the check that they don't mix.
static uint32_t rc_wgsl_parse_operators(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t operand = rc_wgsl_parse_unary(p, arena);

    if (rc_wgsl_is_bitwise(rc_wgsl_peek(p))) {
        return rc_wgsl_parse_bitwise_from(p, operand, arena);
    }

    return rc_wgsl_parse_logical_from(p, operand, arena);
}


// Close the expression that gave result, back at depth, the level outside it.
static uint32_t rc_wgsl_end_expression(rc_wgsl_parser *p, uint32_t depth, uint32_t result)
{
    p->depth = depth;

    // Anything else binary here is a mix that WGSL leaves to parentheses
    if (rc_wgsl_is_binary_operator(rc_wgsl_peek(p))) {
        return rc_wgsl_fail(p, RC_STR("WGSL needs parentheses to mix these operators"));
    }

    return result;
}


// An expression: a comparison, or a run of && or || over them, or a run of a bitwise operator.
static uint32_t rc_wgsl_parse_expression(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t depth = p->depth;

    if (!rc_wgsl_enter(p)) {
        return 0;
    }

    return rc_wgsl_end_expression(p, depth, rc_wgsl_parse_operators(p, arena));
}

// ---- attributes ----

// Step past the '@' and the attribute's name after it, returning the name. diagnostic is a keyword,
// but also the name of an attribute.
static uint32_t rc_wgsl_parse_attribute_name(rc_wgsl_parser *p)
{
    rc_wgsl_advance(p);
    if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_DIAGNOSTIC) {
        return rc_wgsl_advance(p);
    }

    return rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected an attribute's name"));
}


// One attribute, as an ATTRIBUTE.
static uint32_t rc_wgsl_parse_attribute(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t name = rc_wgsl_parse_attribute_name(p);
    uint32_t args = rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_LEFT_PAREN ? rc_wgsl_parse_arguments(p, arena) : 0;

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

        if (!rc_wgsl_failed(p) && !rc_str_is_equal(rc_wgsl_token_text(p->source, name), RC_STR("diagnostic"))) {
            p->error = rc_wgsl_error_at(p->source, name.offset, RC_STR("only @diagnostic can go on a statement"));
        }
    }
}

// ---- statements ----

// True while a block's statements go on: no error yet, and neither '}', END nor stop is next.
static bool rc_wgsl_more_statements(const rc_wgsl_parser *p, rc_wgsl_token_type stop)
{
    rc_wgsl_token_type type = rc_wgsl_peek(p);

    if (rc_wgsl_failed(p)) {
        return false;
    }

    if (type == RC_WGSL_TOKEN_TYPE_RIGHT_BRACE || type == RC_WGSL_TOKEN_TYPE_END) {
        return false;
    }

    return type != stop;
}


// The statements up to the '}' that closes a block, as a list. Stops early at stop, if it is not
// END, to let a loop find its continuing block.
static uint32_t rc_wgsl_parse_statements(rc_wgsl_parser *p, rc_wgsl_token_type stop, rc_arena *arena)
{
    uint32_t mark = rc_wgsl_list_begin(p);

    while (rc_wgsl_more_statements(p, stop)) {
        uint32_t statement = rc_wgsl_parse_statement(p, arena);

        // An empty statement leaves nothing
        if (statement != 0) {
            rc_wgsl_list_push(p, statement, arena);
        }
    }
    return rc_wgsl_list_end(p, mark, arena);
}


// The '{' that opens a block, after any @diagnostic attributes, returning its index.
static uint32_t rc_wgsl_expect_brace(rc_wgsl_parser *p, rc_arena *arena)
{
    rc_wgsl_skip_diagnostics(p, arena);
    return rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_LEFT_BRACE, RC_STR("expected '{'"));
}


// A block in braces, inside the level it entered, which this leaves.
static uint32_t rc_wgsl_parse_entered_block(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t brace = rc_wgsl_expect_brace(p, arena);
    uint32_t statements = rc_wgsl_parse_statements(p, RC_WGSL_TOKEN_TYPE_END, arena);

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACE, RC_STR("expected '}'"));
    rc_wgsl_leave(p);
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_BLOCK, brace, statements, 0, arena);
}


// A block in braces, after any @diagnostic attributes.
static uint32_t rc_wgsl_parse_block(rc_wgsl_parser *p, rc_arena *arena)
{
    if (!rc_wgsl_enter(p)) {
        return 0;
    }

    return rc_wgsl_parse_entered_block(p, arena);
}


// A var after its keyword, with the attributes already parsed before it.
static uint32_t rc_wgsl_parse_var_after_keyword(rc_wgsl_parser *p, uint32_t attributes, rc_arena *arena)
{
    uint32_t templates = rc_wgsl_parse_template_list(p, arena);
    uint32_t name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected the variable's name"));
    uint32_t type = rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COLON) ? rc_wgsl_parse_type(p, arena) : 0;
    uint32_t initialiser = rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_EQUAL) ? rc_wgsl_parse_expression(p, arena) : 0;
    uint32_t record = rc_wgsl_add_record(p, (uint32_t[]) {attributes, templates, type, initialiser}, 4, arena);

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_VAR, name, record, 0, arena);
}


// A var, with the attributes already parsed before it.
static uint32_t rc_wgsl_parse_var(rc_wgsl_parser *p, uint32_t attributes, rc_arena *arena)
{
    rc_wgsl_advance(p);
    return rc_wgsl_parse_var_after_keyword(p, attributes, arena);
}


// A let or const, as node type.
static uint32_t rc_wgsl_parse_value_declaration(rc_wgsl_parser *p, rc_wgsl_node_type type, rc_arena *arena)
{
    uint32_t name = rc_wgsl_expect_after(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected a name"));
    uint32_t declared = rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COLON) ? rc_wgsl_parse_type(p, arena) : 0;

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_EQUAL, RC_STR("expected '=' and a value"));
    return rc_wgsl_add_node(p, type, name, declared, rc_wgsl_parse_expression(p, arena), arena);
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


// The increment or decrement of target, already parsed.
static uint32_t rc_wgsl_parse_increment(rc_wgsl_parser *p, uint32_t target, rc_arena *arena)
{
    bool increment = rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_PLUS_PLUS;
    uint32_t op = rc_wgsl_advance(p);
    rc_wgsl_node_type type = increment ? RC_WGSL_NODE_TYPE_INCREMENT : RC_WGSL_NODE_TYPE_DECREMENT;

    return rc_wgsl_add_node(p, type, op, target, 0, arena);
}


// The assignment of target, already parsed, by the operator ahead.
static uint32_t rc_wgsl_parse_assignment(rc_wgsl_parser *p, uint32_t target, rc_arena *arena)
{
    uint32_t op = rc_wgsl_advance(p);
    uint32_t value = rc_wgsl_parse_expression(p, arena);

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_ASSIGN, op, target, value, arena);
}


// The assignment, increment or decrement of target, already parsed.
static uint32_t rc_wgsl_parse_update(rc_wgsl_parser *p, uint32_t target, rc_arena *arena)
{
    rc_wgsl_token_type type = rc_wgsl_peek(p);

    if (type == RC_WGSL_TOKEN_TYPE_PLUS_PLUS || type == RC_WGSL_TOKEN_TYPE_MINUS_MINUS) {
        return rc_wgsl_parse_increment(p, target, arena);
    }

    if (!rc_wgsl_is_assignment_operator(type)) {
        return rc_wgsl_fail(p, RC_STR("expected an assignment, or a call"));
    }

    return rc_wgsl_parse_assignment(p, target, arena);
}


// An assignment to '_', which throws its value away.
static uint32_t rc_wgsl_parse_phony(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t phony = rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_PHONY, rc_wgsl_advance(p), 0, 0, arena);

    if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_EQUAL) {
        return rc_wgsl_fail(p, RC_STR("expected '=' after '_'"));
    }

    return rc_wgsl_parse_update(p, phony, arena);
}


// True if a call starts here: a name, then its arguments or its template list.
static bool rc_wgsl_is_call_ahead(const rc_wgsl_parser *p)
{
    rc_wgsl_token_type after = rc_wgsl_peek_at(p, 1);

    if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_IDENT) {
        return false;
    }

    return after == RC_WGSL_TOKEN_TYPE_LEFT_PAREN || after == RC_WGSL_TOKEN_TYPE_TEMPLATE_START;
}


// The assignment, increment or decrement of the operand ahead. The operand's chain of indexing and
// members ends with it, so its depth is given back.
static uint32_t rc_wgsl_parse_operand_update(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t depth = p->depth;
    uint32_t target = rc_wgsl_parse_unary(p, arena);

    p->depth = depth;
    return rc_wgsl_parse_update(p, target, arena);
}


// A call, assignment, increment or decrement: the statements a for loop can update with.
static uint32_t rc_wgsl_parse_simple_statement(rc_wgsl_parser *p, rc_arena *arena)
{
    if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_UNDERSCORE) {
        return rc_wgsl_parse_phony(p, arena);
    }

    if (rc_wgsl_is_call_ahead(p)) {
        return rc_wgsl_parse_ident_or_call(p, arena);
    }

    return rc_wgsl_parse_operand_update(p, arena);
}


// A statement that needs nothing but its keyword: break, continue or discard.
static uint32_t rc_wgsl_parse_keyword_statement(rc_wgsl_parser *p, rc_wgsl_node_type type, rc_arena *arena)
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
    uint32_t value = rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_SEMICOLON ? rc_wgsl_parse_expression(p, arena) : 0;

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_RETURN, keyword, value, 0, arena);
}


// An else-if, inside the level it entered, which this leaves.
static uint32_t rc_wgsl_parse_entered_else_if(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t other = rc_wgsl_parse_if(p, arena);

    rc_wgsl_leave(p);
    return other;
}


// The block or if after an else. Each else-if in a chain counts as a level deeper.
static uint32_t rc_wgsl_parse_else(rc_wgsl_parser *p, rc_arena *arena)
{
    if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_IF) {
        return rc_wgsl_parse_block(p, arena);
    }

    if (!rc_wgsl_enter(p)) {
        return 0;
    }

    return rc_wgsl_parse_entered_else_if(p, arena);
}


// An if, with its else-ifs and else.
static uint32_t rc_wgsl_parse_if(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t condition = rc_wgsl_parse_expression(p, arena);
    uint32_t then = rc_wgsl_parse_block(p, arena);
    uint32_t other = rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_ELSE) ? rc_wgsl_parse_else(p, arena) : 0;
    uint32_t record = rc_wgsl_add_record(p, (uint32_t[]) {then, other}, 2, arena);

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_IF, keyword, condition, record, arena);
}


// The selectors of a default clause, as a list: just default itself, at keyword.
static uint32_t rc_wgsl_default_selectors(rc_wgsl_parser *p, uint32_t keyword, rc_arena *arena)
{
    uint32_t mark = rc_wgsl_list_begin(p);

    rc_wgsl_list_push(p, rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_DEFAULT, keyword, 0, 0, arena), arena);
    return rc_wgsl_list_end(p, mark, arena);
}


// One selector of a case clause: an expression, or default.
static uint32_t rc_wgsl_parse_selector(rc_wgsl_parser *p, rc_arena *arena)
{
    if (rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_DEFAULT) {
        return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_DEFAULT, rc_wgsl_advance(p), 0, 0, arena);
    }

    return rc_wgsl_parse_expression(p, arena);
}


// Step past the comma after a selector, saying whether another selector follows it. A trailing
// comma may come before the ':' or the body.
static bool rc_wgsl_accept_selector_comma(rc_wgsl_parser *p)
{
    if (!rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COMMA)) {
        return false;
    }

    switch (rc_wgsl_peek(p)) {
    case RC_WGSL_TOKEN_TYPE_COLON:
    case RC_WGSL_TOKEN_TYPE_LEFT_BRACE:
    case RC_WGSL_TOKEN_TYPE_AT:
        return false;
    default:
        return true;
    }
}


// The selectors of a case clause, as a list.
static uint32_t rc_wgsl_parse_selectors(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t mark = rc_wgsl_list_begin(p);

    while (!rc_wgsl_failed(p)) {
        rc_wgsl_list_push(p, rc_wgsl_parse_selector(p, arena), arena);
        if (!rc_wgsl_accept_selector_comma(p)) {
            break;
        }
    }
    return rc_wgsl_list_end(p, mark, arena);
}


// One case or default clause of a switch.
static uint32_t rc_wgsl_parse_case(rc_wgsl_parser *p, rc_arena *arena)
{
    bool is_default = rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_DEFAULT;
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t selectors = is_default ? rc_wgsl_default_selectors(p, keyword, arena) : rc_wgsl_parse_selectors(p, arena);

    rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_COLON);
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_CASE, keyword, selectors, rc_wgsl_parse_block(p, arena), arena);
}


// True for the keywords that start a clause of a switch.
static bool rc_wgsl_is_clause_start(rc_wgsl_token_type type)
{
    return type == RC_WGSL_TOKEN_TYPE_CASE || type == RC_WGSL_TOKEN_TYPE_DEFAULT;
}


// The clauses of a switch in braces, after any @diagnostic attributes, as a list.
static uint32_t rc_wgsl_parse_clauses(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t mark = rc_wgsl_list_begin(p);

    rc_wgsl_expect_brace(p, arena);
    while (!rc_wgsl_failed(p) && rc_wgsl_is_clause_start(rc_wgsl_peek(p))) {
        rc_wgsl_list_push(p, rc_wgsl_parse_case(p, arena), arena);
    }
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACE, RC_STR("expected 'case', 'default' or '}'"));
    return rc_wgsl_list_end(p, mark, arena);
}


// A switch and its clauses.
static uint32_t rc_wgsl_parse_switch(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t selector = rc_wgsl_parse_expression(p, arena);
    uint32_t clauses = rc_wgsl_parse_clauses(p, arena);

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_SWITCH, keyword, selector, clauses, arena);
}


// The condition of a 'break if' after its 'if', then the ';' and the '}' after that.
static uint32_t rc_wgsl_parse_break_if(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t condition = rc_wgsl_parse_expression_after(p, arena);

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_SEMICOLON, RC_STR("expected ';'"));
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACE, RC_STR("'break if' must be the last statement in a continuing block"));
    return condition;
}


// The end of a continuing block: its '}', after a 'break if' if it has one. Returns the condition
// of the break-if, or 0.
static uint32_t rc_wgsl_parse_continuing_end(rc_wgsl_parser *p, rc_arena *arena)
{
    if (rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_BREAK) {
        rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACE, RC_STR("expected '}'"));
        return 0;
    }

    if (rc_wgsl_peek_at(p, 1) != RC_WGSL_TOKEN_TYPE_IF) {
        return rc_wgsl_fail(p, RC_STR("a continuing block can't break, except by 'break if'"));
    }

    rc_wgsl_advance(p);
    return rc_wgsl_parse_break_if(p, arena);
}


// A loop's continuing block, ending with its break-if if it has one.
static uint32_t rc_wgsl_parse_continuing(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t brace = rc_wgsl_expect_brace(p, arena);
    uint32_t statements = rc_wgsl_parse_statements(p, RC_WGSL_TOKEN_TYPE_BREAK, arena);
    uint32_t break_if = rc_wgsl_parse_continuing_end(p, arena);
    uint32_t block = rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_BLOCK, brace, statements, 0, arena);

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_CONTINUING, keyword, block, break_if, arena);
}


// A loop, with its continuing block if it has one.
static uint32_t rc_wgsl_parse_loop(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t brace = rc_wgsl_expect_brace(p, arena);
    uint32_t statements = rc_wgsl_parse_statements(p, RC_WGSL_TOKEN_TYPE_CONTINUING, arena);
    uint32_t body = rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_BLOCK, brace, statements, 0, arena);
    bool has_continuing = rc_wgsl_peek(p) == RC_WGSL_TOKEN_TYPE_CONTINUING;
    uint32_t continuing = has_continuing ? rc_wgsl_parse_continuing(p, arena) : 0;
    rc_str message = has_continuing ? RC_STR("the continuing block must come last in a loop") : RC_STR("expected '}'");

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_BRACE, message);
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_LOOP, keyword, body, continuing, arena);
}


// The '(' of a for loop, then its initialiser if it has one.
static uint32_t rc_wgsl_parse_for_init(rc_wgsl_parser *p, rc_arena *arena)
{
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_LEFT_PAREN, RC_STR("expected '('"));
    switch (rc_wgsl_peek(p)) {
    case RC_WGSL_TOKEN_TYPE_VAR:
    case RC_WGSL_TOKEN_TYPE_LET:
    case RC_WGSL_TOKEN_TYPE_CONST:
        return rc_wgsl_parse_local_declaration(p, arena);
    case RC_WGSL_TOKEN_TYPE_SEMICOLON:
        return 0;
    default:
        return rc_wgsl_parse_simple_statement(p, arena);
    }
}


// The ';' after a for loop's initialiser, then its condition if it has one.
static uint32_t rc_wgsl_parse_for_condition(rc_wgsl_parser *p, rc_arena *arena)
{
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_SEMICOLON, RC_STR("expected ';'"));
    return rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_SEMICOLON ? rc_wgsl_parse_expression(p, arena) : 0;
}


// The ';' after a for loop's condition, then its update if it has one.
static uint32_t rc_wgsl_parse_for_update(rc_wgsl_parser *p, rc_arena *arena)
{
    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_SEMICOLON, RC_STR("expected ';'"));
    return rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_RIGHT_PAREN ? rc_wgsl_parse_simple_statement(p, arena) : 0;
}


// The header of a for loop in parentheses, as a record of its parts.
static uint32_t rc_wgsl_parse_for_header(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t init = rc_wgsl_parse_for_init(p, arena);
    uint32_t condition = rc_wgsl_parse_for_condition(p, arena);
    uint32_t update = rc_wgsl_parse_for_update(p, arena);

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_RIGHT_PAREN, RC_STR("expected ')'"));
    return rc_wgsl_add_record(p, (uint32_t[]) {init, condition, update}, 3, arena);
}


// A for loop.
static uint32_t rc_wgsl_parse_for(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t header = rc_wgsl_parse_for_header(p, arena);

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_FOR, keyword, header, rc_wgsl_parse_block(p, arena), arena);
}


// A while loop.
static uint32_t rc_wgsl_parse_while(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t keyword = rc_wgsl_advance(p);
    uint32_t condition = rc_wgsl_parse_expression(p, arena);

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_WHILE, keyword, condition, rc_wgsl_parse_block(p, arena), arena);
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


// A statement that ends with a ';', without the ';'.
static uint32_t rc_wgsl_parse_unterminated_statement(rc_wgsl_parser *p, rc_arena *arena)
{
    switch (rc_wgsl_peek(p)) {
    case RC_WGSL_TOKEN_TYPE_RETURN:
        return rc_wgsl_parse_return(p, arena);
    case RC_WGSL_TOKEN_TYPE_BREAK:
        return rc_wgsl_parse_keyword_statement(p, RC_WGSL_NODE_TYPE_BREAK, arena);
    case RC_WGSL_TOKEN_TYPE_CONTINUE:
        return rc_wgsl_parse_keyword_statement(p, RC_WGSL_NODE_TYPE_CONTINUE, arena);
    case RC_WGSL_TOKEN_TYPE_DISCARD:
        return rc_wgsl_parse_keyword_statement(p, RC_WGSL_NODE_TYPE_DISCARD, arena);
    case RC_WGSL_TOKEN_TYPE_VAR:
    case RC_WGSL_TOKEN_TYPE_LET:
    case RC_WGSL_TOKEN_TYPE_CONST:
        return rc_wgsl_parse_local_declaration(p, arena);
    case RC_WGSL_TOKEN_TYPE_CONST_ASSERT:
        return rc_wgsl_parse_const_assert(p, arena);
    default:
        return rc_wgsl_parse_simple_statement(p, arena);
    }
}


// A statement that ends with a ';', and the ';'.
static uint32_t rc_wgsl_parse_terminated_statement(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t statement = rc_wgsl_parse_unterminated_statement(p, arena);

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
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_FIELD, name, attributes, rc_wgsl_parse_type(p, arena), arena);
}


// A struct's fields in braces, as a list. There must be at least one.
static uint32_t rc_wgsl_parse_fields(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t mark = rc_wgsl_list_begin(p);

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_LEFT_BRACE, RC_STR("expected '{'"));
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
    return rc_wgsl_list_end(p, mark, arena);
}


// A struct and its fields.
static uint32_t rc_wgsl_parse_struct(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t name = rc_wgsl_expect_after(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected the struct's name"));
    uint32_t fields = rc_wgsl_parse_fields(p, arena);

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_STRUCT, name, fields, 0, arena);
}


// A function parameter.
static uint32_t rc_wgsl_parse_param(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t attributes = rc_wgsl_parse_attributes(p, arena);
    uint32_t name = rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected a parameter's name"));

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_COLON, RC_STR("expected ':' and the parameter's type"));
    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_PARAM, name, attributes, rc_wgsl_parse_type(p, arena), arena);
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
    uint32_t name = rc_wgsl_expect_after(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected the function's name"));
    uint32_t params = rc_wgsl_parse_params(p, arena);
    bool arrow = rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_ARROW);
    uint32_t return_attributes = arrow ? rc_wgsl_parse_attributes(p, arena) : 0;
    uint32_t return_type = arrow ? rc_wgsl_parse_type(p, arena) : 0;
    uint32_t body = rc_wgsl_parse_block(p, arena);
    uint32_t record = rc_wgsl_add_record(p, (uint32_t[]) {attributes, params, return_attributes, return_type, body}, 5, arena);

    return rc_wgsl_add_node(p, RC_WGSL_NODE_TYPE_FN, name, record, 0, arena);
}


// A type alias.
static uint32_t rc_wgsl_parse_alias(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t name = rc_wgsl_expect_after(p, RC_WGSL_TOKEN_TYPE_IDENT, RC_STR("expected the alias's name"));

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


// A var, const, alias or const_assert after its attributes, without the ';' that ends it.
static uint32_t rc_wgsl_parse_unterminated_declaration(rc_wgsl_parser *p, uint32_t attributes, rc_arena *arena)
{
    switch (rc_wgsl_peek(p)) {
    case RC_WGSL_TOKEN_TYPE_VAR:
        return rc_wgsl_parse_var(p, attributes, arena);
    case RC_WGSL_TOKEN_TYPE_CONST:
        rc_wgsl_refuse_attributes(p, attributes);
        return rc_wgsl_parse_value_declaration(p, RC_WGSL_NODE_TYPE_CONST, arena);
    case RC_WGSL_TOKEN_TYPE_ALIAS:
        rc_wgsl_refuse_attributes(p, attributes);
        return rc_wgsl_parse_alias(p, arena);
    default:
        rc_wgsl_refuse_attributes(p, attributes);
        return rc_wgsl_parse_const_assert(p, arena);
    }
}


// A var, const, alias or const_assert after its attributes, and the ';' that ends it.
static uint32_t rc_wgsl_parse_terminated_declaration(rc_wgsl_parser *p, uint32_t attributes, rc_arena *arena)
{
    uint32_t declaration = rc_wgsl_parse_unterminated_declaration(p, attributes, arena);

    rc_wgsl_expect(p, RC_WGSL_TOKEN_TYPE_SEMICOLON, RC_STR("expected ';'"));
    return declaration;
}


// A declaration at module scope, after its attributes.
static uint32_t rc_wgsl_parse_attributed_declaration(rc_wgsl_parser *p, uint32_t attributes, rc_arena *arena)
{
    switch (rc_wgsl_peek(p)) {
    case RC_WGSL_TOKEN_TYPE_FN:
        return rc_wgsl_parse_fn(p, attributes, arena);
    case RC_WGSL_TOKEN_TYPE_STRUCT:
        rc_wgsl_refuse_attributes(p, attributes);
        return rc_wgsl_parse_struct(p, arena);
    case RC_WGSL_TOKEN_TYPE_VAR:
    case RC_WGSL_TOKEN_TYPE_CONST:
    case RC_WGSL_TOKEN_TYPE_ALIAS:
    case RC_WGSL_TOKEN_TYPE_CONST_ASSERT:
        return rc_wgsl_parse_terminated_declaration(p, attributes, arena);
    case RC_WGSL_TOKEN_TYPE_OVERRIDE:
        return rc_wgsl_fail(p, RC_STR("override declarations aren't supported yet"));
    case RC_WGSL_TOKEN_TYPE_LET:
        return rc_wgsl_fail(p, RC_STR("'let' only goes in functions: use 'const' here"));
    default:
        return rc_wgsl_fail(p, RC_STR("expected a declaration"));
    }
}


// A declaration at module scope, or 0 for a stray ';'.
static uint32_t rc_wgsl_parse_global_declaration(rc_wgsl_parser *p, rc_arena *arena)
{
    if (rc_wgsl_accept(p, RC_WGSL_TOKEN_TYPE_SEMICOLON)) {
        return 0;
    }

    return rc_wgsl_parse_attributed_declaration(p, rc_wgsl_parse_attributes(p, arena), arena);
}


// The directives at the top of the module. Diagnostics are checked and dropped.
// The rest are refused.
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


// The directives and declarations of the module, returning the declarations as a list. The
// directives leave nothing on the stack, so the list can start before them.
static uint32_t rc_wgsl_parse_module(rc_wgsl_parser *p, rc_arena *arena)
{
    uint32_t mark = rc_wgsl_list_begin(p);

    rc_wgsl_parse_directives(p, arena);
    while (!rc_wgsl_failed(p) && rc_wgsl_peek(p) != RC_WGSL_TOKEN_TYPE_END) {
        uint32_t declaration = rc_wgsl_parse_global_declaration(p, arena);

        if (declaration != 0) {
            rc_wgsl_list_push(p, declaration, arena);
        }
    }
    return rc_wgsl_list_end(p, mark, arena);
}


// A parser at the start of tokens. Node 0 is none, and extra[0] the empty list.
static rc_wgsl_parser rc_wgsl_parser_make(rc_str source, rc_view_wgsl_token tokens, rc_arena *arena)
{
    rc_wgsl_parser p = {
        .source = source,
        .tokens = tokens,
        .nodes = rc_array_wgsl_node_make(tokens.num + 1, arena),
        .extra = rc_array_wgsl_index_make(tokens.num + 1, arena),
        .stack = rc_array_wgsl_index_make(64, arena)
    };

    RC_ASSERT(tokens.num > 0);
    rc_array_wgsl_node_push(&p.nodes, (rc_wgsl_node) {0}, arena);
    rc_array_wgsl_index_push(&p.extra, 0, arena);
    return p;
}


rc_wgsl_parse_result rc_wgsl_parse(rc_str source, rc_view_wgsl_token tokens, rc_arena *arena)
{
    rc_wgsl_parser p = rc_wgsl_parser_make(source, tokens, arena);
    uint32_t decls = rc_wgsl_parse_module(&p, arena);

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
