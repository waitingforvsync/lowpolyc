// ast.c
//
// Walking the syntax tree.

#include "ast.h"

// Push node onto out, unless it is none.
static void rc_wgsl_push_child(rc_array_wgsl_index *out, uint32_t node, rc_arena *arena)
{
    if (node != 0) {
        rc_array_wgsl_index_push(out, node, arena);
    }
}


// Push each node of the list onto out.
static void rc_wgsl_push_list(rc_wgsl_ast ast, rc_array_wgsl_index *out, uint32_t list,
    rc_arena *arena)
{
    rc_view_wgsl_index items = rc_wgsl_ast_list(ast, list);

    for (uint32_t i = 0; i < items.num; i++) {
        rc_wgsl_push_child(out, rc_view_wgsl_index_get(items, i), arena);
    }
}


// Push the children of the record-holding nodes: FN, VAR, IF and FOR.
static void rc_wgsl_push_record_children(rc_wgsl_ast ast, uint32_t node, rc_array_wgsl_index *out,
    rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(ast, node);

    if (n.type == RC_WGSL_NODE_TYPE_FN) {
        rc_wgsl_push_child(out, rc_wgsl_ast_fn_parts(ast, node).body, arena);
    }
    else if (n.type == RC_WGSL_NODE_TYPE_VAR) {
        rc_wgsl_push_child(out, rc_wgsl_ast_var_parts(ast, node).initialiser, arena);
    }
    else if (n.type == RC_WGSL_NODE_TYPE_IF) {
        rc_wgsl_if_parts parts = rc_wgsl_ast_if_parts(ast, node);

        rc_wgsl_push_child(out, n.lhs, arena);
        rc_wgsl_push_child(out, parts.then, arena);
        rc_wgsl_push_child(out, parts.other, arena);
    }
    else {
        rc_wgsl_for_parts parts = rc_wgsl_ast_for_parts(ast, node);

        rc_wgsl_push_child(out, parts.init, arena);
        rc_wgsl_push_child(out, parts.condition, arena);
        rc_wgsl_push_child(out, parts.update, arena);
        rc_wgsl_push_child(out, n.rhs, arena);
    }
}


void rc_wgsl_ast_push_children(rc_wgsl_ast ast, uint32_t node, rc_array_wgsl_index *out,
    rc_arena *arena)
{
    rc_wgsl_node n = rc_wgsl_ast_node(ast, node);

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_FN:
    case RC_WGSL_NODE_TYPE_VAR:
    case RC_WGSL_NODE_TYPE_IF:
    case RC_WGSL_NODE_TYPE_FOR:
        rc_wgsl_push_record_children(ast, node, out, arena);
        break;
    case RC_WGSL_NODE_TYPE_CALL:
    case RC_WGSL_NODE_TYPE_SWITCH:
        rc_wgsl_push_child(out, n.type == RC_WGSL_NODE_TYPE_SWITCH ? n.lhs : 0, arena);
        rc_wgsl_push_list(ast, out, n.rhs, arena);
        break;
    case RC_WGSL_NODE_TYPE_BLOCK:
        rc_wgsl_push_list(ast, out, n.lhs, arena);
        break;
    case RC_WGSL_NODE_TYPE_CASE:
        rc_wgsl_push_list(ast, out, n.lhs, arena);
        rc_wgsl_push_child(out, n.rhs, arena);
        break;
    case RC_WGSL_NODE_TYPE_LET:
    case RC_WGSL_NODE_TYPE_CONST:
        rc_wgsl_push_child(out, n.rhs, arena);
        break;
    case RC_WGSL_NODE_TYPE_INDEX:
    case RC_WGSL_NODE_TYPE_BINARY:
    case RC_WGSL_NODE_TYPE_LOOP:
    case RC_WGSL_NODE_TYPE_CONTINUING:
    case RC_WGSL_NODE_TYPE_WHILE:
    case RC_WGSL_NODE_TYPE_ASSIGN:
        rc_wgsl_push_child(out, n.lhs, arena);
        rc_wgsl_push_child(out, n.rhs, arena);
        break;
    case RC_WGSL_NODE_TYPE_MEMBER:
    case RC_WGSL_NODE_TYPE_UNARY:
    case RC_WGSL_NODE_TYPE_RETURN:
    case RC_WGSL_NODE_TYPE_INCREMENT:
    case RC_WGSL_NODE_TYPE_DECREMENT:
        rc_wgsl_push_child(out, n.lhs, arena);
        break;
    default:
        break;
    }
}
