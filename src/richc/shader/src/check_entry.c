// check_entry.c
//
// Entry points: their inputs and outputs, and what their stage allows them to call.
//
// Each input and output is a parameter or the return value, or a member of a struct one of those
// has, and carries @location or @builtin. A vertex shader must give @builtin(position).

#include "checker.h"

#include "richc/mstr.h"

// The inputs or outputs of one entry point seen so far, to catch repeats.
typedef struct rc_wgsl_io_seen {
    rc_array_wgsl_index locations;
    uint32_t builtins;  // a bit per rc_wgsl_value
} rc_wgsl_io_seen;

// Which way an input or output goes, at which stage.
typedef struct rc_wgsl_io_place {
    rc_wgsl_stage stage;
    bool input;
} rc_wgsl_io_place;

// True if an input or output at place passes between stages, and so is interpolated.
static bool rc_wgsl_is_inter_stage(rc_wgsl_io_place place)
{
    return place.input == (place.stage == RC_WGSL_STAGE_FRAGMENT);
}


// The type a built-in value has, and whether it can be found at place; 0 if it can't.
static uint32_t rc_wgsl_builtin_value_type(rc_wgsl_checker *c, rc_wgsl_value value,
    rc_wgsl_io_place place, rc_arena *arena)
{
    bool vertex = place.stage == RC_WGSL_STAGE_VERTEX;

    switch (value) {
    case RC_WGSL_VALUE_POSITION:
        return rc_wgsl_is_inter_stage(place)
            ? rc_wgsl_type_vector(&c->types, RC_WGSL_TYPE_ID_F32, 4, arena) : 0;
    case RC_WGSL_VALUE_VERTEX_INDEX:
    case RC_WGSL_VALUE_INSTANCE_INDEX:
        return vertex && place.input ? RC_WGSL_TYPE_ID_U32 : 0;
    case RC_WGSL_VALUE_FRONT_FACING:
        return !vertex && place.input ? RC_WGSL_TYPE_ID_BOOL : 0;
    case RC_WGSL_VALUE_FRAG_DEPTH:
        return !vertex && !place.input ? RC_WGSL_TYPE_ID_F32 : 0;
    case RC_WGSL_VALUE_SAMPLE_INDEX:
        return !vertex && place.input ? RC_WGSL_TYPE_ID_U32 : 0;
    case RC_WGSL_VALUE_SAMPLE_MASK:
        return !vertex ? RC_WGSL_TYPE_ID_U32 : 0;
    default:
        return 0;
    }
}


// Check an input or output that is a built-in value, of type, at node.
static void rc_wgsl_check_builtin_io(rc_wgsl_checker *c, uint32_t node, rc_wgsl_io io,
    uint32_t type, rc_wgsl_io_place place, rc_wgsl_io_seen *seen, rc_arena *arena)
{
    uint32_t expected = rc_wgsl_builtin_value_type(c, (rc_wgsl_value)io.builtin, place,
        arena);
    uint32_t bit = 1u << io.builtin;

    if (expected == 0) {
        rc_wgsl_check_fail(c, node, place.input ? RC_STR("this built-in can't be an input here")
                                                : RC_STR("this built-in can't be an output here"));
    }
    else if (type != expected) {
        rc_wgsl_check_fail_types(c, node, RC_STR("this built-in is "), expected, RC_STR(", not "),
            type, arena);
    }
    else if (io.interpolation != RC_WGSL_INTERPOLATE_NONE) {
        rc_wgsl_check_fail(c, node, RC_STR("a built-in isn't interpolated"));
    }
    else if (seen->builtins & bit) {
        rc_wgsl_check_fail(c, node, RC_STR("this built-in is here already"));
    }
    seen->builtins |= bit;
}


// Check an input or output at a location, of type, at node.
static void rc_wgsl_check_location_io(rc_wgsl_checker *c, uint32_t node, rc_wgsl_io io,
    uint32_t type, rc_wgsl_io_place place, rc_wgsl_io_seen *seen, rc_arena *arena)
{
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.view, type);
    bool integer = scalar == RC_WGSL_TYPE_ID_I32 || scalar == RC_WGSL_TYPE_ID_U32;

    if (!rc_wgsl_type_is_scalar_or_vector(c->types.view, type) || scalar == RC_WGSL_TYPE_ID_BOOL) {
        rc_wgsl_check_fail_type(c, node, RC_STR("a location can't hold "), type, RC_STR(""), arena);
        return;
    }
    if (!rc_wgsl_is_inter_stage(place) && io.interpolation != RC_WGSL_INTERPOLATE_NONE) {
        rc_wgsl_check_fail(c, node, RC_STR("only values passed between stages are interpolated"));
    }
    else if (rc_wgsl_is_inter_stage(place) && integer
        && io.interpolation != RC_WGSL_INTERPOLATE_FLAT) {
        rc_wgsl_check_fail(c, node,
            RC_STR("an integer passed between stages needs @interpolate(flat)"));
    }
    for (uint32_t i = 0; i < seen->locations.num; i++) {
        if (rc_array_wgsl_index_get(&seen->locations, i) == io.location) {
            rc_wgsl_check_fail(c, node, RC_STR("this location is taken already"));
        }
    }
    rc_array_wgsl_index_push(&seen->locations, io.location, arena);
}


// Check an input or output with attributes and type, at node: a location, a built-in, or a struct
// of them if nested is false.
static void rc_wgsl_check_io(rc_wgsl_checker *c, uint32_t node, uint32_t attributes,
    uint32_t type, rc_wgsl_io_place place, bool nested, rc_wgsl_io_seen *seen, rc_arena *arena)
{
    rc_wgsl_io io = rc_wgsl_io_of(c->ast, c->sema.view, attributes);
    rc_wgsl_typeinfo t = rc_wgsl_check_typeinfo(c, type);

    if (io.location == RC_INDEX_NONE && io.builtin == RC_WGSL_VALUE_NONE
        && t.type == RC_WGSL_TYPEINFO_TYPE_STRUCT && !nested) {
        rc_view_wgsl_index fields = rc_wgsl_ast_list(c->ast, rc_wgsl_ast_node(c->ast, t.decl).lhs);

        for (uint32_t i = 0; i < fields.num && !rc_wgsl_check_failed(c); i++) {
            uint32_t field = rc_view_wgsl_index_get(fields, i);

            rc_wgsl_check_io(c, field, rc_wgsl_ast_node(c->ast, field).lhs,
                rc_wgsl_check_sema(c, field).type, place, true, seen, arena);
        }
        return;
    }
    if (io.location == RC_INDEX_NONE && io.builtin == RC_WGSL_VALUE_NONE) {
        rc_wgsl_check_fail(c, node, RC_STR("this needs @location or @builtin"));
    }
    else if (io.location != RC_INDEX_NONE && io.builtin != RC_WGSL_VALUE_NONE) {
        rc_wgsl_check_fail(c, node, RC_STR("this can't have both @location and @builtin"));
    }
    else if (io.builtin != RC_WGSL_VALUE_NONE) {
        rc_wgsl_check_builtin_io(c, node, io, type, place, seen, arena);
    }
    else {
        rc_wgsl_check_location_io(c, node, io, type, place, seen, arena);
    }
}


// True if fn is in found.
static bool rc_wgsl_is_found(rc_array_wgsl_index found, uint32_t fn)
{
    for (uint32_t i = 0; i < found.num; i++) {
        if (rc_array_wgsl_index_get(&found, i) == fn) {
            return true;
        }
    }
    return false;
}


// Fail at a function reachable from the vertex entry point node that does something only a
// fragment shader may. found holds every function reached, pending those still to look in.
static void rc_wgsl_check_vertex_calls(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_array_wgsl_index found = rc_array_wgsl_index_make(16, arena);
    rc_array_wgsl_index pending = rc_array_wgsl_index_make(16, arena);

    rc_array_wgsl_index_push(&found, node, arena);
    rc_array_wgsl_index_push(&pending, node, arena);
    while (pending.num > 0 && !rc_wgsl_check_failed(c)) {
        uint32_t fn = rc_array_wgsl_index_pop(&pending);
        uint32_t at = rc_wgsl_check_sema(c, fn).fragment_only_at;

        if (at != 0) {
            rc_wgsl_check_fail(c, at, RC_STR("only a fragment shader can do this"));
        }
        for (uint32_t i = 0; i < c->calls.num; i++) {
            rc_wgsl_edge edge = rc_array_wgsl_edge_get(&c->calls, i);

            if (edge.caller == fn && !rc_wgsl_is_found(found, edge.callee)) {
                rc_array_wgsl_index_push(&found, edge.callee, arena);
                rc_array_wgsl_index_push(&pending, edge.callee, arena);
            }
        }
    }
}


void rc_wgsl_check_entry_point(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_wgsl_fn_parts parts = rc_wgsl_ast_fn_parts(c->ast, node);
    rc_view_wgsl_index params = rc_wgsl_ast_list(c->ast, parts.params);
    rc_wgsl_stage stage = (rc_wgsl_stage)rc_wgsl_check_sema(c, node).stage;
    rc_wgsl_io_seen inputs = {.locations = rc_array_wgsl_index_make(8, arena)};
    rc_wgsl_io_seen outputs = {.locations = rc_array_wgsl_index_make(8, arena)};
    uint32_t type = rc_wgsl_check_sema(c, node).type;

    for (uint32_t i = 0; i < params.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t param = rc_view_wgsl_index_get(params, i);

        rc_wgsl_check_io(c, param, rc_wgsl_ast_node(c->ast, param).lhs,
            rc_wgsl_check_sema(c, param).type, (rc_wgsl_io_place) {.stage = stage, .input = true},
            false, &inputs, arena);
    }
    if (type != 0) {
        rc_wgsl_check_io(c, parts.return_type, parts.return_attributes, type,
            (rc_wgsl_io_place) {.stage = stage}, false, &outputs, arena);
    }
    if (rc_wgsl_check_failed(c) || stage != RC_WGSL_STAGE_VERTEX) {
        return;
    }
    if (!(outputs.builtins & (1u << RC_WGSL_VALUE_POSITION))) {
        rc_wgsl_check_fail(c, node, RC_STR("a vertex shader must return @builtin(position)"));
    }
    rc_wgsl_check_vertex_calls(c, node, arena);
}
