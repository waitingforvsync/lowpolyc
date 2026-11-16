// package.c
//
// Shader packages: translating a WGSL module into one, and writing one out as a C header.

#include "richc/shader/wgsl.h"

#include "check.h"
#include "glsl.h"
#include "layout.h"
#include "parser.h"
#include "richc/mstr.h"

// ---- bindings ----

// The rc_shader_binding_type of a resource var.
static rc_shader_binding_type rc_package_binding_type(rc_wgsl_module module, uint32_t var)
{
    rc_wgsl_sema sema = rc_wgsl_module_sema(module, var);

    switch ((rc_wgsl_space)sema.space) {
    case RC_WGSL_SPACE_UNIFORM:
        return RC_SHADER_BINDING_TYPE_UNIFORM_BUFFER;
    case RC_WGSL_SPACE_STORAGE:
        return sema.access == RC_WGSL_ACCESS_READ ? RC_SHADER_BINDING_TYPE_READ_ONLY_STORAGE_BUFFER
                                                  : RC_SHADER_BINDING_TYPE_STORAGE_BUFFER;
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
    if (texture.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH
        || texture.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH_MULTISAMPLED) {
        return RC_SHADER_SAMPLE_DEPTH;
    }
    if (texture.element == RC_WGSL_TYPE_ID_I32) {
        return RC_SHADER_SAMPLE_SINT;
    }
    return texture.element == RC_WGSL_TYPE_ID_U32 ? RC_SHADER_SAMPLE_UINT : RC_SHADER_SAMPLE_FLOAT;
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

    // The rc_wgsl_dim values match rc_shader_dimension's
    return (rc_shader_binding) {
        .min_size = buffer ? rc_wgsl_layout_of(module, sema.type, RC_WGSL_LAYOUT_RULE_WGSL).size : 0,
        .binding = (uint16_t)binding.binding,
        .group = (uint8_t)binding.group,
        .type = (uint8_t)type,
        .dimension = texture ? t.dim : 0,
        .sample_type = texture ? (uint8_t)rc_package_sample_type(t) : 0,
        .multisampled = texture && (t.texture_class == RC_WGSL_TEXTURE_CLASS_MULTISAMPLED
            || t.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH_MULTISAMPLED)
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

        if (rc_wgsl_ast_type(module.ast, decl) == RC_WGSL_NODE_TYPE_VAR
            && (space == RC_WGSL_SPACE_UNIFORM || space == RC_WGSL_SPACE_STORAGE
                || space == RC_WGSL_SPACE_HANDLE)) {
            rc_array_wgsl_index_push(&resources, decl, arena);
        }
    }
    return resources.view;
}


// The index of var among the resources, or RC_SHADER_NONE.
static uint16_t rc_package_index_of(rc_view_wgsl_index resources, uint32_t var)
{
    for (uint32_t i = 0; i < resources.num; i++) {
        if (rc_view_wgsl_index_get(resources, i) == var) {
            return (uint16_t)i;
        }
    }
    return RC_SHADER_NONE;
}

// ---- entry points ----

// The rc_shader_format of a type at a location: a numeric scalar or vector.
static rc_shader_format rc_package_format(rc_wgsl_module module, uint32_t type)
{
    uint32_t scalar = rc_wgsl_type_scalar_of(module.types, type);
    uint32_t width = rc_wgsl_type_width_of(module.types, type);
    rc_wgsl_typeinfo_type scalar_type = rc_wgsl_typeinfo_type_of(module.types, scalar);
    uint32_t base = scalar_type == RC_WGSL_TYPEINFO_TYPE_I32 ? RC_SHADER_FORMAT_SINT32
                  : scalar_type == RC_WGSL_TYPEINFO_TYPE_U32 ? RC_SHADER_FORMAT_UINT32
                  : RC_SHADER_FORMAT_FLOAT32;

    return (rc_shader_format)(base + width - 1);
}


// Push the location of an input or output with attributes and type, or of each member of its
// struct.
static void rc_package_push_locations(rc_wgsl_module module, uint32_t attributes, uint32_t type,
    rc_array_shader_location *locations, rc_arena *arena)
{
    rc_wgsl_io io = rc_wgsl_io_of(module.ast, module.sema, attributes);
    rc_wgsl_typeinfo t = rc_wgsl_typeinfo_of(module.types, type);

    if (io.location != RC_INDEX_NONE) {
        rc_shader_location location = {
            .location = (uint8_t)io.location,
            .format = (uint8_t)rc_package_format(module, type)
        };

        rc_array_shader_location_push(locations, location, arena);
    }
    else if (io.builtin == RC_WGSL_VALUE_NONE && t.type == RC_WGSL_TYPEINFO_TYPE_STRUCT) {
        rc_view_wgsl_index fields = rc_wgsl_ast_list(module.ast,
            rc_wgsl_ast_node(module.ast, t.decl).lhs);

        for (uint32_t i = 0; i < fields.num; i++) {
            uint32_t field = rc_view_wgsl_index_get(fields, i);

            rc_package_push_locations(module, rc_wgsl_ast_node(module.ast, field).lhs,
                rc_wgsl_module_sema(module, field).type, locations, arena);
        }
    }
}


// The locations of an entry point's inputs, or of its outputs.
static rc_view_shader_location rc_package_locations(rc_wgsl_module module, uint32_t entry,
    bool inputs, rc_arena *arena)
{
    rc_wgsl_fn_parts parts = rc_wgsl_ast_fn_parts(module.ast, entry);
    rc_array_shader_location locations = rc_array_shader_location_make(4, arena);

    if (inputs) {
        rc_view_wgsl_index params = rc_wgsl_ast_list(module.ast, parts.params);

        for (uint32_t i = 0; i < params.num; i++) {
            uint32_t param = rc_view_wgsl_index_get(params, i);

            rc_package_push_locations(module, rc_wgsl_ast_node(module.ast, param).lhs,
                rc_wgsl_module_sema(module, param).type, &locations, arena);
        }
    }
    else if (parts.return_type != 0) {
        rc_package_push_locations(module, parts.return_attributes,
            rc_wgsl_module_sema(module, entry).type, &locations, arena);
    }
    return locations.view;
}


// The combined samplers a program declares, with the indices of their bindings.
static rc_view_shader_sampler rc_package_samplers(rc_wgsl_module module, rc_glsl_result program,
    rc_view_wgsl_index resources, rc_arena *arena)
{
    rc_span_shader_sampler samplers = rc_span_shader_sampler_make(program.samplers.num, arena);

    for (uint32_t i = 0; i < program.samplers.num; i++) {
        rc_glsl_pair pair = rc_view_glsl_pair_get(program.samplers, i);
        rc_mstr name = {0};

        rc_glsl_append_sampler_name(&name, module, pair, arena);
        rc_span_shader_sampler_set(samplers, i, (rc_shader_sampler) {
            .name = name.view,
            .texture = rc_package_index_of(resources, pair.texture),
            .sampler = pair.sampler != 0 ? rc_package_index_of(resources, pair.sampler)
                                         : RC_SHADER_NONE
        });
    }
    return samplers.view;
}


// The blocks a program declares, with the indices of their bindings.
static rc_view_shader_block rc_package_blocks(rc_wgsl_module module, rc_glsl_result program,
    rc_view_wgsl_index resources, rc_arena *arena)
{
    rc_span_shader_block blocks = rc_span_shader_block_make(program.buffers.num, arena);

    for (uint32_t i = 0; i < program.buffers.num; i++) {
        uint32_t var = rc_view_wgsl_index_get(program.buffers, i);
        rc_mstr name = {0};

        rc_glsl_append_block_name(&name, module, var, arena);
        rc_span_shader_block_set(blocks, i, (rc_shader_block) {
            .name = name.view,
            .binding = rc_package_index_of(resources, var)
        });
    }
    return blocks.view;
}


// Note in bindings that stage uses each binding an entry point's program declares.
static void rc_package_note_stage(rc_span_shader_binding bindings, rc_shader_entry_point entry)
{
    uint8_t bit = (uint8_t)(1u << entry.stage);

    for (uint32_t i = 0; i < entry.samplers.num; i++) {
        rc_shader_sampler sampler = rc_view_shader_sampler_get(entry.samplers, i);

        rc_span_shader_binding_at(bindings, sampler.texture)->stages |= bit;
        if (sampler.sampler != RC_SHADER_NONE) {
            rc_span_shader_binding_at(bindings, sampler.sampler)->stages |= bit;
        }
    }
    for (uint32_t i = 0; i < entry.blocks.num; i++) {
        rc_span_shader_binding_at(bindings, rc_view_shader_block_get(entry.blocks, i).binding)->stages
            |= bit;
    }
}


// One entry point, or why it can't be translated.
typedef struct rc_package_entry_result {
    rc_shader_entry_point entry_point;
    rc_wgsl_error error;
} rc_package_entry_result;

// Translate the entry point entry, in both dialects of GLSL.
static rc_package_entry_result rc_package_entry_point(rc_wgsl_module module, uint32_t entry,
    rc_view_wgsl_index resources, rc_arena *arena)
{
    rc_glsl_result es = rc_glsl_write(module, entry, RC_GLSL_DIALECT_ES300, arena);
    rc_glsl_result gl = rc_glsl_write(module, entry, RC_GLSL_DIALECT_460, arena);
    rc_shader_stage stage = (rc_shader_stage)rc_wgsl_module_sema(module, entry).stage;

    if (rc_wgsl_error_is_set(es.error) || rc_wgsl_error_is_set(gl.error)) {
        return (rc_package_entry_result) {
            .error = rc_wgsl_error_is_set(es.error) ? es.error : gl.error
        };
    }

    return (rc_package_entry_result) {
        .entry_point = {
            .name = rc_wgsl_ast_text(module.ast, entry),
            .glsl_es = es.code,
            .glsl = gl.code,
            .samplers = rc_package_samplers(module, es, resources, arena),
            .blocks = rc_package_blocks(module, es, resources, arena),
            .inputs = stage == RC_SHADER_STAGE_VERTEX
                ? rc_package_locations(module, entry, true, arena) : (rc_view_shader_location) {0},
            .outputs = stage == RC_SHADER_STAGE_FRAGMENT
                ? rc_package_locations(module, entry, false, arena) : (rc_view_shader_location) {0},
            .stage = (uint8_t)stage
        }
    };
}


// The checked module for source, or the error that stopped it.
static rc_wgsl_check_result rc_package_check(rc_str source, rc_arena *arena)
{
    rc_wgsl_lex_result lexed = rc_wgsl_lex(source, arena);
    rc_wgsl_parse_result parsed;

    if (rc_wgsl_error_is_set(lexed.error)) {
        return (rc_wgsl_check_result) {.error = lexed.error};
    }
    parsed = rc_wgsl_parse(source, lexed.tokens, arena);
    if (rc_wgsl_error_is_set(parsed.error)) {
        return (rc_wgsl_check_result) {.error = parsed.error};
    }
    return rc_wgsl_check(parsed.ast, arena);
}


rc_wgsl_translate_result rc_wgsl_translate(rc_str source, rc_arena *arena)
{
    // The package keeps its own copy of the source, which everything else points into
    rc_str own = rc_mstr_from_str(source, 0, arena).view;
    rc_wgsl_check_result checked = rc_package_check(own, arena);
    rc_wgsl_module module = checked.module;
    rc_view_wgsl_index resources;
    rc_span_shader_binding bindings;
    rc_span_shader_entry_point entry_points;

    if (rc_wgsl_error_is_set(checked.error)) {
        return (rc_wgsl_translate_result) {.error = checked.error};
    }
    resources = rc_package_resources(module, arena);
    bindings = rc_span_shader_binding_make(resources.num, arena);
    for (uint32_t i = 0; i < resources.num; i++) {
        rc_span_shader_binding_set(bindings, i,
            rc_package_binding(module, rc_view_wgsl_index_get(resources, i)));
    }
    entry_points = rc_span_shader_entry_point_make(module.entry_points.num, arena);
    for (uint32_t i = 0; i < module.entry_points.num; i++) {
        rc_package_entry_result entry = rc_package_entry_point(module,
            rc_view_wgsl_index_get(module.entry_points, i), resources, arena);

        if (rc_wgsl_error_is_set(entry.error)) {
            return (rc_wgsl_translate_result) {.error = entry.error};
        }
        rc_span_shader_entry_point_set(entry_points, i, entry.entry_point);
        rc_package_note_stage(bindings, entry.entry_point);
    }

    return (rc_wgsl_translate_result) {
        .package = {
            .wgsl = own,
            .bindings = bindings.view,
            .entry_points = entry_points.view
        }
    };
}
