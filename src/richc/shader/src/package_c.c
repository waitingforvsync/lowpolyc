// package_c.c
//
// Writing a shader package as a C header: static const arrays beside the package that views them,
// and every string a run of literals, a line of source to each.

#include "richc/shader/wgsl.h"

#include "richc/mstr.h"

// ---- text ----

// Append text.
static void rc_package_put(rc_mstr *out, rc_str text, rc_arena *arena)
{
    rc_mstr_append(out, text, arena);
}


// Append one character escaped for a C string literal. A ? is escaped too, so no pair of them
// can start a trigraph.
static void rc_package_put_escaped(rc_mstr *out, char c, rc_arena *arena)
{
    uint8_t byte = (uint8_t)c;

    switch (c) {
    case '\\':
        rc_package_put(out, RC_STR("\\\\"), arena);
        return;
    case '"':
        rc_package_put(out, RC_STR("\\\""), arena);
        return;
    case '?':
        rc_package_put(out, RC_STR("\\?"), arena);
        return;
    case '\t':
        rc_package_put(out, RC_STR("\\t"), arena);
        return;
    case '\r':
        rc_package_put(out, RC_STR("\\r"), arena);
        return;
    default:
        break;
    }
    if (byte < 0x20 || byte >= 0x7f) {
        rc_mstr_append_char(out, '\\', arena);
        rc_mstr_append_char(out, (char)('0' + (byte >> 6)), arena);
        rc_mstr_append_char(out, (char)('0' + ((byte >> 3) & 7)), arena);
        rc_mstr_append_char(out, (char)('0' + (byte & 7)), arena);
        return;
    }
    rc_mstr_append_char(out, c, arena);
}


// Append text as RC_STR_INIT of a string literal, split after each line break, at indent.
static void rc_package_put_string(rc_mstr *out, rc_str text, uint32_t indent, rc_arena *arena)
{
    rc_package_put(out, RC_STR("RC_STR_INIT("), arena);
    if (rc_str_is_empty(text)) {
        rc_package_put(out, RC_STR("\"\")"), arena);
        return;
    }
    for (uint32_t i = 0; i < text.len; i++) {
        if (i == 0 || text.data[i - 1] == '\n') {
            rc_mstr_append_char(out, '\n', arena);
            rc_mstr_append_n(out, ' ', indent, arena);
            rc_mstr_append_char(out, '"', arena);
        }
        if (text.data[i] == '\n') {
            rc_package_put(out, RC_STR("\\n\""), arena);
        }
        else {
            rc_package_put_escaped(out, text.data[i], arena);
        }
    }
    if (text.data[text.len - 1] != '\n') {
        rc_mstr_append_char(out, '"', arena);
    }
    rc_package_put(out, RC_STR(")"), arena);
}


// Append "name_" then part, the name of one of the package's arrays.
static void rc_package_put_array_name(rc_mstr *out, rc_str name, rc_str part, rc_arena *arena)
{
    rc_package_put(out, name, arena);
    rc_mstr_append_char(out, '_', arena);
    rc_package_put(out, part, arena);
}


// Append the name of an entry point's array: name_<index>_part.
static void rc_package_put_entry_array_name(rc_mstr *out, rc_str name, uint32_t entry,
    rc_str part, rc_arena *arena)
{
    rc_package_put(out, name, arena);
    rc_mstr_append_char(out, '_', arena);
    rc_mstr_append_u32(out, entry, arena);
    rc_mstr_append_char(out, '_', arena);
    rc_package_put(out, part, arena);
}


// Append a view's initialiser: {0} when empty, or the array and its count.
static void rc_package_put_view(rc_mstr *out, rc_str name, uint32_t entry, rc_str part,
    uint32_t num, rc_arena *arena)
{
    if (num == 0) {
        rc_package_put(out, RC_STR("{0}"), arena);
        return;
    }
    rc_package_put(out, RC_STR("{.data = "), arena);
    if (entry == RC_INDEX_NONE) {
        rc_package_put_array_name(out, name, part, arena);
    }
    else {
        rc_package_put_entry_array_name(out, name, entry, part, arena);
    }
    rc_package_put(out, RC_STR(", .num = "), arena);
    rc_mstr_append_u32(out, num, arena);
    rc_package_put(out, RC_STR("}"), arena);
}

// ---- enumerations by name ----

// The name of an rc_shader_binding_type.
static rc_str rc_package_binding_type_name(uint8_t type)
{
    static const rc_str names[] = {
        [RC_SHADER_BINDING_TYPE_NONE] = RC_STR_INIT("RC_SHADER_BINDING_TYPE_NONE"),
        [RC_SHADER_BINDING_TYPE_UNIFORM_BUFFER] = RC_STR_INIT("RC_SHADER_BINDING_TYPE_UNIFORM_BUFFER"),
        [RC_SHADER_BINDING_TYPE_STORAGE_BUFFER] = RC_STR_INIT("RC_SHADER_BINDING_TYPE_STORAGE_BUFFER"),
        [RC_SHADER_BINDING_TYPE_READ_ONLY_STORAGE_BUFFER] = RC_STR_INIT("RC_SHADER_BINDING_TYPE_READ_ONLY_STORAGE_BUFFER"),
        [RC_SHADER_BINDING_TYPE_SAMPLER] = RC_STR_INIT("RC_SHADER_BINDING_TYPE_SAMPLER"),
        [RC_SHADER_BINDING_TYPE_COMPARISON_SAMPLER] = RC_STR_INIT("RC_SHADER_BINDING_TYPE_COMPARISON_SAMPLER"),
        [RC_SHADER_BINDING_TYPE_TEXTURE] = RC_STR_INIT("RC_SHADER_BINDING_TYPE_TEXTURE")
    };

    RC_ASSERT(type < RC_COUNT(names));
    return names[type];
}


// The name of an rc_shader_dimension.
static rc_str rc_package_dimension_name(uint8_t dimension)
{
    static const rc_str names[] = {
        [RC_SHADER_DIMENSION_NONE] = RC_STR_INIT("RC_SHADER_DIMENSION_NONE"),
        [RC_SHADER_DIMENSION_1D] = RC_STR_INIT("RC_SHADER_DIMENSION_1D"),
        [RC_SHADER_DIMENSION_2D] = RC_STR_INIT("RC_SHADER_DIMENSION_2D"),
        [RC_SHADER_DIMENSION_2D_ARRAY] = RC_STR_INIT("RC_SHADER_DIMENSION_2D_ARRAY"),
        [RC_SHADER_DIMENSION_3D] = RC_STR_INIT("RC_SHADER_DIMENSION_3D"),
        [RC_SHADER_DIMENSION_CUBE] = RC_STR_INIT("RC_SHADER_DIMENSION_CUBE"),
        [RC_SHADER_DIMENSION_CUBE_ARRAY] = RC_STR_INIT("RC_SHADER_DIMENSION_CUBE_ARRAY")
    };

    RC_ASSERT(dimension < RC_COUNT(names));
    return names[dimension];
}


// The name of an rc_shader_sample.
static rc_str rc_package_sample_type_name(uint8_t sample_type)
{
    static const rc_str names[] = {
        [RC_SHADER_SAMPLE_NONE] = RC_STR_INIT("RC_SHADER_SAMPLE_NONE"),
        [RC_SHADER_SAMPLE_FLOAT] = RC_STR_INIT("RC_SHADER_SAMPLE_FLOAT"),
        [RC_SHADER_SAMPLE_DEPTH] = RC_STR_INIT("RC_SHADER_SAMPLE_DEPTH"),
        [RC_SHADER_SAMPLE_SINT] = RC_STR_INIT("RC_SHADER_SAMPLE_SINT"),
        [RC_SHADER_SAMPLE_UINT] = RC_STR_INIT("RC_SHADER_SAMPLE_UINT")
    };

    RC_ASSERT(sample_type < RC_COUNT(names));
    return names[sample_type];
}


// The name of an rc_shader_format.
static rc_str rc_package_format_name(uint8_t format)
{
    static const rc_str names[] = {
        [RC_SHADER_FORMAT_NONE] = RC_STR_INIT("RC_SHADER_FORMAT_NONE"),
        [RC_SHADER_FORMAT_FLOAT32] = RC_STR_INIT("RC_SHADER_FORMAT_FLOAT32"),
        [RC_SHADER_FORMAT_FLOAT32X2] = RC_STR_INIT("RC_SHADER_FORMAT_FLOAT32X2"),
        [RC_SHADER_FORMAT_FLOAT32X3] = RC_STR_INIT("RC_SHADER_FORMAT_FLOAT32X3"),
        [RC_SHADER_FORMAT_FLOAT32X4] = RC_STR_INIT("RC_SHADER_FORMAT_FLOAT32X4"),
        [RC_SHADER_FORMAT_SINT32] = RC_STR_INIT("RC_SHADER_FORMAT_SINT32"),
        [RC_SHADER_FORMAT_SINT32X2] = RC_STR_INIT("RC_SHADER_FORMAT_SINT32X2"),
        [RC_SHADER_FORMAT_SINT32X3] = RC_STR_INIT("RC_SHADER_FORMAT_SINT32X3"),
        [RC_SHADER_FORMAT_SINT32X4] = RC_STR_INIT("RC_SHADER_FORMAT_SINT32X4"),
        [RC_SHADER_FORMAT_UINT32] = RC_STR_INIT("RC_SHADER_FORMAT_UINT32"),
        [RC_SHADER_FORMAT_UINT32X2] = RC_STR_INIT("RC_SHADER_FORMAT_UINT32X2"),
        [RC_SHADER_FORMAT_UINT32X3] = RC_STR_INIT("RC_SHADER_FORMAT_UINT32X3"),
        [RC_SHADER_FORMAT_UINT32X4] = RC_STR_INIT("RC_SHADER_FORMAT_UINT32X4")
    };

    RC_ASSERT(format < RC_COUNT(names));
    return names[format];
}


// The name of an rc_shader_stage.
static rc_str rc_package_stage_name(uint8_t stage)
{
    static const rc_str names[] = {
        [RC_SHADER_STAGE_NONE] = RC_STR_INIT("RC_SHADER_STAGE_NONE"),
        [RC_SHADER_STAGE_VERTEX] = RC_STR_INIT("RC_SHADER_STAGE_VERTEX"),
        [RC_SHADER_STAGE_FRAGMENT] = RC_STR_INIT("RC_SHADER_STAGE_FRAGMENT")
    };

    RC_ASSERT(stage < RC_COUNT(names));
    return names[stage];
}

// ---- arrays ----

// Append the bindings array.
static void rc_package_put_bindings(rc_mstr *out, rc_shader_package package, rc_str name,
    rc_arena *arena)
{
    if (package.bindings.num == 0) {
        return;
    }
    rc_package_put(out, RC_STR("static const rc_shader_binding "), arena);
    rc_package_put_array_name(out, name, RC_STR("bindings"), arena);
    rc_package_put(out, RC_STR("[] = {\n"), arena);
    for (uint32_t i = 0; i < package.bindings.num; i++) {
        rc_shader_binding b = rc_view_shader_binding_get(package.bindings, i);

        rc_package_put(out, RC_STR("    {\n        .min_size = "), arena);
        rc_mstr_append_u32(out, b.min_size, arena);
        rc_package_put(out, RC_STR(",\n        .binding = "), arena);
        rc_mstr_append_u32(out, b.binding, arena);
        rc_package_put(out, RC_STR(",\n        .group = "), arena);
        rc_mstr_append_u32(out, b.group, arena);
        rc_package_put(out, RC_STR(",\n        .type = "), arena);
        rc_package_put(out, rc_package_binding_type_name(b.type), arena);
        rc_package_put(out, RC_STR(",\n        .stages = "), arena);
        rc_mstr_append_u32(out, b.stages, arena);
        rc_package_put(out, RC_STR(",\n        .dimension = "), arena);
        rc_package_put(out, rc_package_dimension_name(b.dimension), arena);
        rc_package_put(out, RC_STR(",\n        .sample_type = "), arena);
        rc_package_put(out, rc_package_sample_type_name(b.sample_type), arena);
        rc_package_put(out, RC_STR(",\n        .multisampled = "), arena);
        rc_mstr_append_u32(out, b.multisampled, arena);
        rc_package_put(out, RC_STR("\n    },\n"), arena);
    }
    rc_package_put(out, RC_STR("};\n\n"), arena);
}


// Append an entry point's samplers and blocks arrays.
static void rc_package_put_names(rc_mstr *out, rc_shader_entry_point entry, rc_str name,
    uint32_t index, rc_arena *arena)
{
    if (entry.samplers.num > 0) {
        rc_package_put(out, RC_STR("static const rc_shader_sampler "), arena);
        rc_package_put_entry_array_name(out, name, index, RC_STR("samplers"), arena);
        rc_package_put(out, RC_STR("[] = {\n"), arena);
        for (uint32_t i = 0; i < entry.samplers.num; i++) {
            rc_shader_sampler s = rc_view_shader_sampler_get(entry.samplers, i);

            rc_package_put(out, RC_STR("    {.name = "), arena);
            rc_package_put_string(out, s.name, 8, arena);
            rc_package_put(out, RC_STR(", .texture = "), arena);
            rc_mstr_append_u32(out, s.texture, arena);
            rc_package_put(out, RC_STR(", .sampler = "), arena);
            rc_mstr_append_u32(out, s.sampler, arena);
            rc_package_put(out, RC_STR("},\n"), arena);
        }
        rc_package_put(out, RC_STR("};\n\n"), arena);
    }
    if (entry.blocks.num > 0) {
        rc_package_put(out, RC_STR("static const rc_shader_block "), arena);
        rc_package_put_entry_array_name(out, name, index, RC_STR("blocks"), arena);
        rc_package_put(out, RC_STR("[] = {\n"), arena);
        for (uint32_t i = 0; i < entry.blocks.num; i++) {
            rc_shader_block b = rc_view_shader_block_get(entry.blocks, i);

            rc_package_put(out, RC_STR("    {.name = "), arena);
            rc_package_put_string(out, b.name, 8, arena);
            rc_package_put(out, RC_STR(", .binding = "), arena);
            rc_mstr_append_u32(out, b.binding, arena);
            rc_package_put(out, RC_STR("},\n"), arena);
        }
        rc_package_put(out, RC_STR("};\n\n"), arena);
    }
}


// Append an entry point's array of locations, part being inputs or outputs.
static void rc_package_put_locations(rc_mstr *out, rc_view_shader_location locations, rc_str name,
    uint32_t index, rc_str part, rc_arena *arena)
{
    if (locations.num == 0) {
        return;
    }
    rc_package_put(out, RC_STR("static const rc_shader_location "), arena);
    rc_package_put_entry_array_name(out, name, index, part, arena);
    rc_package_put(out, RC_STR("[] = {\n"), arena);
    for (uint32_t i = 0; i < locations.num; i++) {
        rc_shader_location l = rc_view_shader_location_get(locations, i);

        rc_package_put(out, RC_STR("    {.location = "), arena);
        rc_mstr_append_u32(out, l.location, arena);
        rc_package_put(out, RC_STR(", .format = "), arena);
        rc_package_put(out, rc_package_format_name(l.format), arena);
        rc_package_put(out, RC_STR("},\n"), arena);
    }
    rc_package_put(out, RC_STR("};\n\n"), arena);
}


// Append one entry point's initialiser, in the entry points array.
static void rc_package_put_entry_point(rc_mstr *out, rc_shader_entry_point entry, rc_str name,
    uint32_t index, rc_arena *arena)
{
    rc_package_put(out, RC_STR("    {\n        .name = "), arena);
    rc_package_put_string(out, entry.name, 12, arena);
    rc_package_put(out, RC_STR(",\n        .glsl_es = "), arena);
    rc_package_put_string(out, entry.glsl_es, 12, arena);
    rc_package_put(out, RC_STR(",\n        .glsl = "), arena);
    rc_package_put_string(out, entry.glsl, 12, arena);
    rc_package_put(out, RC_STR(",\n        .samplers = "), arena);
    rc_package_put_view(out, name, index, RC_STR("samplers"), entry.samplers.num, arena);
    rc_package_put(out, RC_STR(",\n        .blocks = "), arena);
    rc_package_put_view(out, name, index, RC_STR("blocks"), entry.blocks.num, arena);
    rc_package_put(out, RC_STR(",\n        .inputs = "), arena);
    rc_package_put_view(out, name, index, RC_STR("inputs"), entry.inputs.num, arena);
    rc_package_put(out, RC_STR(",\n        .outputs = "), arena);
    rc_package_put_view(out, name, index, RC_STR("outputs"), entry.outputs.num, arena);
    rc_package_put(out, RC_STR(",\n        .stage = "), arena);
    rc_package_put(out, rc_package_stage_name(entry.stage), arena);
    rc_package_put(out, RC_STR("\n    },\n"), arena);
}


// Append the guard macro's name: the package's name in capitals, between a prefix and _H_.
static void rc_package_put_guard(rc_mstr *out, rc_str name, rc_arena *arena)
{
    rc_package_put(out, RC_STR("RC_SHADER_PACKAGE_"), arena);
    for (uint32_t i = 0; i < name.len; i++) {
        char c = name.data[i];

        rc_mstr_append_char(out, c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c, arena);
    }
    rc_package_put(out, RC_STR("_H_"), arena);
}


void rc_shader_package_append_c(rc_mstr *out, rc_shader_package package, rc_str name,
    rc_arena *arena)
{
    rc_package_put(out, RC_STR("// Made by richc's shader tool. Don't change it: change the WGSL, "
        "and build again.\n\n#ifndef "), arena);
    rc_package_put_guard(out, name, arena);
    rc_package_put(out, RC_STR("\n#define "), arena);
    rc_package_put_guard(out, name, arena);
    rc_package_put(out, RC_STR("\n\n#include \"richc/shader/shader.h\"\n\n"), arena);
    rc_package_put_bindings(out, package, name, arena);
    for (uint32_t i = 0; i < package.entry_points.num; i++) {
        rc_shader_entry_point entry = rc_view_shader_entry_point_get(package.entry_points, i);

        rc_package_put_names(out, entry, name, i, arena);
        rc_package_put_locations(out, entry.inputs, name, i, RC_STR("inputs"), arena);
        rc_package_put_locations(out, entry.outputs, name, i, RC_STR("outputs"), arena);
    }
    if (package.entry_points.num > 0) {
        rc_package_put(out, RC_STR("static const rc_shader_entry_point "), arena);
        rc_package_put_array_name(out, name, RC_STR("entry_points"), arena);
        rc_package_put(out, RC_STR("[] = {\n"), arena);
        for (uint32_t i = 0; i < package.entry_points.num; i++) {
            rc_package_put_entry_point(out, rc_view_shader_entry_point_get(package.entry_points, i),
                name, i, arena);
        }
        rc_package_put(out, RC_STR("};\n\n"), arena);
    }
    rc_package_put(out, RC_STR("static const rc_shader_package "), arena);
    rc_package_put(out, name, arena);
    rc_package_put(out, RC_STR(" = {\n    .wgsl = "), arena);
    rc_package_put_string(out, package.wgsl, 8, arena);
    rc_package_put(out, RC_STR(",\n    .bindings = "), arena);
    rc_package_put_view(out, name, RC_INDEX_NONE, RC_STR("bindings"), package.bindings.num, arena);
    rc_package_put(out, RC_STR(",\n    .entry_points = "), arena);
    rc_package_put_view(out, name, RC_INDEX_NONE, RC_STR("entry_points"),
        package.entry_points.num, arena);
    rc_package_put(out, RC_STR("\n};\n\n#endif\n"), arena);
}
