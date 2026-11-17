// glsl.c
//
// The GLSL writer's driver: the declarations around an entry point's functions, its inputs and
// outputs, and main.
//
// Module-scope vars and consts are declared without values and given them at the start of main.
// That is when WGSL gives them too, once per invocation, and it lets their values use anything,
// where a GLSL initialiser must be a constant expression without calls.

#include "glsl_writer.h"

#include "layout.h"

// ---- names of resources ----

// Append "G_B" for the binding of the resource var.
static void rc_glsl_append_binding(rc_mstr *out, const rc_writer *w, uint32_t var, rc_arena *arena)
{
    rc_wgsl_binding binding = rc_wgsl_binding_of(w->module.ast, w->module.sema, var);

    rc_mstr_append_u32(out, binding.group, arena);
    rc_mstr_append_char(out, '_', arena);
    rc_mstr_append_u32(out, binding.binding, arena);
}


void rc_glsl_append_sampler_name(rc_mstr *out, const rc_writer *w, rc_wgsl_texture_pair pair, rc_arena *arena)
{
    rc_mstr_append(out, RC_STR("rc_t"), arena);
    rc_glsl_append_binding(out, w, pair.texture, arena);
    if (pair.sampler != 0) {
        rc_mstr_append(out, RC_STR("_s"), arena);
        rc_glsl_append_binding(out, w, pair.sampler, arena);
    }
}


// Append the name of the block for the buffer var, as rc_ubo_0_1 or rc_ssbo_0_1.
static void rc_glsl_append_block_name(rc_mstr *out, const rc_writer *w, uint32_t var, rc_arena *arena)
{
    bool uniform = rc_writer_sema(w, var).space == RC_WGSL_SPACE_UNIFORM;

    rc_mstr_append(out, uniform ? RC_STR("rc_ubo_") : RC_STR("rc_ssbo_"), arena);
    rc_glsl_append_binding(out, w, var, arena);
}

// ---- declarations ----

// Append the version and default precisions.
static void rc_glsl_write_header(rc_writer *w, rc_mstr *out, rc_arena *arena)
{
    if (w->target == RC_SHADER_TARGET_GLSL_ES300) {
        rc_mstr_append(out, RC_STR("#version 300 es\nprecision highp float;\nprecision highp int;\n"), arena);
    }
    else {
        rc_mstr_append(out, RC_STR("#version 460\n"), arena);
    }
}


// The type of the last field of the struct decl.
static uint32_t rc_glsl_last_field_type(const rc_writer *w, uint32_t decl)
{
    rc_view_wgsl_index fields = rc_wgsl_ast_struct_fields(w->module.ast, decl);

    return rc_writer_sema(w, rc_view_wgsl_index_get(fields, fields.num - 1)).type;
}


// True if a struct holds an array sized at run time, which only a storage block can.
static bool rc_glsl_is_runtime_sized(const rc_writer *w, uint32_t type)
{
    rc_wgsl_typeinfo t = rc_writer_typeinfo(w, type);

    if (t.type == RC_WGSL_TYPEINFO_TYPE_ARRAY) {
        return t.count == 0;
    }

    if (t.type != RC_WGSL_TYPEINFO_TYPE_STRUCT) {
        return false;
    }

    return rc_glsl_is_runtime_sized(w, rc_glsl_last_field_type(w, t.decl));
}


// Append a struct's fields as declarations, one to a line, at indent.
static void rc_glsl_write_fields(rc_writer *w, rc_mstr *out, uint32_t decl, rc_arena *arena)
{
    rc_view_wgsl_index fields = rc_wgsl_ast_struct_fields(w->module.ast, decl);

    for (uint32_t i = 0; i < fields.num; i++) {
        uint32_t field = rc_view_wgsl_index_get(fields, i);

        rc_mstr_append(out, RC_STR("    "), arena);
        rc_glsl_write_declaration(w, out, rc_writer_sema(w, field).type, rc_wgsl_ast_text(w->module.ast, field), field, arena);
        rc_mstr_append(out, RC_STR(";\n"), arena);
    }
}


// Append every struct reached, except those sized at run time, which become storage blocks.
static void rc_glsl_write_structs(rc_writer *w, rc_mstr *out, rc_arena *arena)
{
    for (uint32_t i = 0; i < w->entry.structs.num; i++) {
        uint32_t decl = rc_view_wgsl_index_get(w->entry.structs, i);

        if (rc_glsl_is_runtime_sized(w, rc_writer_sema(w, decl).type)) {
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

// The start of the name of an input or output at place: vertex attribute, fragment output, or a
// variable passed between the stages.
static rc_str rc_glsl_io_prefix(rc_wgsl_io_place place)
{
    bool vertex = place.stage == RC_SHADER_STAGE_VERTEX;

    if (vertex && place.input) {
        return RC_STR("rc_attr_");
    }

    if (!vertex && !place.input) {
        return RC_STR("rc_frag_");
    }

    return RC_STR("rc_var_");
}


// Append the name of the variable for the input or output at location, at place.
static void rc_glsl_append_io_name(rc_mstr *out, uint32_t location, rc_wgsl_io_place place, rc_arena *arena)
{
    rc_mstr_append(out, rc_glsl_io_prefix(place), arena);
    rc_mstr_append_u32(out, location, arena);
}


// Append the qualifiers for io's interpolation, when it passes between stages.
static void rc_glsl_write_interpolation(rc_writer *w, rc_mstr *out, rc_wgsl_io io, uint32_t node, rc_arena *arena)
{
    bool es = w->target == RC_SHADER_TARGET_GLSL_ES300;

    if (io.interpolation == RC_WGSL_INTERPOLATE_FLAT) {
        rc_mstr_append(out, RC_STR("flat "), arena);
    }
    else if (io.interpolation == RC_WGSL_INTERPOLATE_LINEAR && es) {
        rc_writer_fail_target(w, node, RC_STR("@interpolate(linear)"), arena);
    }
    else if (io.interpolation == RC_WGSL_INTERPOLATE_LINEAR) {
        rc_mstr_append(out, RC_STR("noperspective "), arena);
    }

    if (io.sampling == RC_WGSL_SAMPLING_CENTROID) {
        rc_mstr_append(out, RC_STR("centroid "), arena);
    }
    else if (io.sampling == RC_WGSL_SAMPLING_SAMPLE && es) {
        rc_writer_fail_target(w, node, RC_STR("@interpolate(..., sample)"), arena);
    }
    else if (io.sampling == RC_WGSL_SAMPLING_SAMPLE) {
        rc_mstr_append(out, RC_STR("sample "), arena);
    }
}


// True if io is the position built-in, marked invariant.
static bool rc_glsl_is_invariant_position(rc_wgsl_io io)
{
    return io.builtin_value == RC_WGSL_BUILTIN_VALUE_POSITION && io.invariant;
}


// Append the declaration of one input or output at a location, or of gl_Position as invariant.
static void rc_glsl_declare_io(rc_writer *w, rc_mstr *out, rc_wgsl_io_slot slot, rc_wgsl_io_place place, rc_arena *arena)
{
    if (rc_glsl_is_invariant_position(slot.io) && !place.input) {
        rc_mstr_append(out, RC_STR("invariant gl_Position;\n"), arena);
    }

    if (slot.io.location == RC_INDEX_NONE) {
        return;
    }

    if (!rc_wgsl_is_inter_stage(place)) {
        rc_mstr_append(out, RC_STR("layout(location = "), arena);
        rc_mstr_append_u32(out, slot.io.location, arena);
        rc_mstr_append(out, RC_STR(") "), arena);
    }
    else {
        rc_glsl_write_interpolation(w, out, slot.io, slot.node, arena);
    }

    rc_mstr_append(out, place.input ? RC_STR("in ") : RC_STR("out "), arena);
    rc_glsl_write_type(w, out, slot.type, slot.node, arena);
    rc_mstr_append_char(out, ' ', arena);
    rc_glsl_append_io_name(out, slot.io.location, place, arena);
    rc_mstr_append(out, RC_STR(";\n"), arena);
}


// Append the declarations of the entry point's inputs and outputs.
static void rc_glsl_write_io(rc_writer *w, rc_mstr *out, rc_arena *arena)
{
    rc_wgsl_io_place inputs = {.stage = w->entry.stage, .input = true};
    rc_wgsl_io_place outputs = {.stage = w->entry.stage};

    for (uint32_t i = 0; i < w->entry.inputs.num; i++) {
        rc_glsl_declare_io(w, out, rc_view_wgsl_io_slot_get(w->entry.inputs, i), inputs, arena);
    }
    for (uint32_t i = 0; i < w->entry.outputs.num; i++) {
        rc_glsl_declare_io(w, out, rc_view_wgsl_io_slot_get(w->entry.outputs, i), outputs, arena);
    }
}

// ---- buffers and samplers ----

// Append the name of the block or combined sampler slot finds, and note the slot with that name.
static void rc_glsl_add_slot(rc_writer *w, rc_mstr *out, rc_writer_slot slot, rc_arena *arena)
{
    rc_mstr name = rc_mstr_make(16, arena);

    if (rc_writer_sema(w, slot.var).space == RC_WGSL_SPACE_HANDLE) {
        rc_glsl_append_sampler_name(&name, w, (rc_wgsl_texture_pair) {.texture = slot.var, .sampler = slot.sampler}, arena);
    }
    else {
        rc_glsl_append_block_name(&name, w, slot.var, arena);
    }

    rc_mstr_append(out, name.view, arena);
    rc_writer_add_slot(w, (rc_writer_slot) {
        .name = name.view,
        .var = slot.var,
        .sampler = slot.sampler
    }, arena);
}


// What to say when a buffer lays out differently in GLSL: under std140 if uniform, else std430.
static rc_str rc_glsl_layout_message(bool uniform)
{
    if (uniform) {
        return RC_STR("GLSL's std140 lays this out differently: in a uniform buffer, avoid matrices with two rows, and use "
            "@align and @size only where std140 agrees");
    }

    return RC_STR("GLSL's std430 lays this out differently: use @align and @size only where std430 agrees");
}


// Fail unless a buffer of type lays out the same in GLSL as in WGSL, blaming var if no field.
static void rc_glsl_check_layout(rc_writer *w, uint32_t var, uint32_t type, bool uniform)
{
    rc_wgsl_layout_rule rule = uniform ? RC_WGSL_LAYOUT_RULE_STD140 : RC_WGSL_LAYOUT_RULE_STD430;
    uint32_t mismatch = rc_wgsl_layout_mismatch(w->module, type, rule);

    if (mismatch != 0) {
        rc_writer_fail(w, mismatch == RC_INDEX_NONE ? var : mismatch, rc_glsl_layout_message(uniform));
    }
}


// Append a declaration in a storage block: an array sized at run time is written name[].
static void rc_glsl_write_block_member(rc_writer *w, rc_mstr *out, uint32_t type, rc_str name, uint32_t node, rc_arena *arena)
{
    rc_wgsl_typeinfo t = rc_writer_typeinfo(w, type);

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


// Append each field of the struct decl as a member of a storage block.
static void rc_glsl_write_block_fields(rc_writer *w, rc_mstr *out, uint32_t decl, rc_arena *arena)
{
    rc_view_wgsl_index fields = rc_wgsl_ast_struct_fields(w->module.ast, decl);

    for (uint32_t i = 0; i < fields.num; i++) {
        uint32_t field = rc_view_wgsl_index_get(fields, i);

        rc_glsl_write_block_member(w, out, rc_writer_sema(w, field).type, rc_wgsl_ast_text(w->module.ast, field), field, arena);
    }
}


// Append the block for one uniform or storage var. A struct sized at run time spreads its members
// across the block, under the var's name, so var.member still reaches them.
static void rc_glsl_write_buffer(rc_writer *w, rc_mstr *out, uint32_t var, rc_arena *arena)
{
    rc_wgsl_sema sema = rc_writer_sema(w, var);
    bool uniform = sema.space == RC_WGSL_SPACE_UNIFORM;
    rc_str name = rc_wgsl_ast_text(w->module.ast, var);
    bool spread = rc_writer_typeinfo(w, sema.type).type == RC_WGSL_TYPEINFO_TYPE_STRUCT && rc_glsl_is_runtime_sized(w, sema.type);

    if (!uniform && w->target == RC_SHADER_TARGET_GLSL_ES300) {
        rc_writer_fail_target(w, var, RC_STR("storage buffers"), arena);
        return;
    }

    rc_glsl_check_layout(w, var, sema.type, uniform);
    rc_mstr_append(out, uniform ? RC_STR("layout(std140) uniform ") : RC_STR("layout(std430) "), arena);
    if (!uniform) {
        rc_mstr_append(out, sema.access == RC_WGSL_ACCESS_READ ? RC_STR("readonly buffer ") : RC_STR("buffer "), arena);
    }

    rc_glsl_add_slot(w, out, (rc_writer_slot) {.var = var}, arena);
    rc_mstr_append(out, RC_STR(" {\n"), arena);
    if (spread) {
        rc_glsl_write_block_fields(w, out, rc_writer_typeinfo(w, sema.type).decl, arena);
        rc_mstr_append(out, RC_STR("} "), arena);
        rc_glsl_append_name(out, name, arena);
        rc_mstr_append(out, RC_STR(";\n"), arena);
        return;
    }

    rc_glsl_write_block_member(w, out, sema.type, name, var, arena);
    rc_mstr_append(out, RC_STR("};\n"), arena);
}


// True if GLSL ES 3.00 has no sampler for the texture: one multisampled, 1D or a cube array.
static bool rc_glsl_es_lacks_sampler(rc_wgsl_typeinfo texture)
{
    if (rc_wgsl_texture_is_multisampled(texture)) {
        return true;
    }

    return texture.dim == RC_SHADER_DIMENSION_1D || texture.dim == RC_SHADER_DIMENSION_CUBE_ARRAY;
}


// Append the GLSL sampler type of a combined sampler, as sampler2DShadow or usampler3D.
static void rc_glsl_write_sampler_type(rc_writer *w, rc_mstr *out, rc_wgsl_texture_pair pair, rc_arena *arena)
{
    static const rc_str dims[] = {
        [RC_SHADER_DIMENSION_NONE] = RC_STR_INIT(""),
        [RC_SHADER_DIMENSION_1D] = RC_STR_INIT("1D"),
        [RC_SHADER_DIMENSION_2D] = RC_STR_INIT("2D"),
        [RC_SHADER_DIMENSION_2D_ARRAY] = RC_STR_INIT("2DArray"),
        [RC_SHADER_DIMENSION_3D] = RC_STR_INIT("3D"),
        [RC_SHADER_DIMENSION_CUBE] = RC_STR_INIT("Cube"),
        [RC_SHADER_DIMENSION_CUBE_ARRAY] = RC_STR_INIT("CubeArray")
    };
    rc_wgsl_typeinfo t = rc_writer_typeinfo(w, rc_writer_sema(w, pair.texture).type);
    bool comparison = pair.sampler != 0 && rc_writer_sema(w, pair.sampler).type == RC_WGSL_TYPE_ID_SAMPLER_COMPARISON;
    bool depth = rc_wgsl_texture_is_depth(t);
    bool multisampled = rc_wgsl_texture_is_multisampled(t);

    if (w->target == RC_SHADER_TARGET_GLSL_ES300 && rc_glsl_es_lacks_sampler(t)) {
        rc_writer_fail_target(w, pair.texture, RC_STR("this texture type"), arena);
    }

    if (!depth && t.sampled_type == RC_WGSL_TYPE_ID_I32) {
        rc_mstr_append_char(out, 'i', arena);
    }
    else if (!depth && t.sampled_type == RC_WGSL_TYPE_ID_U32) {
        rc_mstr_append_char(out, 'u', arena);
    }

    rc_mstr_append(out, RC_STR("sampler"), arena);
    RC_ASSERT(t.dim < RC_COUNT(dims));
    rc_mstr_append(out, multisampled ? RC_STR("2DMS") : dims[t.dim], arena);
    if (depth && comparison) {
        rc_mstr_append(out, RC_STR("Shadow"), arena);
    }
}


// Append the declarations of every buffer and combined sampler reached.
static void rc_glsl_write_resources(rc_writer *w, rc_mstr *out, rc_arena *arena)
{
    for (uint32_t i = 0; i < w->entry.resources.num && !rc_writer_failed(w); i++) {
        uint32_t var = rc_view_wgsl_index_get(w->entry.resources, i);

        if (rc_writer_sema(w, var).space != RC_WGSL_SPACE_HANDLE) {
            rc_glsl_write_buffer(w, out, var, arena);
        }
    }
    for (uint32_t i = 0; i < w->entry.pairs.num; i++) {
        rc_wgsl_texture_pair pair = rc_view_wgsl_texture_pair_get(w->entry.pairs, i);

        rc_mstr_append(out, RC_STR("uniform highp "), arena);
        rc_glsl_write_sampler_type(w, out, pair, arena);
        rc_mstr_append_char(out, ' ', arena);
        rc_glsl_add_slot(w, out, (rc_writer_slot) {.var = pair.texture, .sampler = pair.sampler}, arena);
        rc_mstr_append(out, RC_STR(";\n"), arena);
    }
}

// ---- globals and main ----

// True if a reached module-scope declaration is a private var or a concrete const, which become
// GLSL globals given their values in main.
static bool rc_glsl_is_plain_global(const rc_writer *w, uint32_t decl)
{
    rc_wgsl_sema sema = rc_writer_sema(w, decl);

    if (rc_wgsl_ast_type(w->module.ast, decl) == RC_WGSL_NODE_TYPE_CONST) {
        return !rc_wgsl_type_is_abstract(w->types.infos.view, sema.type);
    }

    return sema.space == RC_WGSL_SPACE_PRIVATE;
}


// Write the declarations of the private vars and concrete consts.
static void rc_glsl_write_globals(rc_writer *w, rc_arena *arena)
{
    for (uint32_t i = 0; i < w->entry.globals.num; i++) {
        uint32_t decl = rc_view_wgsl_index_get(w->entry.globals, i);

        if (rc_glsl_is_plain_global(w, decl)) {
            rc_writer_line(w, arena);
            rc_glsl_write_declaration(w, &w->code, rc_writer_sema(w, decl).type, rc_wgsl_ast_text(w->module.ast, decl), decl, arena);
            rc_writer_put(w, RC_STR(";"), arena);
        }
    }
    rc_writer_put(w, RC_STR("\n"), arena);
}


// Write the statements at the top of main giving the globals their values, in order.
static void rc_glsl_write_global_values(rc_writer *w, rc_arena *arena)
{
    for (uint32_t i = 0; i < w->entry.globals.num; i++) {
        uint32_t decl = rc_view_wgsl_index_get(w->entry.globals, i);
        rc_wgsl_node d = rc_wgsl_ast_node(w->module.ast, decl);
        uint32_t value = d.type == RC_WGSL_NODE_TYPE_VAR ? rc_wgsl_ast_var_parts(w->module.ast, decl).initialiser : d.rhs;

        if (!rc_glsl_is_plain_global(w, decl)) {
            continue;
        }

        rc_writer_line(w, arena);
        rc_glsl_append_name(&w->code, rc_wgsl_ast_text(w->module.ast, decl), arena);
        rc_writer_put(w, RC_STR(" = "), arena);
        if (value != 0) {
            rc_glsl_write_expr(w, value, 0, arena);
        }
        else {
            rc_glsl_write_zero(w, &w->code, rc_writer_sema(w, decl).type, decl, arena);
        }

        rc_writer_put(w, RC_STR(";"), arena);
    }
}


// Write the value of an input: its variable, or a GLSL built-in.
static void rc_glsl_write_input(rc_writer *w, rc_wgsl_io_slot slot, rc_arena *arena)
{
    rc_wgsl_io_place place = {.stage = w->entry.stage, .input = true};
    bool desktop = w->target == RC_SHADER_TARGET_GLSL_460;

    switch ((rc_wgsl_builtin_value)slot.io.builtin_value) {
    case RC_WGSL_BUILTIN_VALUE_NONE:
        rc_glsl_append_io_name(&w->code, slot.io.location, place, arena);
        break;
    case RC_WGSL_BUILTIN_VALUE_POSITION:
        rc_writer_put(w, RC_STR("gl_FragCoord"), arena);
        break;
    case RC_WGSL_BUILTIN_VALUE_FRONT_FACING:
        rc_writer_put(w, RC_STR("gl_FrontFacing"), arena);
        break;
    case RC_WGSL_BUILTIN_VALUE_VERTEX_INDEX:
        rc_writer_put(w, RC_STR("uint(gl_VertexID)"), arena);
        break;
    case RC_WGSL_BUILTIN_VALUE_INSTANCE_INDEX:
        rc_writer_put(w, desktop ? RC_STR("uint(gl_InstanceID + gl_BaseInstance)") : RC_STR("uint(gl_InstanceID)"), arena);
        break;
    case RC_WGSL_BUILTIN_VALUE_SAMPLE_INDEX:
    case RC_WGSL_BUILTIN_VALUE_SAMPLE_MASK:
        if (!desktop) {
            rc_writer_fail_target(w, slot.node, RC_STR("this built-in"), arena);
        }

        rc_writer_put(w, slot.io.builtin_value == RC_WGSL_BUILTIN_VALUE_SAMPLE_INDEX
            ? RC_STR("uint(gl_SampleID)") : RC_STR("uint(gl_SampleMaskIn[0])"), arena);
        break;
    default:
        rc_writer_fail(w, slot.node, RC_STR("this input can't be written in GLSL"));
        break;
    }
}


// Write the value passed for parameter i: its input, or its struct built from its members'.
static void rc_glsl_write_argument(rc_writer *w, uint32_t i, rc_arena *arena)
{
    uint32_t param = rc_view_wgsl_index_get(rc_wgsl_ast_params(w->module.ast, w->entry.fn), i);
    bool members = false;

    for (uint32_t j = 0; j < w->entry.inputs.num; j++) {
        rc_wgsl_io_slot slot = rc_view_wgsl_io_slot_get(w->entry.inputs, j);

        if (slot.param != i) {
            continue;
        }

        if (slot.field != 0 && !members) {
            rc_glsl_write_type(w, &w->code, rc_writer_sema(w, param).type, param, arena);
            rc_writer_put(w, RC_STR("("), arena);
        }
        else if (slot.field != 0) {
            rc_writer_put(w, RC_STR(", "), arena);
        }

        members |= slot.field != 0;
        rc_glsl_write_input(w, slot, arena);
    }
    if (members) {
        rc_writer_put(w, RC_STR(")"), arena);
    }
}


// Write a statement storing the result, or the member of it slot names, in its output.
static void rc_glsl_write_output(rc_writer *w, rc_wgsl_io_slot slot, rc_arena *arena)
{
    rc_wgsl_io_place place = {.stage = w->entry.stage};
    rc_mstr value = rc_mstr_from_str(RC_STR("rc_result"), 32, arena);

    if (slot.field != 0) {
        rc_mstr_append_char(&value, '.', arena);
        rc_glsl_append_name(&value, rc_wgsl_ast_text(w->module.ast, slot.field), arena);
    }

    rc_writer_line(w, arena);
    switch ((rc_wgsl_builtin_value)slot.io.builtin_value) {
    case RC_WGSL_BUILTIN_VALUE_NONE:
        rc_glsl_append_io_name(&w->code, slot.io.location, place, arena);
        rc_writer_put(w, RC_STR(" = "), arena);
        break;
    case RC_WGSL_BUILTIN_VALUE_POSITION:
        rc_writer_put(w, RC_STR("gl_Position = "), arena);
        break;
    case RC_WGSL_BUILTIN_VALUE_FRAG_DEPTH:
        rc_writer_put(w, RC_STR("gl_FragDepth = "), arena);
        break;
    default:
        if (w->target == RC_SHADER_TARGET_GLSL_ES300) {
            rc_writer_fail_target(w, slot.node, RC_STR("@builtin(sample_mask)"), arena);
        }

        rc_writer_put(w, RC_STR("gl_SampleMask[0] = int("), arena);
        rc_writer_put(w, value.view, arena);
        rc_writer_put(w, RC_STR(");"), arena);
        return;
    }
    rc_writer_put(w, value.view, arena);
    rc_writer_put(w, RC_STR(";"), arena);
}


// Write main: give the globals their values, call the entry point with its inputs, and store its
// result in its outputs.
static void rc_glsl_write_main(rc_writer *w, rc_arena *arena)
{
    uint32_t params = rc_wgsl_ast_params(w->module.ast, w->entry.fn).num;
    uint32_t type = rc_writer_sema(w, w->entry.fn).type;

    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("void main() {"), arena);
    w->indent++;
    rc_glsl_write_global_values(w, arena);
    rc_writer_line(w, arena);
    if (type != 0) {
        rc_glsl_write_type(w, &w->code, type, w->entry.fn, arena);
        rc_writer_put(w, RC_STR(" rc_result = "), arena);
    }

    rc_glsl_append_name(&w->code, rc_wgsl_ast_text(w->module.ast, w->entry.fn), arena);
    rc_writer_put(w, RC_STR("("), arena);
    for (uint32_t i = 0; i < params; i++) {
        rc_writer_put(w, i > 0 ? RC_STR(", ") : RC_STR(""), arena);
        rc_glsl_write_argument(w, i, arena);
    }
    rc_writer_put(w, RC_STR(");"), arena);
    for (uint32_t i = 0; i < w->entry.outputs.num; i++) {
        rc_glsl_write_output(w, rc_view_wgsl_io_slot_get(w->entry.outputs, i), arena);
    }

    // WebGPU's clip-space depth runs from 0 to 1, and GL ES's from -1 to 1
    if (w->target == RC_SHADER_TARGET_GLSL_ES300 && w->entry.stage == RC_SHADER_STAGE_VERTEX) {
        rc_writer_line(w, arena);
        rc_writer_put(w, RC_STR("gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;"), arena);
    }

    w->indent--;
    rc_writer_line(w, arena);
    rc_writer_put(w, RC_STR("}\n"), arena);
}

// ---- writing ----

rc_writer_result rc_glsl_write(rc_wgsl_module module, rc_wgsl_entry entry, rc_shader_target target, rc_arena *arena)
{
    rc_writer w = rc_writer_make(module, entry, target, arena);
    rc_mstr head = rc_mstr_make(1024, arena);

    RC_ASSERT(target == RC_SHADER_TARGET_GLSL_ES300 || target == RC_SHADER_TARGET_GLSL_460);
    rc_glsl_write_header(&w, &head, arena);
    rc_glsl_write_structs(&w, &head, arena);
    rc_glsl_write_io(&w, &head, arena);
    rc_glsl_write_resources(&w, &head, arena);
    rc_glsl_write_globals(&w, arena);
    for (uint32_t i = 0; i < entry.functions.num && !rc_writer_failed(&w); i++) {
        rc_glsl_write_function(&w, rc_view_wgsl_index_get(entry.functions, i), arena);
    }
    rc_glsl_write_main(&w, arena);
    return rc_writer_finish(&w, head.view);
}
