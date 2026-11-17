// check_entry.c
//
// Entry points: their inputs and outputs, and what their stage lets them reach.
//
// Each input and output is a parameter or the result, or a member of a struct one of those is,
// and carries @location or @builtin. A vertex shader must give @builtin(position).

#include "checker.h"

#include "richc/bitset.h"

// The inputs or outputs of one entry point seen so far, to catch repeats.
typedef struct rc_wgsl_io_seen {
    uint32_t locations;       // a bit per location
    uint32_t builtin_values;  // a bit per rc_wgsl_builtin_value
} rc_wgsl_io_seen;

// The type a built-in value has, and whether it can be found at place; 0 if it can't.
static uint32_t rc_wgsl_builtin_value_type(rc_wgsl_checker *c, rc_wgsl_builtin_value value, rc_wgsl_io_place place, rc_arena *arena)
{
    bool vertex = place.stage == RC_SHADER_STAGE_VERTEX;

    switch (value) {
    case RC_WGSL_BUILTIN_VALUE_POSITION:
        return rc_wgsl_is_inter_stage(place) ? rc_wgsl_type_vector(&c->types, RC_WGSL_TYPE_ID_F32, 4, arena) : 0;
    case RC_WGSL_BUILTIN_VALUE_VERTEX_INDEX:
    case RC_WGSL_BUILTIN_VALUE_INSTANCE_INDEX:
        return vertex && place.input ? RC_WGSL_TYPE_ID_U32 : 0;
    case RC_WGSL_BUILTIN_VALUE_FRONT_FACING:
        return !vertex && place.input ? RC_WGSL_TYPE_ID_BOOL : 0;
    case RC_WGSL_BUILTIN_VALUE_FRAG_DEPTH:
        return !vertex && !place.input ? RC_WGSL_TYPE_ID_F32 : 0;
    case RC_WGSL_BUILTIN_VALUE_SAMPLE_INDEX:
        return !vertex && place.input ? RC_WGSL_TYPE_ID_U32 : 0;
    case RC_WGSL_BUILTIN_VALUE_SAMPLE_MASK:
        return !vertex ? RC_WGSL_TYPE_ID_U32 : 0;
    default:
        return 0;
    }
}


// Check an input or output that is a built-in value.
static void rc_wgsl_check_builtin_io(rc_wgsl_checker *c, rc_wgsl_io_slot slot,
    rc_wgsl_io_place place, rc_wgsl_io_seen seen, rc_arena *arena)
{
    uint32_t expected = rc_wgsl_builtin_value_type(c, (rc_wgsl_builtin_value)slot.io.builtin_value, place, arena);

    if (expected == 0) {
        rc_wgsl_check_fail(c, slot.node,
            place.input ? RC_STR("this built-in can't be an input here") : RC_STR("this built-in can't be an output here"));
    }
    else if (slot.type != expected) {
        rc_wgsl_check_fail_types(c, slot.node, RC_STR("this built-in is "), expected, RC_STR(", not "), slot.type, arena);
    }
    else if (slot.io.interpolation != RC_WGSL_INTERPOLATE_NONE) {
        rc_wgsl_check_fail(c, slot.node, RC_STR("a built-in isn't interpolated"));
    }
    else if (seen.builtin_values & (1u << slot.io.builtin_value)) {
        rc_wgsl_check_fail(c, slot.node, RC_STR("this built-in is here already"));
    }
}


// True if the input or output slot holds integers but lacks @interpolate(flat), which they need
// to pass between stages.
static bool rc_wgsl_is_unflat_integer(const rc_wgsl_checker *c, rc_wgsl_io_slot slot)
{
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.infos.view, slot.type);

    if (scalar != RC_WGSL_TYPE_ID_I32 && scalar != RC_WGSL_TYPE_ID_U32) {
        return false;
    }

    return slot.io.interpolation != RC_WGSL_INTERPOLATE_FLAT;
}


// Check an input or output at a location.
static void rc_wgsl_check_location_io(rc_wgsl_checker *c, rc_wgsl_io_slot slot,
    rc_wgsl_io_place place, rc_wgsl_io_seen seen, rc_arena *arena)
{
    uint32_t scalar = rc_wgsl_type_scalar_of(c->types.infos.view, slot.type);

    if (!rc_wgsl_type_is_scalar_or_vector(c->types.infos.view, slot.type) || scalar == RC_WGSL_TYPE_ID_BOOL) {
        rc_wgsl_check_fail_type(c, slot.node, RC_STR("a location can't hold "), slot.type, RC_STR(""), arena);
    }
    else if (!rc_wgsl_is_inter_stage(place) && slot.io.interpolation != RC_WGSL_INTERPOLATE_NONE) {
        rc_wgsl_check_fail(c, slot.node, RC_STR("only values passed between stages are interpolated"));
    }
    else if (rc_wgsl_is_inter_stage(place) && rc_wgsl_is_unflat_integer(c, slot)) {
        rc_wgsl_check_fail(c, slot.node, RC_STR("an integer passed between stages needs @interpolate(flat)"));
    }
    else if (seen.locations & (1u << slot.io.location)) {
        rc_wgsl_check_fail(c, slot.node, RC_STR("this location is taken already"));
    }
}


// Check one input or output, given those seen before it.
static void rc_wgsl_check_io_slot(rc_wgsl_checker *c, rc_wgsl_io_slot slot, rc_wgsl_io_place place, rc_wgsl_io_seen seen, rc_arena *arena)
{
    bool located = slot.io.location != RC_INDEX_NONE;
    bool builtin = slot.io.builtin_value != RC_WGSL_BUILTIN_VALUE_NONE;

    if (!located && !builtin) {
        rc_wgsl_check_fail(c, slot.node, RC_STR("this needs @location or @builtin"));
    }
    else if (located && builtin) {
        rc_wgsl_check_fail(c, slot.node, RC_STR("this can't have both @location and @builtin"));
    }
    else if (builtin) {
        rc_wgsl_check_builtin_io(c, slot, place, seen, arena);
    }
    else {
        rc_wgsl_check_location_io(c, slot, place, seen, arena);
    }
}


// True if io carries @location or @builtin.
static bool rc_wgsl_io_is_marked(rc_wgsl_io io)
{
    return io.location != RC_INDEX_NONE || io.builtin_value != RC_WGSL_BUILTIN_VALUE_NONE;
}


// Push a slot for each member of the struct decl, the type of an input or output, part of param.
static void rc_wgsl_push_io_fields(const rc_wgsl_checker *c, rc_array_wgsl_io_slot *slots, uint32_t decl, uint32_t param, rc_arena *arena)
{
    rc_view_wgsl_index fields = rc_wgsl_ast_struct_fields(c->ast, decl);

    for (uint32_t i = 0; i < fields.num; i++) {
        uint32_t field = rc_view_wgsl_index_get(fields, i);
        rc_wgsl_io_slot slot = {
            .io = rc_wgsl_io_of(c->ast, c->sema.view, rc_wgsl_ast_node(c->ast, field).lhs),
            .node = field,
            .type = rc_wgsl_check_sema(c, field).type,
            .param = param,
            .field = field
        };

        rc_array_wgsl_io_slot_push(slots, slot, arena);
    }
}


// Push the input or output at node with attributes and type, part of param, as a slot; or a slot
// for each member, if it is a struct of inputs or outputs.
static void rc_wgsl_push_io(const rc_wgsl_checker *c, rc_array_wgsl_io_slot *slots, uint32_t node,
    uint32_t attributes, uint32_t type, uint32_t param, rc_arena *arena)
{
    rc_wgsl_io io = rc_wgsl_io_of(c->ast, c->sema.view, attributes);
    rc_wgsl_typeinfo t = rc_wgsl_check_typeinfo(c, type);

    if (rc_wgsl_io_is_marked(io) || t.type != RC_WGSL_TYPEINFO_TYPE_STRUCT) {
        rc_wgsl_io_slot slot = {.io = io, .node = node, .type = type, .param = param};

        rc_array_wgsl_io_slot_push(slots, slot, arena);
        return;
    }

    rc_wgsl_push_io_fields(c, slots, t.decl, param, arena);
}


// The inputs of the entry point fn, from its parameters, or its outputs, from its result.
static rc_view_wgsl_io_slot rc_wgsl_gather_ios(const rc_wgsl_checker *c, uint32_t fn, bool input, rc_arena *arena)
{
    rc_wgsl_fn_parts parts = rc_wgsl_ast_fn_parts(c->ast, fn);
    rc_view_wgsl_index params = rc_wgsl_ast_params(c->ast, fn);
    rc_array_wgsl_io_slot slots = rc_array_wgsl_io_slot_make(8, arena);

    for (uint32_t i = 0; input && i < params.num; i++) {
        uint32_t param = rc_view_wgsl_index_get(params, i);

        rc_wgsl_push_io(c, &slots, param, rc_wgsl_ast_node(c->ast, param).lhs, rc_wgsl_check_sema(c, param).type, i, arena);
    }
    if (!input && rc_wgsl_check_sema(c, fn).type != 0) {
        rc_wgsl_push_io(c, &slots, parts.return_type, parts.return_attributes, rc_wgsl_check_sema(c, fn).type, RC_INDEX_NONE, arena);
    }

    return slots.view;
}


// The inputs of the entry point fn, from its parameters, or its outputs, from its result; checked.
static rc_view_wgsl_io_slot rc_wgsl_check_ios(rc_wgsl_checker *c, uint32_t fn, bool input, rc_arena *arena)
{
    rc_view_wgsl_io_slot slots = rc_wgsl_gather_ios(c, fn, input, arena);
    rc_wgsl_io_place place = {.stage = rc_wgsl_check_sema(c, fn).stage, .input = input};
    rc_wgsl_io_seen seen = {0};

    for (uint32_t i = 0; i < slots.num && !rc_wgsl_check_failed(c); i++) {
        rc_wgsl_io_slot slot = rc_view_wgsl_io_slot_get(slots, i);

        rc_wgsl_check_io_slot(c, slot, place, seen, arena);
        if (slot.io.location != RC_INDEX_NONE) {
            seen.locations |= 1u << slot.io.location;
        }

        seen.builtin_values |= 1u << slot.io.builtin_value;
    }
    return slots;
}


// True if the entry point gives the built-in value among its outputs.
static bool rc_wgsl_outputs_builtin(rc_wgsl_entry entry, rc_wgsl_builtin_value value)
{
    for (uint32_t i = 0; i < entry.outputs.num; i++) {
        if (rc_view_wgsl_io_slot_get(entry.outputs, i).io.builtin_value == value) {
            return true;
        }
    }
    return false;
}


// Fail at the first thing the entry point's functions do that only a fragment shader may.
static void rc_wgsl_check_vertex_functions(rc_wgsl_checker *c, rc_wgsl_entry entry)
{
    for (uint32_t i = 0; i < entry.functions.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t at = rc_wgsl_check_sema(c, rc_view_wgsl_index_get(entry.functions, i)).fragment_only_at;

        if (at != 0) {
            rc_wgsl_check_fail(c, at, RC_STR("only a fragment shader can do this"));
        }
    }
}


// Fail at a resource the entry point uses that is bound where another it uses is.
static void rc_wgsl_check_bindings(rc_wgsl_checker *c, rc_wgsl_entry entry, rc_arena *arena)
{
    rc_bitset taken = rc_bitset_make(RC_WGSL_MAX_GROUPS * RC_WGSL_MAX_BINDINGS, arena);

    for (uint32_t i = 0; i < entry.resources.num && !rc_wgsl_check_failed(c); i++) {
        uint32_t var = rc_view_wgsl_index_get(entry.resources, i);
        rc_wgsl_binding binding = rc_wgsl_binding_of(c->ast, c->sema.view, var);
        uint32_t bit = binding.group * RC_WGSL_MAX_BINDINGS + binding.binding;

        if (rc_bitset_is_set(&taken, bit)) {
            rc_wgsl_check_fail_name(c, var, RC_STR("'"),
                RC_STR("' has the @group and @binding of another resource this entry point uses"), arena);
        }

        rc_bitset_set(&taken, bit);
    }
}


// Check the entry point node, giving everything its program needs.
rc_wgsl_entry rc_wgsl_check_entry_point(rc_wgsl_checker *c, uint32_t node, rc_arena *arena)
{
    rc_view_wgsl_io_slot inputs = rc_wgsl_check_ios(c, node, true, arena);
    rc_view_wgsl_io_slot outputs = rc_wgsl_check_ios(c, node, false, arena);
    rc_wgsl_entry entry = rc_wgsl_check_reach(c, node, inputs, outputs, arena);

    if (rc_wgsl_check_failed(c)) {
        return entry;
    }

    if (entry.stage == RC_SHADER_STAGE_VERTEX && !rc_wgsl_outputs_builtin(entry, RC_WGSL_BUILTIN_VALUE_POSITION)) {
        rc_wgsl_check_fail(c, node, RC_STR("a vertex shader must return @builtin(position)"));
    }

    if (entry.stage == RC_SHADER_STAGE_VERTEX) {
        rc_wgsl_check_vertex_functions(c, entry);
    }

    rc_wgsl_check_bindings(c, entry, arena);
    return entry;
}
