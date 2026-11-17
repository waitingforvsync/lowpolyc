// test_package.c
//
// Tests for shader packages: translating a module into one, and writing it as C. The build runs
// the shader tool on data/package_test.wgsl, so the package compiled in below can be held up
// against the one rc_wgsl_translate makes from the same file.

#include "richc/shader/wgsl.h"

#include "richc/file.h"
#include "richc/mstr.h"
#include "richc/test.h"
#include "shaders/package_test.wgsl.h"

// Both GLSL targets, which the build's tool makes too.
#define BOTH (RC_SHADER_TARGET_BIT(GLSL_ES300) | RC_SHADER_TARGET_BIT(GLSL_460))

// Most tests run against a fresh default arena, with a scratch arena beside it.
RC_TEST_GROUP_DATA(package) {
    rc_arena a;
    rc_arena scratch;
};


RC_TEST_GROUP_INIT(package, fix)
{
    fix->a = rc_arena_make_default();
    fix->scratch = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(package, fix)
{
    rc_arena_release(&fix->scratch);
    rc_arena_release(&fix->a);
}


// The package translated from source for targets.
static rc_wgsl_translate_result translate(rc_str source, uint32_t targets, struct rc_test_group_data_package *fix)
{
    return rc_wgsl_translate(source, targets, &fix->a, fix->scratch);
}


// The package translated from the same source as the one compiled in.
static rc_shader_package translated(struct rc_test_group_data_package *fix)
{
    rc_wgsl_translate_result result = translate(package_test_wgsl.wgsl, BOTH, fix);

    RC_CHECK_FALSE(rc_wgsl_error_is_set(result.error));
    return result.package;
}


// The slot called name in the entry point's program for target, or one with no name.
static rc_shader_slot slot_named(rc_shader_entry_point entry, rc_shader_target target, const char *name)
{
    rc_view_shader_slot slots = rc_shader_entry_point_program(entry, target).slots;

    for (uint32_t i = 0; i < slots.num; i++) {
        if (rc_str_is_equal(rc_view_shader_slot_get(slots, i).name, rc_str_from_cstr(name))) {
            return rc_view_shader_slot_get(slots, i);
        }
    }
    return (rc_shader_slot) {0};
}

// ---- translating ----

RC_TEST_STEP(package, bindings, fix)
{
    rc_shader_package package = translated(fix);
    rc_shader_binding uniforms = rc_view_shader_binding_get(package.bindings, 0);
    rc_shader_binding colour = rc_view_shader_binding_get(package.bindings, 1);
    rc_shader_binding sampler = rc_view_shader_binding_get(package.bindings, 2);

    RC_CHECK(package.bindings.num, ==, 3u);
    RC_CHECK(package.targets, ==, BOTH);
    RC_CHECK(uniforms.type, ==, RC_SHADER_BINDING_TYPE_UNIFORM_BUFFER);
    RC_CHECK(uniforms.group, ==, 0u);
    RC_CHECK(uniforms.binding, ==, 0u);
    RC_CHECK(uniforms.min_size, ==, 80u);
    RC_CHECK(uniforms.stages, ==, (1u << RC_SHADER_STAGE_VERTEX) | (1u << RC_SHADER_STAGE_FRAGMENT));
    RC_CHECK(colour.type, ==, RC_SHADER_BINDING_TYPE_TEXTURE);
    RC_CHECK(colour.binding, ==, 1u);
    RC_CHECK(colour.dimension, ==, RC_SHADER_DIMENSION_2D);
    RC_CHECK(colour.sample_type, ==, RC_SHADER_SAMPLE_FLOAT);
    RC_CHECK(colour.stages, ==, 1u << RC_SHADER_STAGE_FRAGMENT);
    RC_CHECK(sampler.type, ==, RC_SHADER_BINDING_TYPE_SAMPLER);
    RC_CHECK(sampler.binding, ==, 2u);
    RC_CHECK(sampler.stages, ==, 1u << RC_SHADER_STAGE_FRAGMENT);
}


RC_TEST_STEP(package, entry_points, fix)
{
    rc_shader_package package = translated(fix);
    rc_shader_entry_point vs = rc_view_shader_entry_point_get(package.entry_points, 0);
    rc_shader_entry_point fs = rc_view_shader_entry_point_get(package.entry_points, 1);
    rc_shader_program fs_es = rc_shader_entry_point_program(fs, RC_SHADER_TARGET_GLSL_ES300);
    rc_shader_program fs_gl = rc_shader_entry_point_program(fs, RC_SHADER_TARGET_GLSL_460);

    RC_CHECK(package.entry_points.num, ==, 2u);
    RC_CHECK(vs.name, ==, RC_STR("vs"));
    RC_CHECK(vs.stage, ==, RC_SHADER_STAGE_VERTEX);
    RC_CHECK(vs.programs.num, ==, 2u);
    RC_CHECK(vs.inputs.num, ==, 2u);
    RC_CHECK(rc_view_shader_location_get(vs.inputs, 0).format, ==, RC_SHADER_FORMAT_FLOAT32X3);
    RC_CHECK(rc_view_shader_location_get(vs.inputs, 1).location, ==, 1u);
    RC_CHECK(rc_view_shader_location_get(vs.inputs, 1).format, ==, RC_SHADER_FORMAT_FLOAT32X2);
    RC_CHECK(vs.outputs.num, ==, 0u);
    RC_CHECK(rc_shader_entry_point_program(vs, RC_SHADER_TARGET_GLSL_ES300).slots.num, ==, 1u);
    RC_CHECK(slot_named(vs, RC_SHADER_TARGET_GLSL_ES300, "rc_ubo_0_0").binding, ==, 0u);
    RC_CHECK(slot_named(vs, RC_SHADER_TARGET_GLSL_ES300, "rc_ubo_0_0").sampler, ==, RC_SHADER_NONE);
    RC_CHECK(fs.stage, ==, RC_SHADER_STAGE_FRAGMENT);
    RC_CHECK(fs.outputs.num, ==, 1u);
    RC_CHECK(rc_view_shader_location_get(fs.outputs, 0).format, ==, RC_SHADER_FORMAT_FLOAT32X4);
    RC_CHECK(fs_es.slots.num, ==, 2u);
    RC_CHECK(slot_named(fs, RC_SHADER_TARGET_GLSL_ES300, "rc_t0_1_s0_2").binding, ==, 1u);
    RC_CHECK(slot_named(fs, RC_SHADER_TARGET_GLSL_ES300, "rc_t0_1_s0_2").sampler, ==, 2u);
    RC_CHECK_TRUE(rc_str_starts_with(rc_view_bytes_as_str(fs_es.code), RC_STR("#version 300 es\n")));
    RC_CHECK_TRUE(rc_str_starts_with(rc_view_bytes_as_str(fs_gl.code), RC_STR("#version 460\n")));
}


RC_TEST_STEP(package, find, fix)
{
    rc_shader_package package = translated(fix);

    RC_CHECK(rc_shader_package_find(package, RC_STR("fs")), ==, 1u);
    RC_CHECK(rc_shader_package_find(package, RC_STR("vs")), ==, 0u);
    RC_CHECK(rc_shader_package_find(package, RC_STR("main")), ==, RC_INDEX_NONE);
    RC_CHECK(rc_shader_entry_point_program(rc_view_shader_entry_point_get(package.entry_points, 0),
        RC_SHADER_TARGET_NONE).code.num, ==, 0u);
}


RC_TEST_STEP(package, errors_point_into_the_source, fix)
{
    rc_wgsl_translate_result result = translate(RC_STR("fn f() {\n  let x = nope;\n}"), BOTH, fix);

    RC_CHECK(result.error.message, ==, RC_STR("there is nothing called 'nope'"));
    RC_CHECK(result.error.line, ==, 2u);
    RC_CHECK(result.error.column, ==, 11u);
    RC_CHECK(result.package.entry_points.num, ==, 0u);
}


RC_TEST_STEP(package, a_target_that_cannot_say_it_fails, fix)
{
    rc_str source = RC_STR(
        "@group(0) @binding(0) var<storage> data: array<f32>;\n"
        "@fragment fn fs() -> @location(0) vec4f { return vec4(data[0]); }\n");
    rc_wgsl_translate_result both = translate(source, BOTH, fix);
    rc_wgsl_translate_result desktop = translate(source, RC_SHADER_TARGET_BIT(GLSL_460), fix);

    RC_CHECK(both.error.message, ==, RC_STR("storage buffers can't be written in GLSL ES 3.00"));
    RC_CHECK_FALSE(rc_wgsl_error_is_set(desktop.error));
    RC_CHECK(rc_view_shader_entry_point_get(desktop.package.entry_points, 0).programs.num, ==, 1u);
}


RC_TEST_STEP(package, the_source_is_its_own, fix)
{
    char source[] = "@fragment fn fs() -> @location(0) vec4f { return vec4(1.0); }";
    rc_wgsl_translate_result result = translate(rc_str_from_cstr(source), BOTH, fix);

    // Scribbling over the caller's copy leaves the package's alone
    source[0] = 'X';
    RC_CHECK_TRUE(result.package.wgsl.data[0] == '@');
    RC_CHECK(rc_view_shader_entry_point_get(result.package.entry_points, 0).name, ==, RC_STR("fs"));
}


RC_TEST_STEP(package, only_the_package_stays_in_the_arena, fix)
{
    rc_arena_mark before = rc_arena_save(&fix->a);
    rc_wgsl_translate_result result = translate(package_test_wgsl.wgsl, BOTH, fix);
    rc_arena_mark after = rc_arena_save(&fix->a);
    uint32_t used = after.chunk == before.chunk ? after.top - before.top : UINT32_MAX;

    // The package is the source, the programs' text, and a little more
    RC_CHECK_FALSE(rc_wgsl_error_is_set(result.error));
    RC_CHECK(used, <, 4 * (package_test_wgsl.wgsl.len + 4096));
}

// ---- as C ----

RC_TEST_STEP(package, the_built_header_matches, fix)
{
    rc_shader_package built = package_test_wgsl;
    rc_shader_package made = translated(fix);

    RC_CHECK(built.targets, ==, made.targets);
    RC_CHECK(built.bindings.num, ==, made.bindings.num);
    RC_CHECK(built.entry_points.num, ==, made.entry_points.num);
    for (uint32_t i = 0; i < made.bindings.num; i++) {
        rc_shader_binding a = rc_view_shader_binding_get(built.bindings, i);
        rc_shader_binding b = rc_view_shader_binding_get(made.bindings, i);

        RC_CHECK(rc_memcmp(&a, &b, sizeof(a)), ==, 0);
    }
    for (uint32_t i = 0; i < made.entry_points.num; i++) {
        rc_shader_entry_point a = rc_view_shader_entry_point_get(built.entry_points, i);
        rc_shader_entry_point b = rc_view_shader_entry_point_get(made.entry_points, i);

        RC_CHECK(a.name, ==, b.name);
        RC_CHECK(a.stage, ==, b.stage);
        RC_CHECK(a.inputs.num, ==, b.inputs.num);
        RC_CHECK(a.outputs.num, ==, b.outputs.num);
        RC_CHECK(a.programs.num, ==, b.programs.num);
        for (uint32_t j = 0; j < a.programs.num; j++) {
            rc_shader_program pa = rc_view_shader_program_get(a.programs, j);
            rc_shader_program pb = rc_view_shader_program_get(b.programs, j);

            RC_CHECK(pa.target, ==, pb.target);
            RC_CHECK(rc_view_bytes_as_str(pa.code), ==, rc_view_bytes_as_str(pb.code));
            RC_CHECK(pa.slots.num, ==, pb.slots.num);
        }
    }
}


RC_TEST_STEP(package, strings_survive_escaping, fix)
{
    // The source's comments hold quotes, backslashes, question marks and a tab
    rc_file_load_text_result file = rc_file_load_text(RC_STR("data/package_test.wgsl"), 0, &fix->a);

    RC_CHECK(file.error, ==, RC_FILE_ERROR_NONE);
    RC_CHECK(package_test_wgsl.wgsl, ==, file.text.view);
}


RC_TEST_STEP(package, as_c, fix)
{
    rc_mstr c = {0};

    rc_shader_package_append_c(&c, translated(fix), RC_STR("my_shader"), &fix->a);
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR("#ifndef RC_SHADER_PACKAGE_MY_SHADER_H_\n")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR("static const rc_shader_binding my_shader_bindings[] = {")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR(".type = RC_SHADER_BINDING_TYPE_UNIFORM_BUFFER")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR("static const rc_shader_package my_shader = {")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR(".programs = {.data = my_shader_1_programs, .num = 2}")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR("static const char my_shader_1_0_code[] = ")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR(".slots = {.data = my_shader_1_0_slots, .num = 2}")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR(".targets = RC_SHADER_TARGET_BIT(GLSL_ES300) | RC_SHADER_TARGET_BIT(GLSL_460)")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR("\\\" \\\\ \\?\\? and a tab:\\t.")));
}


RC_TEST_STEP(package, long_text_is_written_as_bytes, fix)
{
    rc_mstr source = rc_mstr_from_str(RC_STR("// "), 70000, &fix->a);
    rc_mstr c = {0};
    rc_wgsl_translate_result result;

    rc_mstr_append_n(&source, 'x', 66000, &fix->a);
    rc_mstr_append(&source, RC_STR("\nfn f() {}\n"), &fix->a);
    result = translate(source.view, BOTH, fix);
    RC_CHECK_FALSE(rc_wgsl_error_is_set(result.error));
    rc_shader_package_append_c(&c, result.package, RC_STR("big"), &fix->a);
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR("static const char big_wgsl[] = {\n    47, 47, 32")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR(".wgsl = {.data = big_wgsl, .len = 66014}")));
}
