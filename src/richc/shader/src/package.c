// package.c
//
// Shader packages: translating a WGSL module into one. Everything the translation needs along the
// way comes from scratch, and only the package itself goes in the caller's arena: its arrays and
// strings, and its copy of the source, which the entry points' names point into.

#include "richc/shader/wgsl.h"

#include "richc/ops.h"

#include "glsl.h"
#include "layout.h"
#include "parser.h"

#define RC_ARRAY_TYPE uint16_t
#define RC_ARRAY_NAME package_index
#include "richc/array.template.h"

// ---- bindings ----

// The rc_shader_binding_type of a resource var.
static rc_shader_binding_type rc_package_binding_type(rc_wgsl_module module, uint32_t var)
{
    rc_wgsl_sema sema = rc_wgsl_module_sema(module, var);

    switch ((rc_wgsl_space)sema.space) {
    case RC_WGSL_SPACE_UNIFORM:
        return RC_SHADER_BINDING_TYPE_UNIFORM_BUFFER;
    case RC_WGSL_SPACE_STORAGE:
        return sema.access == RC_WGSL_ACCESS_READ ? RC_SHADER_BINDING_TYPE_READ_ONLY_STORAGE_BUFFER : RC_SHADER_BINDING_TYPE_STORAGE_BUFFER;
    default:
        break;
    }
    switch (rc_wgsl_typeinfo_type_of(module.types, sema.type)) {
    case RC_WGSL_TYPEINFO_TYPE_SAMPLER:
        return RC_SHADER_BINDING_TYPE_SAMPLER;
    case RC_WGSL_TYPEINFO_TYPE_SAMPLER_COMPARISON:
        return RC_SHADER_BINDING_TYPE_COMPARISON_SAMPLER;
    default:
        return RC_SHADER_BINDING_TYPE_TEXTURE;
    }
}


// The rc_shader_sample of a texture type.
static rc_shader_sample rc_package_sample_type(rc_wgsl_typeinfo texture)
{
    if (rc_wgsl_texture_is_depth(texture)) {
        return RC_SHADER_SAMPLE_DEPTH;
    }

    if (texture.sampled_type == RC_WGSL_TYPE_ID_I32) {
        return RC_SHADER_SAMPLE_SINT;
    }

    return texture.sampled_type == RC_WGSL_TYPE_ID_U32 ? RC_SHADER_SAMPLE_UINT : RC_SHADER_SAMPLE_FLOAT;
}


// The binding a resource var describes, used by no stage yet.
static rc_shader_binding rc_package_binding(rc_wgsl_module module, uint32_t var)
{
    rc_wgsl_sema sema = rc_wgsl_module_sema(module, var);
    rc_wgsl_binding binding = rc_wgsl_binding_of(module.ast, module.sema, var);
    rc_shader_binding_type type = rc_package_binding_type(module, var);
    rc_wgsl_typeinfo t = rc_wgsl_typeinfo_of(module.types, sema.type);
    bool buffer = sema.space == RC_WGSL_SPACE_UNIFORM || sema.space == RC_WGSL_SPACE_STORAGE;
    bool texture = type == RC_SHADER_BINDING_TYPE_TEXTURE;

    // The checker holds group and binding to WebGPU's limits, which fit the package's fields
    RC_ASSERT(binding.group < RC_WGSL_MAX_GROUPS && binding.binding < RC_WGSL_MAX_BINDINGS);
    return (rc_shader_binding) {
        .min_size = buffer ? rc_wgsl_layout_of(module, sema.type, RC_WGSL_LAYOUT_RULE_WGSL).size : 0,
        .binding = (uint16_t)binding.binding,
        .group = (uint8_t)binding.group,
        .type = (uint8_t)type,
        .dimension = texture ? t.dim : 0,
        .sample_type = texture ? (uint8_t)rc_package_sample_type(t) : 0,
        .multisampled = texture && rc_wgsl_texture_is_multisampled(t)
    };
}


// The module-scope vars that are resources, in the order the module declares them.
static rc_view_wgsl_index rc_package_resources(rc_wgsl_module module, rc_arena *arena)
{
    rc_view_wgsl_index decls = rc_wgsl_ast_list(module.ast, module.ast.decls);
    rc_array_wgsl_index resources = rc_array_wgsl_index_make(8, arena);

    for (uint32_t i = 0; i < decls.num; i++) {
        uint32_t decl = rc_view_wgsl_index_get(decls, i);
        rc_wgsl_space space = (rc_wgsl_space)rc_wgsl_module_sema(module, decl).space;

        if (rc_wgsl_ast_type(module.ast, decl) == RC_WGSL_NODE_TYPE_VAR && rc_wgsl_is_resource_space(space)) {
            rc_array_wgsl_index_push(&resources, decl, arena);
        }
    }
    return resources.view;
}


// The index among resources of each node that is one, and RC_SHADER_NONE for the rest.
static rc_view_package_index rc_package_indices(rc_wgsl_module module, rc_view_wgsl_index resources, rc_arena *arena)
{
    rc_span_package_index indices = rc_span_package_index_make(module.ast.nodes.num, arena);

    for (uint32_t i = 0; i < indices.num; i++) {
        rc_span_package_index_set(indices, i, RC_SHADER_NONE);
    }
    for (uint32_t i = 0; i < resources.num; i++) {
        rc_span_package_index_set(indices, rc_view_wgsl_index_get(resources, i), (uint16_t)i);
    }
    return indices.view;
}

// ---- entry points ----

// The rc_shader_format of a type at a location: a numeric scalar or vector.
static rc_shader_format rc_package_format(rc_wgsl_module module, uint32_t type)
{
    static const uint8_t formats[][4] = {
        {RC_SHADER_FORMAT_FLOAT32, RC_SHADER_FORMAT_FLOAT32X2, RC_SHADER_FORMAT_FLOAT32X3,
            RC_SHADER_FORMAT_FLOAT32X4},
        {RC_SHADER_FORMAT_SINT32, RC_SHADER_FORMAT_SINT32X2, RC_SHADER_FORMAT_SINT32X3,
            RC_SHADER_FORMAT_SINT32X4},
        {RC_SHADER_FORMAT_UINT32, RC_SHADER_FORMAT_UINT32X2, RC_SHADER_FORMAT_UINT32X3,
            RC_SHADER_FORMAT_UINT32X4}
    };
    uint32_t scalar = rc_wgsl_type_scalar_of(module.types, type);
    uint32_t width = rc_wgsl_type_width_of(module.types, type);
    uint32_t row = scalar == RC_WGSL_TYPE_ID_I32 ? 1 : scalar == RC_WGSL_TYPE_ID_U32 ? 2 : 0;

    RC_ASSERT(width >= 1 && width <= 4);
    return (rc_shader_format)formats[row][width - 1];
}


// The locations among an entry point's inputs or outputs, and their types.
static rc_view_shader_location rc_package_locations(rc_wgsl_module module, rc_view_wgsl_io_slot slots, rc_arena *arena)
{
    rc_array_shader_location locations = rc_array_shader_location_make(slots.num, arena);

    for (uint32_t i = 0; i < slots.num; i++) {
        rc_wgsl_io_slot slot = rc_view_wgsl_io_slot_get(slots, i);
        rc_shader_location location = {
            .location = (uint8_t)slot.io.location,
            .format = (uint8_t)rc_package_format(module, slot.type)
        };

        // The checker holds locations to WebGPU's limit, which fits the package's field
        if (slot.io.location != RC_INDEX_NONE) {
            RC_ASSERT(slot.io.location < RC_WGSL_MAX_LOCATIONS);
            rc_array_shader_location_push(&locations, location, arena);
        }
    }
    return locations.view;
}


// error, with its message copied into arena.
static rc_wgsl_error rc_package_keep_error(rc_wgsl_error error, rc_arena *arena)
{
    error.message = rc_mstr_from_str(error.message, 0, arena).view;
    return error;
}


// The program for entry in target, written by the target's writer.
static rc_writer_result rc_package_write(rc_wgsl_module module, rc_wgsl_entry entry, rc_shader_target target, rc_arena *arena)
{
    switch (target) {
    case RC_SHADER_TARGET_GLSL_ES300:
    case RC_SHADER_TARGET_GLSL_460:
        return rc_glsl_write(module, entry, target, arena);
    default:
        RC_UNREACHABLE();
        return (rc_writer_result) {0};
    }
}


// The slots a program finds its bindings in, from those its writer gives, copied into arena.
static rc_view_shader_slot rc_package_slots(rc_view_writer_slot written, rc_view_package_index indices, rc_arena *arena)
{
    rc_span_shader_slot slots = rc_span_shader_slot_make(written.num, arena);

    for (uint32_t i = 0; i < written.num; i++) {
        rc_writer_slot slot = rc_view_writer_slot_get(written, i);

        rc_span_shader_slot_set(slots, i, (rc_shader_slot) {
            .name = rc_mstr_from_str(slot.name, 0, arena).view,
            .binding = rc_view_package_index_get(indices, slot.var),
            .sampler = slot.sampler != 0 ? rc_view_package_index_get(indices, slot.sampler) : RC_SHADER_NONE
        });
    }
    return slots.view;
}


// One program, or why it can't be written.
typedef struct rc_package_program_result {
    rc_shader_program program;
    rc_wgsl_error error;
} rc_package_program_result;

// The code of a program written: its declarations, polyfills and the rest, copied into arena.
static rc_view_bytes rc_package_code(rc_writer_result written, rc_arena *arena)
{
    rc_mstr code = rc_mstr_make(written.declarations.len + written.polyfills.len + written.code.len, arena);

    rc_mstr_append(&code, written.declarations, arena);
    rc_mstr_append(&code, written.polyfills, arena);
    rc_mstr_append(&code, written.code, arena);
    return rc_view_bytes_from_ptr((const uint8_t *)code.data, code.len);
}


// The program written for target, copied into arena.
static rc_shader_program rc_package_keep_program(rc_writer_result written, rc_shader_target target, rc_view_package_index indices,
    rc_arena *arena)
{
    rc_view_bytes code = rc_package_code(written, arena);
    rc_view_shader_slot slots = rc_package_slots(written.slots, indices, arena);

    return (rc_shader_program) {
        .code = code,
        .slots = slots,
        .target = (uint8_t)target
    };
}


// The program for entry in target, made in scratch and kept in arena.
static rc_package_program_result rc_package_program(rc_wgsl_module module, rc_wgsl_entry entry,
    rc_shader_target target, rc_view_package_index indices, rc_arena *arena, rc_arena scratch)
{
    rc_writer_result written = rc_package_write(module, entry, target, &scratch);

    if (rc_wgsl_error_is_set(written.error)) {
        return (rc_package_program_result) {.error = rc_package_keep_error(written.error, arena)};
    }

    return (rc_package_program_result) {.program = rc_package_keep_program(written, target, indices, arena)};
}


// One entry point, or why it can't be translated.
typedef struct rc_package_entry_result {
    rc_shader_entry_point entry_point;
    rc_wgsl_error error;
} rc_package_entry_result;

// Translate entry for each target in targets.
static rc_package_entry_result rc_package_entry_point(rc_wgsl_module module, rc_wgsl_entry entry,
    uint32_t targets, rc_view_package_index indices, rc_arena *arena, rc_arena scratch)
{
    rc_array_shader_program programs = rc_array_shader_program_make(2, arena);

    // Each bit set in rest is a target still to write, taken lowest first
    for (uint32_t rest = targets; rest != 0; rest &= rest - 1) {
        rc_shader_target target = (rc_shader_target)rc_ctz_u32(rest);
        rc_package_program_result made = rc_package_program(module, entry, target, indices, arena, scratch);

        if (rc_wgsl_error_is_set(made.error)) {
            return (rc_package_entry_result) {.error = made.error};
        }

        rc_array_shader_program_push(&programs, made.program, arena);
    }

    return (rc_package_entry_result) {
        .entry_point = {
            .name = rc_wgsl_ast_text(module.ast, entry.fn),
            .programs = programs.view,
            .inputs = entry.stage == RC_SHADER_STAGE_VERTEX
                ? rc_package_locations(module, entry.inputs, arena)
                : (rc_view_shader_location) {0},
            .outputs = entry.stage == RC_SHADER_STAGE_FRAGMENT
                ? rc_package_locations(module, entry.outputs, arena)
                : (rc_view_shader_location) {0},
            .stage = entry.stage
        }
    };
}


// Note in bindings that entry's stage uses each resource it reaches.
static void rc_package_note_stage(rc_span_shader_binding bindings, rc_wgsl_entry entry, rc_view_package_index indices)
{
    for (uint32_t i = 0; i < entry.resources.num; i++) {
        uint16_t index = rc_view_package_index_get(indices, rc_view_wgsl_index_get(entry.resources, i));

        rc_span_shader_binding_at(bindings, index)->stages |= (uint8_t)(1u << entry.stage);
    }
}


// The checked module for the tokens lexed from source, or the error that stopped it.
static rc_wgsl_check_result rc_package_parse_and_check(rc_str source, rc_view_wgsl_token tokens, rc_arena *arena)
{
    rc_wgsl_parse_result parsed = rc_wgsl_parse(source, tokens, arena);

    if (rc_wgsl_error_is_set(parsed.error)) {
        return (rc_wgsl_check_result) {.error = parsed.error};
    }

    return rc_wgsl_check(parsed.ast, arena);
}


// The checked module for source, or the error that stopped it.
static rc_wgsl_check_result rc_package_check(rc_str source, rc_arena *arena)
{
    rc_wgsl_lex_result lexed = rc_wgsl_lex(source, arena);

    if (rc_wgsl_error_is_set(lexed.error)) {
        return (rc_wgsl_check_result) {.error = lexed.error};
    }

    return rc_package_parse_and_check(source, lexed.tokens, arena);
}


// The bindings of the resources, used by no stage yet, in arena.
static rc_span_shader_binding rc_package_bindings(rc_wgsl_module module, rc_view_wgsl_index resources, rc_arena *arena)
{
    rc_span_shader_binding bindings = rc_span_shader_binding_make(resources.num, arena);

    for (uint32_t i = 0; i < resources.num; i++) {
        rc_span_shader_binding_set(bindings, i, rc_package_binding(module, rc_view_wgsl_index_get(resources, i)));
    }
    return bindings;
}


// The package for the checked module, whose source is own, with each entry point translated for
// each of targets.
static rc_wgsl_translate_result rc_package_make(rc_str own, rc_wgsl_module module, uint32_t targets, rc_arena *arena, rc_arena scratch)
{
    rc_view_wgsl_index resources = rc_package_resources(module, &scratch);
    rc_view_package_index indices = rc_package_indices(module, resources, &scratch);
    rc_span_shader_binding bindings = rc_package_bindings(module, resources, arena);
    rc_span_shader_entry_point entry_points = rc_span_shader_entry_point_make(module.entries.num, arena);

    for (uint32_t i = 0; i < module.entries.num; i++) {
        rc_wgsl_entry entry = rc_view_wgsl_entry_get(module.entries, i);
        rc_package_entry_result made = rc_package_entry_point(module, entry, targets, indices, arena, scratch);

        if (rc_wgsl_error_is_set(made.error)) {
            return (rc_wgsl_translate_result) {.error = made.error};
        }

        rc_span_shader_entry_point_set(entry_points, i, made.entry_point);
        rc_package_note_stage(bindings, entry, indices);
    }

    return (rc_wgsl_translate_result) {
        .package = {
            .wgsl = own,
            .bindings = bindings.view,
            .entry_points = entry_points.view,
            .targets = targets
        }
    };
}


rc_wgsl_translate_result rc_wgsl_translate(rc_str source, uint32_t targets, rc_arena *arena, rc_arena scratch)
{
    // The package keeps its own copy of the source, which the entry points' names point into
    rc_str own = rc_mstr_from_str(source, 0, arena).view;
    rc_wgsl_check_result checked = rc_package_check(own, &scratch);

    RC_ASSERT(!rc_arena_is_same(arena, &scratch));
    RC_ASSERT(!(targets & RC_SHADER_TARGET_BIT(NONE)) && targets < RC_SHADER_TARGET_BIT(GLSL_460) << 1);
    if (rc_wgsl_error_is_set(checked.error)) {
        return (rc_wgsl_translate_result) {.error = rc_package_keep_error(checked.error, arena)};
    }

    return rc_package_make(own, checked.module, targets, arena, scratch);
}
