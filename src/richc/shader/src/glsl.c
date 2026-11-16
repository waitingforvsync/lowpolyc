// glsl.c
//
// The GLSL writer's driver: gathering what an entry point reaches, and writing the declarations
// around its functions, the inputs and outputs, and main.
//
// Module-scope vars and consts are declared without values and given them at the start of main.
// That is when WGSL gives them too, once per invocation, and it lets their values use anything,
// where a GLSL initialiser must be a constant expression without calls.

#include "glsl_writer.h"

#include "error.h"
#include "layout.h"

// ---- state ----

bool rc_glsl_failed(const rc_glsl_writer *w)
{
    return rc_wgsl_error_is_set(w->error);
}


void rc_glsl_fail(rc_glsl_writer *w, uint32_t node, rc_str message)
{
    uint32_t token = rc_wgsl_ast_node(w->module.ast, node).token;

    if (!rc_glsl_failed(w)) {
        uint32_t offset = rc_view_wgsl_token_get(w->module.ast.tokens, token).offset;

        w->error = rc_wgsl_error_at(w->module.ast.source, offset, message);
    }
}


void rc_glsl_fail_dialect(rc_glsl_writer *w, uint32_t node, rc_str what, rc_arena *arena)
{
    rc_mstr message;

    if (rc_glsl_failed(w)) {
        return;
    }
    message = rc_mstr_from_str(what, 64, arena);
    rc_mstr_append(&message, w->dialect == RC_GLSL_DIALECT_ES300
        ? RC_STR(" can't be written in GLSL ES 3.00") : RC_STR(" can't be written in GLSL 4.60"),
        arena);
    rc_glsl_fail(w, node, message.view);
}


rc_wgsl_sema rc_glsl_sema(const rc_glsl_writer *w, uint32_t node)
{
    return rc_view_wgsl_sema_get(w->module.sema, node);
}


rc_wgsl_typeinfo rc_glsl_typeinfo(const rc_glsl_writer *w, uint32_t id)
{
    return rc_array_wgsl_typeinfo_get(&w->types, id);
}


void rc_glsl_put(rc_glsl_writer *w, rc_str text, rc_arena *arena)
{
    rc_mstr_append(&w->code, text, arena);
}


void rc_glsl_line(rc_glsl_writer *w, rc_arena *arena)
{
    rc_mstr_append_char(&w->code, '\n', arena);
    rc_mstr_append_n(&w->code, ' ', 4 * w->indent, arena);
}


// True if node has every flag in flags.
static bool rc_glsl_is_marked(const rc_glsl_writer *w, uint32_t node, uint8_t flags)
{
    return (rc_span_glsl_mark_get(w->marks, node) & flags) == flags;
}


// Give node the flags in flags.
static void rc_glsl_mark(rc_glsl_writer *w, uint32_t node, uint8_t flags)
{
    *rc_span_glsl_mark_at(w->marks, node) |= flags;
}

// ---- names of resources ----

void rc_glsl_append_sampler_name(rc_mstr *out, rc_wgsl_module module, rc_glsl_pair pair,
    rc_arena *arena)
{
    rc_wgsl_binding texture = rc_wgsl_binding_of(module.ast, module.sema, pair.texture);

    rc_mstr_append(out, RC_STR("rc_t"), arena);
    rc_mstr_append_u32(out, texture.group, arena);
    rc_mstr_append_char(out, '_', arena);
    rc_mstr_append_u32(out, texture.binding, arena);
    if (pair.sampler != 0) {
        rc_wgsl_binding sampler = rc_wgsl_binding_of(module.ast, module.sema, pair.sampler);

        rc_mstr_append(out, RC_STR("_s"), arena);
        rc_mstr_append_u32(out, sampler.group, arena);
        rc_mstr_append_char(out, '_', arena);
        rc_mstr_append_u32(out, sampler.binding, arena);
    }
}


void rc_glsl_append_block_name(rc_mstr *out, rc_wgsl_module module, uint32_t var,
    rc_arena *arena)
{
    rc_wgsl_binding binding = rc_wgsl_binding_of(module.ast, module.sema, var);
    bool uniform = rc_view_wgsl_sema_get(module.sema, var).space == RC_WGSL_SPACE_UNIFORM;

    rc_mstr_append(out, uniform ? RC_STR("rc_ubo_") : RC_STR("rc_ssbo_"), arena);
    rc_mstr_append_u32(out, binding.group, arena);
    rc_mstr_append_char(out, '_', arena);
    rc_mstr_append_u32(out, binding.binding, arena);
}

// ---- gathering ----

static void rc_glsl_gather_function(rc_glsl_writer *w, uint32_t fn, rc_arena *arena);
static void rc_glsl_gather_global(rc_glsl_writer *w, uint32_t decl, rc_arena *arena);

// Gather the structs type needs, each after the structs it holds.
static void rc_glsl_gather_type(rc_glsl_writer *w, uint32_t type, rc_arena *arena)
{
    rc_wgsl_typeinfo t = rc_glsl_typeinfo(w, type);
    rc_view_wgsl_index fields;

    switch ((rc_wgsl_typeinfo_type)t.type) {
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
    case RC_WGSL_TYPEINFO_TYPE_ARRAY:
    case RC_WGSL_TYPEINFO_TYPE_POINTER:
        rc_glsl_gather_type(w, t.element, arena);
        return;
    case RC_WGSL_TYPEINFO_TYPE_STRUCT:
        break;
    default:
        return;
    }
    if (rc_glsl_is_marked(w, t.decl, RC_GLSL_MARK_VISITING)) {
        return;
    }
    rc_glsl_mark(w, t.decl, RC_GLSL_MARK_VISITING);
    fields = rc_wgsl_ast_list(w->module.ast, rc_wgsl_ast_node(w->module.ast, t.decl).lhs);
    for (uint32_t i = 0; i < fields.num; i++) {
        rc_glsl_gather_type(w, rc_glsl_sema(w, rc_view_wgsl_index_get(fields, i)).type, arena);
    }
    rc_array_wgsl_index_push(&w->structs, t.decl, arena);
}


// Gather the combined sampler a texture function's call uses, if it is new.
static void rc_glsl_gather_pair(rc_glsl_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_builtin builtin = (rc_wgsl_builtin)rc_glsl_sema(w, node).builtin;
    rc_view_wgsl_index args = rc_wgsl_ast_list(w->module.ast,
        rc_wgsl_ast_node(w->module.ast, node).rhs);
    bool sampled = builtin >= RC_WGSL_BUILTIN_TEXTURE_SAMPLE
        && builtin <= RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL;
    rc_glsl_pair pair = {.texture = rc_glsl_sema(w, rc_view_wgsl_index_get(args, 0)).target};

    if (sampled) {
        pair.sampler = rc_glsl_sema(w, rc_view_wgsl_index_get(args, 1)).target;
    }
    if (rc_glsl_pair_index(w, pair) == w->samplers.num) {
        rc_array_glsl_pair_push(&w->samplers, pair, arena);
    }
}


// Gather what node, an expression or statement, reaches: the functions it calls, the module-scope
// declarations it names, the structs of its type and the combined samplers it samples with.
static void rc_glsl_gather_node(rc_glsl_writer *w, uint32_t node, rc_arena *arena)
{
    rc_wgsl_sema sema = rc_glsl_sema(w, node);
    rc_wgsl_node_type node_type = rc_wgsl_ast_type(w->module.ast, node);

    rc_glsl_gather_type(w, sema.type, arena);
    if (node_type == RC_WGSL_NODE_TYPE_IDENT && rc_glsl_is_marked(w, sema.target, RC_GLSL_MARK_GLOBAL)) {
        rc_glsl_gather_global(w, sema.target, arena);
    }
    else if (node_type == RC_WGSL_NODE_TYPE_CALL && sema.call == RC_WGSL_CALL_FUNCTION) {
        rc_glsl_gather_function(w, sema.target, arena);
    }
    else if (node_type == RC_WGSL_NODE_TYPE_CALL && sema.builtin >= RC_WGSL_BUILTIN_TEXTURE_DIMENSIONS
        && sema.builtin <= RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL) {
        rc_glsl_gather_pair(w, node, arena);
    }
}


// Gather everything under root, which is a whole function body or a declaration's value.
static void rc_glsl_gather_tree(rc_glsl_writer *w, uint32_t root, rc_arena *arena)
{
    rc_array_wgsl_index pending = rc_array_wgsl_index_make(64, arena);

    // Children are pushed in order and taken last first, so each run is turned round to keep
    // what is gathered in the order the source has it
    rc_array_wgsl_index_push(&pending, root, arena);
    while (pending.num > 0 && !rc_glsl_failed(w)) {
        uint32_t node = rc_array_wgsl_index_pop(&pending);
        uint32_t first = pending.num;

        rc_glsl_gather_node(w, node, arena);
        rc_wgsl_ast_push_children(w->module.ast, node, &pending, arena);
        rc_span_wgsl_index_reverse(rc_span_wgsl_index_drop_first(pending.span, first));
    }
}


// Gather a module-scope var or const, after what its value needs.
static void rc_glsl_gather_global(rc_glsl_writer *w, uint32_t decl, rc_arena *arena)
{
    rc_wgsl_node d = rc_wgsl_ast_node(w->module.ast, decl);

    if (rc_glsl_is_marked(w, decl, RC_GLSL_MARK_VISITING)) {
        return;
    }
    rc_glsl_mark(w, decl, RC_GLSL_MARK_VISITING);
    rc_glsl_gather_type(w, rc_glsl_sema(w, decl).type, arena);
    rc_glsl_gather_tree(w, d.type == RC_WGSL_NODE_TYPE_VAR
        ? rc_wgsl_ast_var_parts(w->module.ast, decl).initialiser : d.rhs, arena);
    rc_array_wgsl_index_push(&w->globals, decl, arena);
}


// Gather a function, after the functions it calls and what it uses.
static void rc_glsl_gather_function(rc_glsl_writer *w, uint32_t fn, rc_arena *arena)
{
    rc_wgsl_fn_parts parts = rc_wgsl_ast_fn_parts(w->module.ast, fn);
    rc_view_wgsl_index params = rc_wgsl_ast_list(w->module.ast, parts.params);

    if (rc_glsl_is_marked(w, fn, RC_GLSL_MARK_VISITING)) {
        return;
    }
    rc_glsl_mark(w, fn, RC_GLSL_MARK_VISITING);
    rc_glsl_gather_type(w, rc_glsl_sema(w, fn).type, arena);
    for (uint32_t i = 0; i < params.num; i++) {
        rc_glsl_gather_type(w, rc_glsl_sema(w, rc_view_wgsl_index_get(params, i)).type, arena);
    }
    rc_glsl_gather_tree(w, parts.body, arena);
    rc_array_wgsl_index_push(&w->functions, fn, arena);
}


uint32_t rc_glsl_pair_index(const rc_glsl_writer *w, rc_glsl_pair pair)
{
    for (uint32_t i = 0; i < w->samplers.num; i++) {
        rc_glsl_pair other = rc_array_glsl_pair_get(&w->samplers, i);

        if (other.texture == pair.texture && other.sampler == pair.sampler) {
            return i;
        }
    }
    return w->samplers.num;
}

// ---- declarations ----

// Append the version and default precisions.
static void rc_glsl_write_header(rc_glsl_writer *w, rc_mstr *out, rc_arena *arena)
{
    if (w->dialect == RC_GLSL_DIALECT_ES300) {
        rc_mstr_append(out, RC_STR("#version 300 es\nprecision highp float;\nprecision highp int;\n"),
            arena);
    }
    else {
        rc_mstr_append(out, RC_STR("#version 460\n"), arena);
    }
}


// True if a struct holds an array sized at run time, which only a storage block can.
static bool rc_glsl_is_runtime_sized(const rc_glsl_writer *w, uint32_t type)
{
    rc_wgsl_typeinfo t = rc_glsl_typeinfo(w, type);
    rc_view_wgsl_index fields;

    if (t.type == RC_WGSL_TYPEINFO_TYPE_ARRAY) {
        return t.count == 0;
    }
    if (t.type != RC_WGSL_TYPEINFO_TYPE_STRUCT) {
        return false;
    }
    fields = rc_wgsl_ast_list(w->module.ast, rc_wgsl_ast_node(w->module.ast, t.decl).lhs);
    return rc_glsl_is_runtime_sized(w, rc_glsl_sema(w,
        rc_view_wgsl_index_get(fields, fields.num - 1)).type);
}


// Append a struct's fields as declarations, one to a line, at indent.
static void rc_glsl_write_fields(rc_glsl_writer *w, rc_mstr *out, uint32_t decl, rc_arena *arena)
{
    rc_view_wgsl_index fields = rc_wgsl_ast_list(w->module.ast,
        rc_wgsl_ast_node(w->module.ast, decl).lhs);

    for (uint32_t i = 0; i < fields.num; i++) {
        uint32_t field = rc_view_wgsl_index_get(fields, i);

        rc_mstr_append(out, RC_STR("    "), arena);
        rc_glsl_write_declaration(w, out, rc_glsl_sema(w, field).type,
            rc_wgsl_ast_text(w->module.ast, field), field, arena);
        rc_mstr_append(out, RC_STR(";\n"), arena);
    }
}


// Append every struct gathered, except those sized at run time, which become storage blocks.
static void rc_glsl_write_structs(rc_glsl_writer *w, rc_mstr *out, rc_arena *arena)
{
    for (uint32_t i = 0; i < w->structs.num; i++) {
        uint32_t decl = rc_array_wgsl_index_get(&w->structs, i);

        if (rc_glsl_is_runtime_sized(w, rc_glsl_sema(w, decl).type)) {
            continue;
        }
        rc_mstr_append(out, RC_STR("struct "), arena);
        rc_glsl_append_name(out, rc_wgsl_ast_text(w->module.ast, decl), arena);
        rc_mstr_append(out, RC_STR(" {\n"), arena);
        rc_glsl_write_fields(w, out, decl, arena);
        rc_mstr_append(out, RC_STR("};\n"), arena);
    }
}

// ---- inputs and outputs ----

// Which way an input or output goes, at the entry point's stage.
typedef struct rc_glsl_place {
    rc_wgsl_stage stage;
    bool input;
} rc_glsl_place;

// Append the name of the variable for the input or output at location, at place.
static void rc_glsl_append_io_name(rc_mstr *out, uint32_t location, rc_glsl_place place,
    rc_arena *arena)
{
    bool vertex = place.stage == RC_WGSL_STAGE_VERTEX;

    rc_mstr_append(out, vertex && place.input ? RC_STR("rc_attr_")
                      : !vertex && !place.input ? RC_STR("rc_frag_") : RC_STR("rc_var_"), arena);
    rc_mstr_append_u32(out, location, arena);
}


// Append the qualifiers for io's interpolation, when it passes between stages.
static void rc_glsl_write_interpolation(rc_glsl_writer *w, rc_mstr *out, rc_wgsl_io io,
    uint32_t node, rc_arena *arena)
{
    if (io.interpolation == RC_WGSL_INTERPOLATE_FLAT) {
        rc_mstr_append(out, RC_STR("flat "), arena);
    }
    else if (io.interpolation == RC_WGSL_INTERPOLATE_LINEAR && w->dialect == RC_GLSL_DIALECT_ES300) {
        rc_glsl_fail_dialect(w, node, RC_STR("@interpolate(linear)"), arena);
    }
    else if (io.interpolation == RC_WGSL_INTERPOLATE_LINEAR) {
        rc_mstr_append(out, RC_STR("noperspective "), arena);
    }
    if (io.sampling == RC_WGSL_SAMPLING_CENTROID) {
        rc_mstr_append(out, RC_STR("centroid "), arena);
    }
    else if (io.sampling == RC_WGSL_SAMPLING_SAMPLE && w->dialect == RC_GLSL_DIALECT_ES300) {
        rc_glsl_fail_dialect(w, node, RC_STR("@interpolate(..., sample)"), arena);
    }
    else if (io.sampling == RC_WGSL_SAMPLING_SAMPLE) {
        rc_mstr_append(out, RC_STR("sample "), arena);
    }
}


// Append the declaration of one input or output at a location, or of gl_Position as invariant.
static void rc_glsl_declare_io(rc_glsl_writer *w, rc_mstr *out, uint32_t node, uint32_t attributes,
    uint32_t type, rc_glsl_place place, rc_arena *arena)
{
    rc_wgsl_io io = rc_wgsl_io_of(w->module.ast, w->module.sema, attributes);
    bool inter_stage = place.input == (place.stage == RC_WGSL_STAGE_FRAGMENT);

    if (io.builtin == RC_WGSL_VALUE_POSITION && io.invariant && !place.input) {
        rc_mstr_append(out, RC_STR("invariant gl_Position;\n"), arena);
    }
    if (io.location == RC_INDEX_NONE) {
        return;
    }
    if (!inter_stage) {
        rc_mstr_append(out, RC_STR("layout(location = "), arena);
        rc_mstr_append_u32(out, io.location, arena);
        rc_mstr_append(out, RC_STR(") "), arena);
    }
    else {
        rc_glsl_write_interpolation(w, out, io, node, arena);
    }
    rc_mstr_append(out, place.input ? RC_STR("in ") : RC_STR("out "), arena);
    rc_glsl_write_type(w, out, type, node, arena);
    rc_mstr_append_char(out, ' ', arena);
    rc_glsl_append_io_name(out, io.location, place, arena);
    rc_mstr_append(out, RC_STR(";\n"), arena);
}


// True if an input or output of type with attributes is a struct whose members are the inputs or
// outputs.
static bool rc_glsl_is_io_struct(const rc_glsl_writer *w, uint32_t attributes, uint32_t type)
{
    rc_wgsl_io io = rc_wgsl_io_of(w->module.ast, w->module.sema, attributes);

    return rc_glsl_typeinfo(w, type).type == RC_WGSL_TYPEINFO_TYPE_STRUCT
        && io.location == RC_INDEX_NONE && io.builtin == RC_WGSL_VALUE_NONE;
}


// Append the declarations for a parameter or return value, or each member of its struct.
static void rc_glsl_declare_io_value(rc_glsl_writer *w, rc_mstr *out, uint32_t node,
    uint32_t attributes, uint32_t type, rc_glsl_place place, rc_arena *arena)
{
    rc_view_wgsl_index fields;

    if (!rc_glsl_is_io_struct(w, attributes, type)) {
        rc_glsl_declare_io(w, out, node, attributes, type, place, arena);
        return;
    }
    fields = rc_wgsl_ast_list(w->module.ast,
        rc_wgsl_ast_node(w->module.ast, rc_glsl_typeinfo(w, type).decl).lhs);
    for (uint32_t i = 0; i < fields.num; i++) {
        uint32_t field = rc_view_wgsl_index_get(fields, i);

        rc_glsl_declare_io(w, out, field, rc_wgsl_ast_node(w->module.ast, field).lhs,
            rc_glsl_sema(w, field).type, place, arena);
    }
}


// Append the declarations of the entry point's inputs and outputs.
static void rc_glsl_write_io(rc_glsl_writer *w, rc_mstr *out, rc_arena *arena)
{
    rc_wgsl_fn_parts parts = rc_wgsl_ast_fn_parts(w->module.ast, w->entry);
    rc_view_wgsl_index params = rc_wgsl_ast_list(w->module.ast, parts.params);
    rc_wgsl_stage stage = (rc_wgsl_stage)rc_glsl_sema(w, w->entry).stage;
    uint32_t type = rc_glsl_sema(w, w->entry).type;

    for (uint32_t i = 0; i < params.num; i++) {
        uint32_t param = rc_view_wgsl_index_get(params, i);

        rc_glsl_declare_io_value(w, out, param, rc_wgsl_ast_node(w->module.ast, param).lhs,
            rc_glsl_sema(w, param).type, (rc_glsl_place) {.stage = stage, .input = true}, arena);
    }
    if (type != 0) {
        rc_glsl_declare_io_value(w, out, parts.return_type, parts.return_attributes, type,
            (rc_glsl_place) {.stage = stage}, arena);
    }
}

// ---- buffers and samplers ----

// Fail unless a buffer of type lays out the same in GLSL as in WGSL, blaming var if no field.
static void rc_glsl_check_layout(rc_glsl_writer *w, uint32_t var, uint32_t type, bool uniform)
{
    uint32_t mismatch = rc_wgsl_layout_mismatch(w->module, type,
        uniform ? RC_WGSL_LAYOUT_RULE_STD140 : RC_WGSL_LAYOUT_RULE_STD430);

    if (mismatch == 0) {
        return;
    }
    rc_glsl_fail(w, mismatch == RC_INDEX_NONE ? var : mismatch, uniform
        ? RC_STR("GLSL's std140 lays this out differently: in a uniform buffer, avoid matrices "
            "with two rows, and use @align and @size only where std140 agrees")
        : RC_STR("GLSL's std430 lays this out differently: use @align and @size only where "
            "std430 agrees"));
}


// Append a declaration in a storage block: an array sized at run time is written name[].
static void rc_glsl_write_block_member(rc_glsl_writer *w, rc_mstr *out, uint32_t type,
    rc_str name, uint32_t node, rc_arena *arena)
{
    rc_wgsl_typeinfo t = rc_glsl_typeinfo(w, type);

    rc_mstr_append(out, RC_STR("    "), arena);
    if (t.type == RC_WGSL_TYPEINFO_TYPE_ARRAY && t.count == 0) {
        rc_glsl_write_declaration(w, out, t.element, name, node, arena);
        rc_mstr_append(out, RC_STR("[]"), arena);
    }
    else {
        rc_glsl_write_declaration(w, out, type, name, node, arena);
    }
    rc_mstr_append(out, RC_STR(";\n"), arena);
}


// Append the block for one uniform or storage var. A struct sized at run time spreads its members
// across the block, under the var's name, so var.member still reaches them.
static void rc_glsl_write_buffer(rc_glsl_writer *w, rc_mstr *out, uint32_t var, rc_arena *arena)
{
    rc_wgsl_sema sema = rc_glsl_sema(w, var);
    bool uniform = sema.space == RC_WGSL_SPACE_UNIFORM;
    rc_str name = rc_wgsl_ast_text(w->module.ast, var);
    bool spread = rc_glsl_typeinfo(w, sema.type).type == RC_WGSL_TYPEINFO_TYPE_STRUCT
        && rc_glsl_is_runtime_sized(w, sema.type);

    if (!uniform && w->dialect == RC_GLSL_DIALECT_ES300) {
        rc_glsl_fail_dialect(w, var, RC_STR("storage buffers"), arena);
        return;
    }
    rc_glsl_check_layout(w, var, sema.type, uniform);
    rc_mstr_append(out, uniform ? RC_STR("layout(std140) uniform ") : RC_STR("layout(std430) "),
        arena);
    if (!uniform) {
        rc_mstr_append(out, sema.access == RC_WGSL_ACCESS_READ ? RC_STR("readonly buffer ")
                                                               : RC_STR("buffer "), arena);
    }
    rc_glsl_append_block_name(out, w->module, var, arena);
    rc_mstr_append(out, RC_STR(" {\n"), arena);
    if (spread) {
        rc_view_wgsl_index fields = rc_wgsl_ast_list(w->module.ast,
            rc_wgsl_ast_node(w->module.ast, rc_glsl_typeinfo(w, sema.type).decl).lhs);

        for (uint32_t i = 0; i < fields.num; i++) {
            uint32_t field = rc_view_wgsl_index_get(fields, i);

            rc_glsl_write_block_member(w, out, rc_glsl_sema(w, field).type,
                rc_wgsl_ast_text(w->module.ast, field), field, arena);
        }
        rc_mstr_append(out, RC_STR("} "), arena);
        rc_glsl_append_name(out, name, arena);
        rc_mstr_append(out, RC_STR(";\n"), arena);
        return;
    }
    rc_glsl_write_block_member(w, out, sema.type, name, var, arena);
    rc_mstr_append(out, RC_STR("};\n"), arena);
}


// Append the GLSL sampler type of a combined sampler, as sampler2DShadow or usampler3D.
static void rc_glsl_write_sampler_type(rc_glsl_writer *w, rc_mstr *out, rc_glsl_pair pair,
    rc_arena *arena)
{
    static const rc_str dims[] = {
        [RC_WGSL_DIM_NONE] = RC_STR_INIT(""),
        [RC_WGSL_DIM_1D] = RC_STR_INIT("1D"),
        [RC_WGSL_DIM_2D] = RC_STR_INIT("2D"),
        [RC_WGSL_DIM_2D_ARRAY] = RC_STR_INIT("2DArray"),
        [RC_WGSL_DIM_3D] = RC_STR_INIT("3D"),
        [RC_WGSL_DIM_CUBE] = RC_STR_INIT("Cube"),
        [RC_WGSL_DIM_CUBE_ARRAY] = RC_STR_INIT("CubeArray")
    };
    rc_wgsl_typeinfo t = rc_glsl_typeinfo(w, rc_glsl_sema(w, pair.texture).type);
    bool comparison = pair.sampler != 0
        && rc_glsl_sema(w, pair.sampler).type == RC_WGSL_TYPE_ID_SAMPLER_COMPARISON;
    bool depth = t.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH
        || t.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH_MULTISAMPLED;
    bool multisampled = t.texture_class == RC_WGSL_TEXTURE_CLASS_MULTISAMPLED
        || t.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH_MULTISAMPLED;

    if (w->dialect == RC_GLSL_DIALECT_ES300 && (multisampled || t.dim == RC_WGSL_DIM_1D
        || t.dim == RC_WGSL_DIM_CUBE_ARRAY)) {
        rc_glsl_fail_dialect(w, pair.texture, RC_STR("this texture type"), arena);
    }
    if (!depth && t.element == RC_WGSL_TYPE_ID_I32) {
        rc_mstr_append_char(out, 'i', arena);
    }
    else if (!depth && t.element == RC_WGSL_TYPE_ID_U32) {
        rc_mstr_append_char(out, 'u', arena);
    }
    rc_mstr_append(out, RC_STR("sampler"), arena);
    RC_ASSERT(t.dim < RC_COUNT(dims));
    rc_mstr_append(out, multisampled ? RC_STR("2DMS") : dims[t.dim], arena);
    if (depth && comparison) {
        rc_mstr_append(out, RC_STR("Shadow"), arena);
    }
}


// Append the declarations of every buffer and combined sampler gathered.
static void rc_glsl_write_resources(rc_glsl_writer *w, rc_mstr *out, rc_arena *arena)
{
    for (uint32_t i = 0; i < w->globals.num && !rc_glsl_failed(w); i++) {
        uint32_t var = rc_array_wgsl_index_get(&w->globals, i);
        rc_wgsl_space space = (rc_wgsl_space)rc_glsl_sema(w, var).space;

        if (rc_wgsl_ast_type(w->module.ast, var) == RC_WGSL_NODE_TYPE_VAR
            && (space == RC_WGSL_SPACE_UNIFORM || space == RC_WGSL_SPACE_STORAGE)) {
            rc_glsl_write_buffer(w, out, var, arena);
        }
    }
    for (uint32_t i = 0; i < w->samplers.num; i++) {
        rc_glsl_pair pair = rc_array_glsl_pair_get(&w->samplers, i);

        rc_mstr_append(out, RC_STR("uniform highp "), arena);
        rc_glsl_write_sampler_type(w, out, pair, arena);
        rc_mstr_append_char(out, ' ', arena);
        rc_glsl_append_sampler_name(out, w->module, pair, arena);
        rc_mstr_append(out, RC_STR(";\n"), arena);
    }
}

// ---- globals and main ----

// True if a gathered module-scope declaration is a private var or a concrete const, which become
// GLSL globals given their values in main.
static bool rc_glsl_is_plain_global(const rc_glsl_writer *w, uint32_t decl)
{
    rc_wgsl_sema sema = rc_glsl_sema(w, decl);

    if (rc_wgsl_ast_type(w->module.ast, decl) == RC_WGSL_NODE_TYPE_CONST) {
        return !rc_wgsl_type_is_abstract(w->types.view, sema.type);
    }
    return sema.space == RC_WGSL_SPACE_PRIVATE;
}


// Write the declarations of the private vars and concrete consts.
static void rc_glsl_write_globals(rc_glsl_writer *w, rc_arena *arena)
{
    for (uint32_t i = 0; i < w->globals.num; i++) {
        uint32_t decl = rc_array_wgsl_index_get(&w->globals, i);

        if (rc_glsl_is_plain_global(w, decl)) {
            rc_glsl_line(w, arena);
            rc_glsl_write_declaration(w, &w->code, rc_glsl_sema(w, decl).type,
                rc_wgsl_ast_text(w->module.ast, decl), decl, arena);
            rc_glsl_put(w, RC_STR(";"), arena);
        }
    }
    rc_glsl_put(w, RC_STR("\n"), arena);
}


// Write the statements at the top of main giving the globals their values, in order.
static void rc_glsl_write_global_values(rc_glsl_writer *w, rc_arena *arena)
{
    for (uint32_t i = 0; i < w->globals.num; i++) {
        uint32_t decl = rc_array_wgsl_index_get(&w->globals, i);
        rc_wgsl_node d = rc_wgsl_ast_node(w->module.ast, decl);
        uint32_t value = d.type == RC_WGSL_NODE_TYPE_VAR
            ? rc_wgsl_ast_var_parts(w->module.ast, decl).initialiser : d.rhs;

        if (!rc_glsl_is_plain_global(w, decl)) {
            continue;
        }
        rc_glsl_line(w, arena);
        rc_glsl_append_name(&w->code, rc_wgsl_ast_text(w->module.ast, decl), arena);
        rc_glsl_put(w, RC_STR(" = "), arena);
        if (value != 0) {
            rc_glsl_write_expr(w, value, 0, arena);
        }
        else {
            rc_glsl_write_zero(w, &w->code, rc_glsl_sema(w, decl).type, decl, arena);
        }
        rc_glsl_put(w, RC_STR(";"), arena);
    }
}


// Write the value of an input with attributes at node: its variable, or a GLSL built-in.
static void rc_glsl_write_input(rc_glsl_writer *w, uint32_t node, uint32_t attributes,
    rc_arena *arena)
{
    rc_wgsl_io io = rc_wgsl_io_of(w->module.ast, w->module.sema, attributes);
    rc_wgsl_stage stage = (rc_wgsl_stage)rc_glsl_sema(w, w->entry).stage;
    bool desktop = w->dialect == RC_GLSL_DIALECT_460;

    switch ((rc_wgsl_value)io.builtin) {
    case RC_WGSL_VALUE_NONE:
        rc_glsl_append_io_name(&w->code, io.location,
            (rc_glsl_place) {.stage = stage, .input = true}, arena);
        break;
    case RC_WGSL_VALUE_POSITION:
        rc_glsl_put(w, RC_STR("gl_FragCoord"), arena);
        break;
    case RC_WGSL_VALUE_FRONT_FACING:
        rc_glsl_put(w, RC_STR("gl_FrontFacing"), arena);
        break;
    case RC_WGSL_VALUE_VERTEX_INDEX:
        rc_glsl_put(w, RC_STR("uint(gl_VertexID)"), arena);
        break;
    case RC_WGSL_VALUE_INSTANCE_INDEX:
        rc_glsl_put(w, desktop ? RC_STR("uint(gl_InstanceID + gl_BaseInstance)")
                               : RC_STR("uint(gl_InstanceID)"), arena);
        break;
    case RC_WGSL_VALUE_SAMPLE_INDEX:
    case RC_WGSL_VALUE_SAMPLE_MASK:
        if (!desktop) {
            rc_glsl_fail_dialect(w, node, RC_STR("this built-in"), arena);
        }
        rc_glsl_put(w, io.builtin == RC_WGSL_VALUE_SAMPLE_INDEX ? RC_STR("uint(gl_SampleID)")
                                                                : RC_STR("uint(gl_SampleMaskIn[0])"),
            arena);
        break;
    default:
        rc_glsl_fail(w, node, RC_STR("this input can't be written in GLSL"));
        break;
    }
}


// Write the value passed for a parameter: its input, or its struct built from its members'.
static void rc_glsl_write_argument(rc_glsl_writer *w, uint32_t param, rc_arena *arena)
{
    uint32_t attributes = rc_wgsl_ast_node(w->module.ast, param).lhs;
    uint32_t type = rc_glsl_sema(w, param).type;
    rc_view_wgsl_index fields;

    if (!rc_glsl_is_io_struct(w, attributes, type)) {
        rc_glsl_write_input(w, param, attributes, arena);
        return;
    }
    fields = rc_wgsl_ast_list(w->module.ast,
        rc_wgsl_ast_node(w->module.ast, rc_glsl_typeinfo(w, type).decl).lhs);
    rc_glsl_write_type(w, &w->code, type, param, arena);
    rc_glsl_put(w, RC_STR("("), arena);
    for (uint32_t i = 0; i < fields.num; i++) {
        uint32_t field = rc_view_wgsl_index_get(fields, i);

        rc_glsl_put(w, i > 0 ? RC_STR(", ") : RC_STR(""), arena);
        rc_glsl_write_input(w, field, rc_wgsl_ast_node(w->module.ast, field).lhs, arena);
    }
    rc_glsl_put(w, RC_STR(")"), arena);
}


// Write a statement storing value, the result or a member of it, in the output with attributes.
static void rc_glsl_write_output(rc_glsl_writer *w, uint32_t node, uint32_t attributes,
    rc_str value, rc_arena *arena)
{
    rc_wgsl_io io = rc_wgsl_io_of(w->module.ast, w->module.sema, attributes);
    rc_wgsl_stage stage = (rc_wgsl_stage)rc_glsl_sema(w, w->entry).stage;

    rc_glsl_line(w, arena);
    switch ((rc_wgsl_value)io.builtin) {
    case RC_WGSL_VALUE_NONE:
        rc_glsl_append_io_name(&w->code, io.location, (rc_glsl_place) {.stage = stage}, arena);
        rc_glsl_put(w, RC_STR(" = "), arena);
        break;
    case RC_WGSL_VALUE_POSITION:
        rc_glsl_put(w, RC_STR("gl_Position = "), arena);
        break;
    case RC_WGSL_VALUE_FRAG_DEPTH:
        rc_glsl_put(w, RC_STR("gl_FragDepth = "), arena);
        break;
    default:
        if (w->dialect == RC_GLSL_DIALECT_ES300) {
            rc_glsl_fail_dialect(w, node, RC_STR("@builtin(sample_mask)"), arena);
        }
        rc_glsl_put(w, RC_STR("gl_SampleMask[0] = int("), arena);
        rc_glsl_put(w, value, arena);
        rc_glsl_put(w, RC_STR(");"), arena);
        return;
    }
    rc_glsl_put(w, value, arena);
    rc_glsl_put(w, RC_STR(";"), arena);
}


// Write the statements storing the entry point's result in its outputs.
static void rc_glsl_write_outputs(rc_glsl_writer *w, rc_arena *arena)
{
    rc_wgsl_fn_parts parts = rc_wgsl_ast_fn_parts(w->module.ast, w->entry);
    uint32_t type = rc_glsl_sema(w, w->entry).type;
    rc_view_wgsl_index fields;

    if (!rc_glsl_is_io_struct(w, parts.return_attributes, type)) {
        rc_glsl_write_output(w, parts.return_type, parts.return_attributes, RC_STR("rc_result"),
            arena);
        return;
    }
    fields = rc_wgsl_ast_list(w->module.ast,
        rc_wgsl_ast_node(w->module.ast, rc_glsl_typeinfo(w, type).decl).lhs);
    for (uint32_t i = 0; i < fields.num; i++) {
        uint32_t field = rc_view_wgsl_index_get(fields, i);
        rc_mstr value = rc_mstr_from_str(RC_STR("rc_result."), 32, arena);

        rc_glsl_append_name(&value, rc_wgsl_ast_text(w->module.ast, field), arena);
        rc_glsl_write_output(w, field, rc_wgsl_ast_node(w->module.ast, field).lhs, value.view,
            arena);
    }
}


// Write main: give the globals their values, call the entry point with its inputs, and store its
// result in its outputs.
static void rc_glsl_write_main(rc_glsl_writer *w, rc_arena *arena)
{
    rc_view_wgsl_index params = rc_wgsl_ast_list(w->module.ast,
        rc_wgsl_ast_fn_parts(w->module.ast, w->entry).params);
    uint32_t type = rc_glsl_sema(w, w->entry).type;

    rc_glsl_line(w, arena);
    rc_glsl_put(w, RC_STR("void main() {"), arena);
    w->indent++;
    rc_glsl_write_global_values(w, arena);
    rc_glsl_line(w, arena);
    if (type != 0) {
        rc_glsl_write_type(w, &w->code, type, w->entry, arena);
        rc_glsl_put(w, RC_STR(" rc_result = "), arena);
    }
    rc_glsl_append_name(&w->code, rc_wgsl_ast_text(w->module.ast, w->entry), arena);
    rc_glsl_put(w, RC_STR("("), arena);
    for (uint32_t i = 0; i < params.num; i++) {
        rc_glsl_put(w, i > 0 ? RC_STR(", ") : RC_STR(""), arena);
        rc_glsl_write_argument(w, rc_view_wgsl_index_get(params, i), arena);
    }
    rc_glsl_put(w, RC_STR(");"), arena);
    if (type != 0) {
        rc_glsl_write_outputs(w, arena);
    }

    // WebGPU's clip-space depth runs from 0 to 1, and GL ES's from -1 to 1
    if (w->dialect == RC_GLSL_DIALECT_ES300
        && rc_glsl_sema(w, w->entry).stage == RC_WGSL_STAGE_VERTEX) {
        rc_glsl_line(w, arena);
        rc_glsl_put(w, RC_STR("gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;"), arena);
    }
    w->indent--;
    rc_glsl_line(w, arena);
    rc_glsl_put(w, RC_STR("}\n"), arena);
}

// ---- writing ----

// The uniform and storage vars among the globals gathered.
static rc_view_wgsl_index rc_glsl_buffers(const rc_glsl_writer *w, rc_arena *arena)
{
    rc_array_wgsl_index buffers = rc_array_wgsl_index_make(4, arena);

    for (uint32_t i = 0; i < w->globals.num; i++) {
        uint32_t decl = rc_array_wgsl_index_get(&w->globals, i);
        rc_wgsl_space space = (rc_wgsl_space)rc_glsl_sema(w, decl).space;

        if (rc_wgsl_ast_type(w->module.ast, decl) == RC_WGSL_NODE_TYPE_VAR
            && (space == RC_WGSL_SPACE_UNIFORM || space == RC_WGSL_SPACE_STORAGE)) {
            rc_array_wgsl_index_push(&buffers, decl, arena);
        }
    }
    return buffers.view;
}


// Mark the module-scope declarations, so names that reach them can be told from local ones.
static void rc_glsl_mark_globals(rc_glsl_writer *w)
{
    rc_view_wgsl_index decls = rc_wgsl_ast_list(w->module.ast, w->module.ast.decls);

    for (uint32_t i = 0; i < decls.num; i++) {
        rc_glsl_mark(w, rc_view_wgsl_index_get(decls, i), RC_GLSL_MARK_GLOBAL);
    }
}


rc_glsl_result rc_glsl_write(rc_wgsl_module module, uint32_t entry, rc_glsl_dialect dialect,
    rc_arena *arena)
{
    rc_glsl_writer w = {
        .module = module,
        .types = rc_array_wgsl_typeinfo_from_view(module.types, module.types.num + 16, arena),
        .dialect = dialect,
        .entry = entry,
        .marks = rc_span_glsl_mark_make_zero(module.ast.nodes.num, arena),
        .structs = rc_array_wgsl_index_make(8, arena),
        .globals = rc_array_wgsl_index_make(16, arena),
        .functions = rc_array_wgsl_index_make(8, arena),
        .samplers = rc_array_glsl_pair_make(4, arena),
        .polyfills_written = rc_array_glsl_polyfill_written_make(4, arena),
        .polyfills = rc_mstr_make(256, arena),
        .code = rc_mstr_make(4096, arena)
    };
    rc_mstr head = rc_mstr_make(1024, arena);

    // Gather, then write the declarations, globals, functions and main
    rc_glsl_mark_globals(&w);
    rc_glsl_gather_function(&w, entry, arena);
    rc_glsl_write_header(&w, &head, arena);
    rc_glsl_write_structs(&w, &head, arena);
    rc_glsl_write_io(&w, &head, arena);
    rc_glsl_write_resources(&w, &head, arena);
    rc_glsl_write_globals(&w, arena);
    for (uint32_t i = 0; i < w.functions.num && !rc_glsl_failed(&w); i++) {
        rc_glsl_write_function(&w, rc_array_wgsl_index_get(&w.functions, i), arena);
    }
    rc_glsl_write_main(&w, arena);
    if (rc_glsl_failed(&w)) {
        return (rc_glsl_result) {.error = w.error};
    }

    // The polyfills go between the declarations and the code that calls them
    rc_mstr_append(&head, w.polyfills.view, arena);
    rc_mstr_append(&head, w.code.view, arena);

    return (rc_glsl_result) {
        .code = head.view,
        .samplers = w.samplers.view,
        .buffers = rc_glsl_buffers(&w, arena)
    };
}
