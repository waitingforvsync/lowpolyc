// package_c.c
//
// Writing a shader package as a C header: static const arrays beside the package that views them,
// and every string a run of literals, a line of source to each.

#include "richc/shader/wgsl.h"

#include "richc/mstr.h"

// ---- text ----

// Append one character escaped for a C string literal. A ? is escaped too, so no pair of them
// can start a trigraph.
static void rc_package_put_escaped(rc_mstr *out, char c, rc_arena *arena)
{
    uint8_t byte = (uint8_t)c;

    switch (c) {
    case '\\':
        rc_mstr_append(out, RC_STR("\\\\"), arena);
        return;
    case '"':
        rc_mstr_append(out, RC_STR("\\\""), arena);
        return;
    case '?':
        rc_mstr_append(out, RC_STR("\\?"), arena);
        return;
    case '\t':
        rc_mstr_append(out, RC_STR("\\t"), arena);
        return;
    case '\r':
        rc_mstr_append(out, RC_STR("\\r"), arena);
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


// Append text as a string literal, split after each line break, at indent.
static void rc_package_put_literal(rc_mstr *out, rc_str text, uint32_t indent, rc_arena *arena)
{
    if (rc_str_is_empty(text)) {
        rc_mstr_append(out, RC_STR("\"\""), arena);
        return;
    }

    for (uint32_t i = 0; i < text.len; i++) {
        if (i == 0 || text.data[i - 1] == '\n') {
            rc_mstr_append_char(out, '\n', arena);
            rc_mstr_append_n(out, ' ', indent, arena);
            rc_mstr_append_char(out, '"', arena);
        }

        if (text.data[i] == '\n') {
            rc_mstr_append(out, RC_STR("\\n\""), arena);
        }
        else {
            rc_package_put_escaped(out, text.data[i], arena);
        }
    }
    if (text.data[text.len - 1] != '\n') {
        rc_mstr_append_char(out, '"', arena);
    }
}


// Append text as RC_STR_INIT of a string literal, split after each line break, at indent.
static void rc_package_put_string(rc_mstr *out, rc_str text, uint32_t indent, rc_arena *arena)
{
    rc_mstr_append(out, RC_STR("RC_STR_INIT("), arena);
    rc_package_put_literal(out, text, indent, arena);
    rc_mstr_append(out, RC_STR(")"), arena);
}


// Append "name_" then part, the name of one of the package's arrays.
static void rc_package_put_array_name(rc_mstr *out, rc_str name, rc_str part, rc_arena *arena)
{
    rc_mstr_append(out, name, arena);
    rc_mstr_append_char(out, '_', arena);
    rc_mstr_append(out, part, arena);
}


// Append the name of an entry point's array: name_<index>_part.
static void rc_package_put_entry_array_name(rc_mstr *out, rc_str name, uint32_t entry, rc_str part, rc_arena *arena)
{
    rc_mstr_append(out, name, arena);
    rc_mstr_append_char(out, '_', arena);
    rc_mstr_append_u32(out, entry, arena);
    rc_mstr_append_char(out, '_', arena);
    rc_mstr_append(out, part, arena);
}


// Append a view's initialiser: {0} when empty, or the array and its count.
static void rc_package_put_view(rc_mstr *out, rc_str name, uint32_t entry, rc_str part, uint32_t num, rc_arena *arena)
{
    if (num == 0) {
        rc_mstr_append(out, RC_STR("{0}"), arena);
        return;
    }

    rc_mstr_append(out, RC_STR("{.data = "), arena);
    if (entry == RC_INDEX_NONE) {
        rc_package_put_array_name(out, name, part, arena);
    }
    else {
        rc_package_put_entry_array_name(out, name, entry, part, arena);
    }

    rc_mstr_append(out, RC_STR(", .num = "), arena);
    rc_mstr_append_u32(out, num, arena);
    rc_mstr_append(out, RC_STR("}"), arena);
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


// The name of an rc_shader_target.
static rc_str rc_package_target_name(uint8_t target)
{
    static const rc_str names[] = {
        [RC_SHADER_TARGET_NONE] = RC_STR_INIT("RC_SHADER_TARGET_NONE"),
        [RC_SHADER_TARGET_GLSL_ES300] = RC_STR_INIT("RC_SHADER_TARGET_GLSL_ES300"),
        [RC_SHADER_TARGET_GLSL_460] = RC_STR_INIT("RC_SHADER_TARGET_GLSL_460")
    };

    RC_ASSERT(target < RC_COUNT(names));
    return names[target];
}


// Append a set of targets, as RC_SHADER_TARGET_BIT(GLSL_ES300) | RC_SHADER_TARGET_BIT(GLSL_460).
static void rc_package_put_targets(rc_mstr *out, uint32_t targets, rc_arena *arena)
{
    bool first = true;

    for (uint32_t target = 1; target < 32; target++) {
        if (targets & (1u << target)) {
            rc_mstr_append(out, first ? RC_STR("RC_SHADER_TARGET_BIT(") : RC_STR(" | RC_SHADER_TARGET_BIT("), arena);
            rc_mstr_append(out, rc_str_drop_first(rc_package_target_name((uint8_t)target), 17), arena);
            rc_mstr_append(out, RC_STR(")"), arena);
            first = false;
        }
    }
    if (first) {
        rc_mstr_append(out, RC_STR("0"), arena);
    }
}

// ---- arrays ----

// Append the bindings array.
static void rc_package_put_bindings(rc_mstr *out, rc_shader_package package, rc_str name, rc_arena *arena)
{
    if (package.bindings.num == 0) {
        return;
    }

    rc_mstr_append(out, RC_STR("static const rc_shader_binding "), arena);
    rc_package_put_array_name(out, name, RC_STR("bindings"), arena);
    rc_mstr_append(out, RC_STR("[] = {\n"), arena);
    for (uint32_t i = 0; i < package.bindings.num; i++) {
        rc_shader_binding b = rc_view_shader_binding_get(package.bindings, i);

        rc_mstr_append(out, RC_STR("    {\n        .min_size = "), arena);
        rc_mstr_append_u32(out, b.min_size, arena);
        rc_mstr_append(out, RC_STR(",\n        .binding = "), arena);
        rc_mstr_append_u32(out, b.binding, arena);
        rc_mstr_append(out, RC_STR(",\n        .group = "), arena);
        rc_mstr_append_u32(out, b.group, arena);
        rc_mstr_append(out, RC_STR(",\n        .type = "), arena);
        rc_mstr_append(out, rc_package_binding_type_name(b.type), arena);
        rc_mstr_append(out, RC_STR(",\n        .stages = "), arena);
        rc_mstr_append_u32(out, b.stages, arena);
        rc_mstr_append(out, RC_STR(",\n        .dimension = "), arena);
        rc_mstr_append(out, rc_package_dimension_name(b.dimension), arena);
        rc_mstr_append(out, RC_STR(",\n        .sample_type = "), arena);
        rc_mstr_append(out, rc_package_sample_type_name(b.sample_type), arena);
        rc_mstr_append(out, RC_STR(",\n        .multisampled = "), arena);
        rc_mstr_append_u32(out, b.multisampled, arena);
        rc_mstr_append(out, RC_STR("\n    },\n"), arena);
    }
    rc_mstr_append(out, RC_STR("};\n\n"), arena);
}


// Append text as a static const char array called array: a string literal, or a list of bytes
// when it is longer than every compiler takes a string literal to be. MSVC's limit is 64 KiB.
static void rc_package_put_text(rc_mstr *out, rc_str text, rc_str array, rc_arena *arena)
{
    rc_mstr_append(out, RC_STR("static const char "), arena);
    rc_mstr_append(out, array, arena);
    rc_mstr_append(out, RC_STR("[] = "), arena);
    if (text.len < 0xffff) {
        rc_package_put_literal(out, text, 4, arena);
        rc_mstr_append(out, RC_STR(";\n\n"), arena);
        return;
    }

    rc_mstr_append(out, RC_STR("{"), arena);
    for (uint32_t i = 0; i < text.len; i++) {
        rc_mstr_append(out, i % 16 == 0 ? RC_STR("\n    ") : RC_STR(" "), arena);
        rc_mstr_append_u32(out, (uint8_t)text.data[i], arena);
        rc_mstr_append_char(out, ',', arena);
    }
    rc_mstr_append(out, RC_STR("\n};\n\n"), arena);
}


// Append the name of program j of entry point i's part: name_i_j_part.
static void rc_package_put_program_part(rc_mstr *out, rc_str name, uint32_t i, uint32_t j, rc_str part, rc_arena *arena)
{
    rc_mstr part_name = rc_mstr_make(16, arena);

    rc_mstr_append_u32(&part_name, i, arena);
    rc_mstr_append_char(&part_name, '_', arena);
    rc_mstr_append_u32(&part_name, j, arena);
    rc_mstr_append_char(&part_name, '_', arena);
    rc_mstr_append(&part_name, part, arena);
    rc_package_put_array_name(out, name, part_name.view, arena);
}


// Append program j of entry point i's code and slots arrays.
static void rc_package_put_program_arrays(rc_mstr *out, rc_shader_program program, rc_str name, uint32_t i, uint32_t j, rc_arena *arena)
{
    rc_mstr code = rc_mstr_make(32, arena);

    rc_package_put_program_part(&code, name, i, j, RC_STR("code"), arena);
    rc_package_put_text(out, rc_view_bytes_as_str(program.code), code.view, arena);
    if (program.slots.num == 0) {
        return;
    }

    rc_mstr_append(out, RC_STR("static const rc_shader_slot "), arena);
    rc_package_put_program_part(out, name, i, j, RC_STR("slots"), arena);
    rc_mstr_append(out, RC_STR("[] = {\n"), arena);
    for (uint32_t k = 0; k < program.slots.num; k++) {
        rc_shader_slot slot = rc_view_shader_slot_get(program.slots, k);

        rc_mstr_append(out, RC_STR("    {.name = "), arena);
        rc_package_put_string(out, slot.name, 8, arena);
        rc_mstr_append(out, RC_STR(", .binding = "), arena);
        rc_mstr_append_u32(out, slot.binding, arena);
        rc_mstr_append(out, RC_STR(", .sampler = "), arena);
        rc_mstr_append_u32(out, slot.sampler, arena);
        rc_mstr_append(out, RC_STR("},\n"), arena);
    }
    rc_mstr_append(out, RC_STR("};\n\n"), arena);
}


// Append entry point i's programs, and the array of them.
static void rc_package_put_programs(rc_mstr *out, rc_shader_entry_point entry, rc_str name, uint32_t i, rc_arena *arena)
{
    for (uint32_t j = 0; j < entry.programs.num; j++) {
        rc_package_put_program_arrays(out, rc_view_shader_program_get(entry.programs, j), name, i, j, arena);
    }
    if (entry.programs.num == 0) {
        return;
    }

    rc_mstr_append(out, RC_STR("static const rc_shader_program "), arena);
    rc_package_put_entry_array_name(out, name, i, RC_STR("programs"), arena);
    rc_mstr_append(out, RC_STR("[] = {\n"), arena);
    for (uint32_t j = 0; j < entry.programs.num; j++) {
        rc_shader_program program = rc_view_shader_program_get(entry.programs, j);

        rc_mstr_append(out, RC_STR("    {\n        .code = {.data = (const uint8_t *)"), arena);
        rc_package_put_program_part(out, name, i, j, RC_STR("code"), arena);
        rc_mstr_append(out, RC_STR(", .num = "), arena);
        rc_mstr_append_u32(out, program.code.num, arena);
        rc_mstr_append(out, RC_STR("},\n        .slots = "), arena);
        if (program.slots.num == 0) {
            rc_mstr_append(out, RC_STR("{0}"), arena);
        }
        else {
            rc_mstr_append(out, RC_STR("{.data = "), arena);
            rc_package_put_program_part(out, name, i, j, RC_STR("slots"), arena);
            rc_mstr_append(out, RC_STR(", .num = "), arena);
            rc_mstr_append_u32(out, program.slots.num, arena);
            rc_mstr_append(out, RC_STR("}"), arena);
        }

        rc_mstr_append(out, RC_STR(",\n        .target = "), arena);
        rc_mstr_append(out, rc_package_target_name(program.target), arena);
        rc_mstr_append(out, RC_STR("\n    },\n"), arena);
    }
    rc_mstr_append(out, RC_STR("};\n\n"), arena);
}


// Append an entry point's array of locations, part being inputs or outputs.
static void rc_package_put_locations(rc_mstr *out, rc_view_shader_location locations, rc_str name,
    uint32_t index, rc_str part, rc_arena *arena)
{
    if (locations.num == 0) {
        return;
    }

    rc_mstr_append(out, RC_STR("static const rc_shader_location "), arena);
    rc_package_put_entry_array_name(out, name, index, part, arena);
    rc_mstr_append(out, RC_STR("[] = {\n"), arena);
    for (uint32_t i = 0; i < locations.num; i++) {
        rc_shader_location l = rc_view_shader_location_get(locations, i);

        rc_mstr_append(out, RC_STR("    {.location = "), arena);
        rc_mstr_append_u32(out, l.location, arena);
        rc_mstr_append(out, RC_STR(", .format = "), arena);
        rc_mstr_append(out, rc_package_format_name(l.format), arena);
        rc_mstr_append(out, RC_STR("},\n"), arena);
    }
    rc_mstr_append(out, RC_STR("};\n\n"), arena);
}


// Append one entry point's initialiser, in the entry points array.
static void rc_package_put_entry_point(rc_mstr *out, rc_shader_entry_point entry, rc_str name, uint32_t index, rc_arena *arena)
{
    rc_mstr_append(out, RC_STR("    {\n        .name = "), arena);
    rc_package_put_string(out, entry.name, 12, arena);
    rc_mstr_append(out, RC_STR(",\n        .programs = "), arena);
    rc_package_put_view(out, name, index, RC_STR("programs"), entry.programs.num, arena);
    rc_mstr_append(out, RC_STR(",\n        .inputs = "), arena);
    rc_package_put_view(out, name, index, RC_STR("inputs"), entry.inputs.num, arena);
    rc_mstr_append(out, RC_STR(",\n        .outputs = "), arena);
    rc_package_put_view(out, name, index, RC_STR("outputs"), entry.outputs.num, arena);
    rc_mstr_append(out, RC_STR(",\n        .stage = "), arena);
    rc_mstr_append(out, rc_package_stage_name(entry.stage), arena);
    rc_mstr_append(out, RC_STR("\n    },\n"), arena);
}


// Append the guard macro's name: the package's name in capitals, between a prefix and _H_.
static void rc_package_put_guard(rc_mstr *out, rc_str name, rc_arena *arena)
{
    rc_mstr_append(out, RC_STR("RC_SHADER_PACKAGE_"), arena);
    for (uint32_t i = 0; i < name.len; i++) {
        char c = name.data[i];

        rc_mstr_append_char(out, c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c, arena);
    }
    rc_mstr_append(out, RC_STR("_H_"), arena);
}


// Append each entry point's arrays, and the array of entry points.
static void rc_package_put_entry_points(rc_mstr *out, rc_shader_package package, rc_str name, rc_arena *arena)
{
    for (uint32_t i = 0; i < package.entry_points.num; i++) {
        rc_shader_entry_point entry = rc_view_shader_entry_point_get(package.entry_points, i);

        rc_package_put_programs(out, entry, name, i, arena);
        rc_package_put_locations(out, entry.inputs, name, i, RC_STR("inputs"), arena);
        rc_package_put_locations(out, entry.outputs, name, i, RC_STR("outputs"), arena);
    }
    if (package.entry_points.num == 0) {
        return;
    }

    rc_mstr_append(out, RC_STR("static const rc_shader_entry_point "), arena);
    rc_package_put_array_name(out, name, RC_STR("entry_points"), arena);
    rc_mstr_append(out, RC_STR("[] = {\n"), arena);
    for (uint32_t i = 0; i < package.entry_points.num; i++) {
        rc_package_put_entry_point(out, rc_view_shader_entry_point_get(package.entry_points, i), name, i, arena);
    }
    rc_mstr_append(out, RC_STR("};\n\n"), arena);
}


// Append the package itself, viewing the arrays before it.
static void rc_package_put_package(rc_mstr *out, rc_shader_package package, rc_str name, rc_arena *arena)
{
    rc_mstr_append(out, RC_STR("static const rc_shader_package "), arena);
    rc_mstr_append(out, name, arena);
    rc_mstr_append(out, RC_STR(" = {\n    .wgsl = {.data = "), arena);
    rc_package_put_array_name(out, name, RC_STR("wgsl"), arena);
    rc_mstr_append(out, RC_STR(", .len = "), arena);
    rc_mstr_append_u32(out, package.wgsl.len, arena);
    rc_mstr_append(out, RC_STR("},\n    .bindings = "), arena);
    rc_package_put_view(out, name, RC_INDEX_NONE, RC_STR("bindings"), package.bindings.num, arena);
    rc_mstr_append(out, RC_STR(",\n    .entry_points = "), arena);
    rc_package_put_view(out, name, RC_INDEX_NONE, RC_STR("entry_points"), package.entry_points.num, arena);
    rc_mstr_append(out, RC_STR(",\n    .targets = "), arena);
    rc_package_put_targets(out, package.targets, arena);
    rc_mstr_append(out, RC_STR("\n};\n"), arena);
}


void rc_shader_package_append_c(rc_mstr *out, rc_shader_package package, rc_str name, rc_arena *arena)
{
    rc_mstr wgsl = rc_mstr_make(32, arena);

    rc_package_put_array_name(&wgsl, name, RC_STR("wgsl"), arena);
    rc_mstr_append(out, RC_STR("// Made by richc's shader tool. Don't change it: change the WGSL, and build again.\n\n#ifndef "), arena);
    rc_package_put_guard(out, name, arena);
    rc_mstr_append(out, RC_STR("\n#define "), arena);
    rc_package_put_guard(out, name, arena);
    rc_mstr_append(out, RC_STR("\n\n#include \"richc/shader/shader.h\"\n\n"), arena);
    rc_package_put_text(out, package.wgsl, wgsl.view, arena);
    rc_package_put_bindings(out, package, name, arena);
    rc_package_put_entry_points(out, package, name, arena);
    rc_package_put_package(out, package, name, arena);
    rc_mstr_append(out, RC_STR("\n#endif\n"), arena);
}
