// ast.h
//
// Private to richc_shader: the syntax tree the parser builds from WGSL tokens.
//
// Nodes live in one array and refer to each other by index. Node 0 is a placeholder meaning
// "none", so a zero field is always an absent part. Each node has a type, the token it is named
// by (its name, operator or keyword, for text and errors) and two fields, lhs and rhs, whose
// meaning depends on the type (below).
//
// Lists and records of more than two parts live in a second array of indices, extra. A list is a
// count followed by that many node indices, named by the index of its count; extra[0] is 0, so a
// zero list is the empty list. A record is a fixed run of fields read back through the
// rc_wgsl_ast_*_parts accessors.
//
// Types are written as expressions: an IDENT with a template list, as in array<vec3<f32>, 4>.
//
//   node            token          lhs                    rhs
//   IDENT           name           template args (list)
//   INT, FLOAT      literal
//   BOOL            true / false
//   CALL            callee's name  callee IDENT           args (list)
//   INDEX           [              base                   index
//   MEMBER          member name    base
//   UNARY           operator       operand
//   BINARY          operator       left                   right
//   ATTRIBUTE       name           args (list)
//   STRUCT          name           fields (list)
//   FIELD           name           attributes (list)      type
//   FN              name           rc_wgsl_fn_parts
//   PARAM           name           attributes (list)      type
//   VAR             name           rc_wgsl_var_parts
//   LET, CONST      name           type                   initialiser
//   ALIAS           name           type
//   CONST_ASSERT    const_assert   condition
//   BLOCK           {              statements (list)
//   RETURN          return         value
//   IF              if             condition              rc_wgsl_if_parts
//   SWITCH          switch         selector               clauses (list of CASE)
//   CASE            case/default   selectors (list)       body BLOCK
//   DEFAULT         default        (a selector)
//   LOOP            loop           body BLOCK             CONTINUING
//   CONTINUING      continuing     body BLOCK             break-if condition
//   FOR             for            rc_wgsl_for_parts      body BLOCK
//   WHILE           while          condition              body BLOCK
//   BREAK, CONTINUE, DISCARD
//   ASSIGN          operator       target (or PHONY)      value
//   PHONY           _
//   INCREMENT, DECREMENT  operator target
//
// A call statement is its CALL node, standing in a statement list.

#ifndef RC_SHADER_AST_H_
#define RC_SHADER_AST_H_

#include "lexer.h"

typedef enum rc_wgsl_node_type {
    RC_WGSL_NODE_TYPE_NONE = 0,

    // Expressions
    RC_WGSL_NODE_TYPE_IDENT,
    RC_WGSL_NODE_TYPE_INT,
    RC_WGSL_NODE_TYPE_FLOAT,
    RC_WGSL_NODE_TYPE_BOOL,
    RC_WGSL_NODE_TYPE_CALL,
    RC_WGSL_NODE_TYPE_INDEX,
    RC_WGSL_NODE_TYPE_MEMBER,
    RC_WGSL_NODE_TYPE_UNARY,
    RC_WGSL_NODE_TYPE_BINARY,

    // Declarations
    RC_WGSL_NODE_TYPE_ATTRIBUTE,
    RC_WGSL_NODE_TYPE_STRUCT,
    RC_WGSL_NODE_TYPE_FIELD,
    RC_WGSL_NODE_TYPE_FN,
    RC_WGSL_NODE_TYPE_PARAM,
    RC_WGSL_NODE_TYPE_VAR,
    RC_WGSL_NODE_TYPE_LET,
    RC_WGSL_NODE_TYPE_CONST,
    RC_WGSL_NODE_TYPE_ALIAS,
    RC_WGSL_NODE_TYPE_CONST_ASSERT,

    // Statements
    RC_WGSL_NODE_TYPE_BLOCK,
    RC_WGSL_NODE_TYPE_RETURN,
    RC_WGSL_NODE_TYPE_IF,
    RC_WGSL_NODE_TYPE_SWITCH,
    RC_WGSL_NODE_TYPE_CASE,
    RC_WGSL_NODE_TYPE_DEFAULT,
    RC_WGSL_NODE_TYPE_LOOP,
    RC_WGSL_NODE_TYPE_CONTINUING,
    RC_WGSL_NODE_TYPE_FOR,
    RC_WGSL_NODE_TYPE_WHILE,
    RC_WGSL_NODE_TYPE_BREAK,
    RC_WGSL_NODE_TYPE_CONTINUE,
    RC_WGSL_NODE_TYPE_DISCARD,
    RC_WGSL_NODE_TYPE_ASSIGN,
    RC_WGSL_NODE_TYPE_PHONY,
    RC_WGSL_NODE_TYPE_INCREMENT,
    RC_WGSL_NODE_TYPE_DECREMENT
} rc_wgsl_node_type;

typedef struct rc_wgsl_node {
    uint32_t token;
    uint32_t lhs;
    uint32_t rhs;
    uint8_t type;  // rc_wgsl_node_type
} rc_wgsl_node;

#define RC_ARRAY_TYPE rc_wgsl_node
#define RC_ARRAY_NAME wgsl_node
#include "richc/array.template.h"

// Indices of nodes, and the counts that lead lists.
#define RC_ARRAY_TYPE uint32_t
#define RC_ARRAY_NAME wgsl_index
#include "richc/array.template.h"

// The tree, with the source and tokens it was parsed from, which give the nodes their text.
typedef struct rc_wgsl_ast {
    rc_str source;
    rc_view_wgsl_token tokens;
    rc_view_wgsl_node nodes;
    rc_view_wgsl_index extra;
    uint32_t decls;  // the module's declarations (list)
} rc_wgsl_ast;

// The parts of a function, in extra.
typedef struct rc_wgsl_fn_parts {
    uint32_t attributes;         // list
    uint32_t params;             // list of PARAM
    uint32_t return_attributes;  // list
    uint32_t return_type;
    uint32_t body;               // BLOCK
} rc_wgsl_fn_parts;

// The parts of a var, in extra.
typedef struct rc_wgsl_var_parts {
    uint32_t attributes;  // list
    uint32_t templates;   // list: the address space, then the access mode
    uint32_t type;
    uint32_t initialiser;
} rc_wgsl_var_parts;

// The parts of an if after its condition, in extra.
typedef struct rc_wgsl_if_parts {
    uint32_t then;  // BLOCK
    uint32_t other;  // BLOCK, IF or none
} rc_wgsl_if_parts;

// The parts of a for loop's header, in extra. Each may be none.
typedef struct rc_wgsl_for_parts {
    uint32_t init;
    uint32_t condition;
    uint32_t update;
} rc_wgsl_for_parts;

// Node index.
static inline rc_wgsl_node rc_wgsl_ast_node(rc_wgsl_ast ast, uint32_t index)
{
    return rc_view_wgsl_node_get(ast.nodes, index);
}

// The text of the token that names node index.
static inline rc_str rc_wgsl_ast_text(rc_wgsl_ast ast, uint32_t index)
{
    uint32_t token = rc_wgsl_ast_node(ast, index).token;

    return rc_wgsl_token_text(ast.source, rc_view_wgsl_token_get(ast.tokens, token));
}

// The node's type.
static inline rc_wgsl_node_type rc_wgsl_ast_type(rc_wgsl_ast ast, uint32_t index)
{
    return (rc_wgsl_node_type)rc_wgsl_ast_node(ast, index).type;
}

// The node indices in the list at extra index list.
static inline rc_view_wgsl_index rc_wgsl_ast_list(rc_wgsl_ast ast, uint32_t list)
{
    uint32_t num = rc_view_wgsl_index_get(ast.extra, list);

    return rc_view_wgsl_index_subview(ast.extra, list + 1, list + 1 + num);
}

// Field i of the record at extra index record.
static inline uint32_t rc_wgsl_ast_field(rc_wgsl_ast ast, uint32_t record, uint32_t i)
{
    return rc_view_wgsl_index_get(ast.extra, record + i);
}

// The parts of the FN node at index.
static inline rc_wgsl_fn_parts rc_wgsl_ast_fn_parts(rc_wgsl_ast ast, uint32_t index)
{
    uint32_t record = rc_wgsl_ast_node(ast, index).lhs;

    return (rc_wgsl_fn_parts) {
        .attributes = rc_wgsl_ast_field(ast, record, 0),
        .params = rc_wgsl_ast_field(ast, record, 1),
        .return_attributes = rc_wgsl_ast_field(ast, record, 2),
        .return_type = rc_wgsl_ast_field(ast, record, 3),
        .body = rc_wgsl_ast_field(ast, record, 4)
    };
}

// The parts of the VAR node at index.
static inline rc_wgsl_var_parts rc_wgsl_ast_var_parts(rc_wgsl_ast ast, uint32_t index)
{
    uint32_t record = rc_wgsl_ast_node(ast, index).lhs;

    return (rc_wgsl_var_parts) {
        .attributes = rc_wgsl_ast_field(ast, record, 0),
        .templates = rc_wgsl_ast_field(ast, record, 1),
        .type = rc_wgsl_ast_field(ast, record, 2),
        .initialiser = rc_wgsl_ast_field(ast, record, 3)
    };
}

// The parts of the IF node at index, after its condition.
static inline rc_wgsl_if_parts rc_wgsl_ast_if_parts(rc_wgsl_ast ast, uint32_t index)
{
    uint32_t record = rc_wgsl_ast_node(ast, index).rhs;

    return (rc_wgsl_if_parts) {
        .then = rc_wgsl_ast_field(ast, record, 0),
        .other = rc_wgsl_ast_field(ast, record, 1)
    };
}

// The parts of the FOR node at index's header.
static inline rc_wgsl_for_parts rc_wgsl_ast_for_parts(rc_wgsl_ast ast, uint32_t index)
{
    uint32_t record = rc_wgsl_ast_node(ast, index).lhs;

    return (rc_wgsl_for_parts) {
        .init = rc_wgsl_ast_field(ast, record, 0),
        .condition = rc_wgsl_ast_field(ast, record, 1),
        .update = rc_wgsl_ast_field(ast, record, 2)
    };
}

// Push onto out the children of node that are statements or values: a call's arguments, a block's
// statements, a declaration's initialiser and so on, but not the types written in it.
void rc_wgsl_ast_push_children(rc_wgsl_ast ast, uint32_t node, rc_array_wgsl_index *out,
    rc_arena *arena);

#endif // RC_SHADER_AST_H_
