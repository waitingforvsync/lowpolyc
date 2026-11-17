// check_graph.c
//
// What uses what. Each time the checker meets a module-scope declaration used by another, or by a
// function's body, it notes the reference (check.c). Grouped by user, the references make a graph
// that we walk depth first, keeping the path in arrays rather than on the call stack. One walk
// catches functions calling themselves; one from each entry point finds everything its program
// needs, each thing after whatever it depends on.

#include "checker.h"

#include "richc/mstr.h"

// Where a walk has got with a node: nowhere yet, on the path being followed, or finished.
typedef enum rc_wgsl_walk_state {
    RC_WGSL_WALK_STATE_NONE = 0,
    RC_WGSL_WALK_STATE_ON_PATH,
    RC_WGSL_WALK_STATE_DONE
} rc_wgsl_walk_state;

// Group the references the checker noted by their user, into the graph.
void rc_wgsl_check_graph(rc_wgsl_checker *c, rc_arena *arena)
{
    uint32_t num = c->ast.nodes.num;
    rc_span_wgsl_index starts = rc_span_wgsl_index_make_zero(num + 1, arena);
    rc_span_wgsl_reference grouped = rc_span_wgsl_reference_make(c->references.num, arena);

    // A counting sort: count each user's references, add the counts up into where each user's run
    // starts, then put each reference at the next place in its user's run. That leaves each start
    // where the next run starts, so they all move up one.
    for (uint32_t i = 0; i < c->references.num; i++) {
        (*rc_span_wgsl_index_at(starts, rc_array_wgsl_reference_get(&c->references, i).user + 1))++;
    }

    for (uint32_t n = 0; n < num; n++) {
        *rc_span_wgsl_index_at(starts, n + 1) += rc_span_wgsl_index_get(starts, n);
    }

    for (uint32_t i = 0; i < c->references.num; i++) {
        rc_wgsl_reference reference = rc_array_wgsl_reference_get(&c->references, i);
        uint32_t *start = rc_span_wgsl_index_at(starts, reference.user);

        rc_span_wgsl_reference_set(grouped, (*start)++, reference);
    }

    for (uint32_t n = num; n > 0; n--) {
        rc_span_wgsl_index_set(starts, n, rc_span_wgsl_index_get(starts, n - 1));
    }

    rc_span_wgsl_index_set(starts, 0, 0);
    c->graph = (rc_wgsl_graph) {
        .starts = starts.view,
        .references = grouped.view
    };
}


// The references node makes.
static rc_view_wgsl_reference rc_wgsl_references_of(rc_wgsl_graph graph, uint32_t node)
{
    uint32_t first = rc_view_wgsl_index_get(graph.starts, node);
    uint32_t last = rc_view_wgsl_index_get(graph.starts, node + 1);

    return rc_view_wgsl_reference_subview(graph.references, first, last);
}


// Walk from start, unless an earlier walk sharing states has been there, and append each node to
// order, if there is one, once everything it uses is there. A reference back onto the path is a
// function calling itself, since the checker has stopped every other kind of cycle already.
static void rc_wgsl_walk(rc_wgsl_checker *c, uint32_t start, rc_span_wgsl_state states, rc_array_wgsl_index *order, rc_arena *arena)
{
    rc_array_wgsl_index path = rc_array_wgsl_index_make(16, arena);
    rc_array_wgsl_index followed = rc_array_wgsl_index_make(16, arena);

    if (rc_span_wgsl_state_get(states, start) != RC_WGSL_WALK_STATE_NONE) {
        return;
    }

    rc_span_wgsl_state_set(states, start, RC_WGSL_WALK_STATE_ON_PATH);
    rc_array_wgsl_index_push(&path, start, arena);
    rc_array_wgsl_index_push(&followed, 0, arena);

    while (path.num > 0 && !rc_wgsl_check_failed(c)) {
        uint32_t node = rc_array_wgsl_index_get(&path, path.num - 1);
        uint32_t next = rc_array_wgsl_index_get(&followed, followed.num - 1);
        rc_view_wgsl_reference references = rc_wgsl_references_of(c->graph, node);
        rc_wgsl_reference reference = next < references.num ? rc_view_wgsl_reference_get(references, next) : (rc_wgsl_reference) {0};

        if (next == references.num) {
            rc_span_wgsl_state_set(states, node, RC_WGSL_WALK_STATE_DONE);
            rc_array_wgsl_index_pop(&path);
            rc_array_wgsl_index_pop(&followed);
            if (order != NULL) {
                rc_array_wgsl_index_push(order, node, arena);
            }

            continue;
        }

        rc_array_wgsl_index_set(&followed, followed.num - 1, next + 1);

        switch ((rc_wgsl_walk_state)rc_span_wgsl_state_get(states, reference.used)) {
        case RC_WGSL_WALK_STATE_ON_PATH:
            rc_wgsl_check_fail(c, reference.node, RC_STR("functions can't call themselves, even through others"));
            break;
        case RC_WGSL_WALK_STATE_NONE:
            rc_span_wgsl_state_set(states, reference.used, RC_WGSL_WALK_STATE_ON_PATH);
            rc_array_wgsl_index_push(&path, reference.used, arena);
            rc_array_wgsl_index_push(&followed, 0, arena);
            break;
        default:
            break;
        }
    }
}


// Fail at the first function that calls itself, directly or through others.
void rc_wgsl_check_recursion(rc_wgsl_checker *c, rc_arena *arena)
{
    rc_view_wgsl_index decls = rc_wgsl_ast_list(c->ast, c->ast.decls);
    rc_span_wgsl_state states = rc_span_wgsl_state_make_zero(c->ast.nodes.num, arena);

    for (uint32_t i = 0; i < decls.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t node = rc_view_wgsl_index_get(decls, i);

        if (rc_wgsl_ast_type(c->ast, node) == RC_WGSL_NODE_TYPE_FN) {
            rc_wgsl_walk(c, node, states, NULL, arena);
        }
    }
}


// The declaration whose memory the pointer expression node points into: a var, or a parameter
// holding a pointer. A let holding a pointer is followed to where it points.
static uint32_t rc_wgsl_pointer_root(const rc_wgsl_checker *c, uint32_t node)
{
    while (node != 0) {
        rc_wgsl_node n = rc_wgsl_ast_node(c->ast, node);
        uint32_t target = rc_wgsl_check_sema(c, node).target;

        switch ((rc_wgsl_node_type)n.type) {
        case RC_WGSL_NODE_TYPE_UNARY:
        case RC_WGSL_NODE_TYPE_MEMBER:
        case RC_WGSL_NODE_TYPE_INDEX:
            node = n.lhs;
            break;
        case RC_WGSL_NODE_TYPE_IDENT:
            if (rc_wgsl_ast_type(c->ast, target) != RC_WGSL_NODE_TYPE_LET) {
                return target;
            }

            node = rc_wgsl_ast_node(c->ast, target).rhs;
            break;
        default:
            return 0;
        }
    }
    return 0;
}


// The declaration whose memory the argument arg points into, or 0 if it is no pointer.
static uint32_t rc_wgsl_pointer_arg_root(const rc_wgsl_checker *c, uint32_t arg)
{
    if (rc_wgsl_check_typeinfo_type(c, rc_wgsl_check_sema(c, arg).type) != RC_WGSL_TYPEINFO_TYPE_POINTER) {
        return 0;
    }

    return rc_wgsl_pointer_root(c, arg);
}


// True if node is a module-scope var in private memory.
static bool rc_wgsl_is_private_var(const rc_wgsl_checker *c, uint32_t node)
{
    if (node == 0 || rc_wgsl_ast_type(c->ast, node) != RC_WGSL_NODE_TYPE_VAR) {
        return false;
    }

    return rc_wgsl_check_sema(c, node).space == RC_WGSL_SPACE_PRIVATE;
}


// True if the function fn uses the module-scope declaration decl, itself or through others.
static bool rc_wgsl_reaches(rc_wgsl_checker *c, uint32_t fn, uint32_t decl, rc_arena arena)
{
    rc_span_wgsl_state states = rc_span_wgsl_state_make_zero(c->ast.nodes.num, &arena);

    rc_wgsl_walk(c, fn, states, NULL, &arena);
    return rc_span_wgsl_state_get(states, decl) != RC_WGSL_WALK_STATE_NONE;
}


// Fail at argument i of args, pointing into root, if an argument before it points there too.
static void rc_wgsl_check_unshared(rc_wgsl_checker *c, rc_view_wgsl_index args, uint32_t i, uint32_t root)
{
    uint32_t arg = rc_view_wgsl_index_get(args, i);

    for (uint32_t j = 0; j < i && root != 0; j++) {
        if (rc_wgsl_pointer_arg_root(c, rc_view_wgsl_index_get(args, j)) == root) {
            rc_wgsl_check_fail(c, arg, RC_STR("this points at the same memory as another argument, which WGSL forbids"));
        }
    }
}


// Fail at arg, a pointer into the private var root, which the function called uses itself.
static void rc_wgsl_fail_reached_root(rc_wgsl_checker *c, uint32_t arg, uint32_t root, rc_arena *arena)
{
    rc_mstr message = rc_mstr_from_str(RC_STR("the function called uses '"), 64, arena);

    rc_mstr_append(&message, rc_wgsl_ast_text(c->ast, root), arena);
    rc_mstr_append(&message, RC_STR("' itself, which WGSL forbids with a pointer to it"), arena);
    rc_wgsl_check_fail(c, arg, message.view);
}


// Fail at an argument of the call node pointing into memory the call reaches another way.
static void rc_wgsl_check_pointer_call(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_view_wgsl_index args = rc_wgsl_ast_call_args(c->ast, node);
    uint32_t fn = rc_wgsl_check_sema(c, node).target;

    for (uint32_t i = 0; i < args.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t arg = rc_view_wgsl_index_get(args, i);
        uint32_t root = rc_wgsl_pointer_arg_root(c, arg);

        rc_wgsl_check_unshared(c, args, i, root);
        if (rc_wgsl_is_private_var(c, root) && rc_wgsl_reaches(c, fn, root, *arena)) {
            rc_wgsl_fail_reached_root(c, arg, root, arena);
        }
    }
}


// Fail at the first call passing pointers that WGSL forbids, as they point at memory used another
// way too.
void rc_wgsl_check_aliasing(rc_wgsl_checker *c, rc_arena *arena)
{
    for (uint32_t i = 0; i < c->pointer_calls.num && !rc_wgsl_check_failed(c); i++) {
        rc_wgsl_check_pointer_call(c, rc_array_wgsl_index_get(&c->pointer_calls, i), arena);
    }
}


// True if pairs holds pair.
static bool rc_wgsl_has_pair(rc_view_wgsl_texture_pair pairs, rc_wgsl_texture_pair pair)
{
    for (uint32_t i = 0; i < pairs.num; i++) {
        rc_wgsl_texture_pair other = rc_view_wgsl_texture_pair_get(pairs, i);

        if (other.texture == pair.texture && other.sampler == pair.sampler) {
            return true;
        }
    }
    return false;
}


// The texture and sampler pairs the functions done in states use, each once.
static rc_view_wgsl_texture_pair rc_wgsl_reached_pairs(const rc_wgsl_checker *c, rc_span_wgsl_state states, rc_arena *arena)
{
    rc_array_wgsl_texture_pair pairs = rc_array_wgsl_texture_pair_make(4, arena);

    for (uint32_t i = 0; i < c->texture_uses.num; i++) {
        rc_wgsl_texture_use use = rc_array_wgsl_texture_use_get(&c->texture_uses, i);

        if (rc_span_wgsl_state_get(states, use.function) == RC_WGSL_WALK_STATE_DONE && !rc_wgsl_has_pair(pairs.view, use.pair)) {
            rc_array_wgsl_texture_pair_push(&pairs, use.pair, arena);
        }
    }

    return pairs.view;
}


// Everything the program of the entry point fn needs, in an order where each thing comes after
// whatever it depends on.
rc_wgsl_entry rc_wgsl_check_reach(rc_wgsl_checker *c, uint32_t fn, rc_view_wgsl_io_slot inputs,
    rc_view_wgsl_io_slot outputs, rc_arena *arena)
{
    rc_span_wgsl_state states = rc_span_wgsl_state_make_zero(c->ast.nodes.num, arena);
    rc_array_wgsl_index order = rc_array_wgsl_index_make(32, arena);
    rc_array_wgsl_index structs = rc_array_wgsl_index_make(8, arena);
    rc_array_wgsl_index globals = rc_array_wgsl_index_make(8, arena);
    rc_array_wgsl_index functions = rc_array_wgsl_index_make(8, arena);
    rc_array_wgsl_index resources = rc_array_wgsl_index_make(8, arena);

    rc_wgsl_walk(c, fn, states, &order, arena);

    for (uint32_t i = 0; i < order.num; i++) {
        uint32_t node = rc_array_wgsl_index_get(&order, i);

        switch (rc_wgsl_ast_type(c->ast, node)) {
        case RC_WGSL_NODE_TYPE_STRUCT:
            rc_array_wgsl_index_push(&structs, node, arena);
            break;
        case RC_WGSL_NODE_TYPE_FN:
            rc_array_wgsl_index_push(&functions, node, arena);
            break;
        case RC_WGSL_NODE_TYPE_VAR:
        case RC_WGSL_NODE_TYPE_CONST:
            rc_array_wgsl_index_push(&globals, node, arena);
            if (rc_wgsl_is_resource_space((rc_wgsl_space)rc_wgsl_check_sema(c, node).space)) {
                rc_array_wgsl_index_push(&resources, node, arena);
            }

            break;
        default:
            break;
        }
    }

    return (rc_wgsl_entry) {
        .fn = fn,
        .structs = structs.view,
        .globals = globals.view,
        .functions = functions.view,
        .resources = resources.view,
        .pairs = rc_wgsl_reached_pairs(c, states, arena),
        .inputs = inputs,
        .outputs = outputs,
        .stage = rc_wgsl_check_sema(c, fn).stage
    };
}
