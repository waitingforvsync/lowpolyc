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

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(package) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(package, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(package, fix)
{
    rc_arena_release(&fix->a);
}


// The package translated from the same source as the one compiled in.
static rc_shader_package translated(rc_arena *arena)
{
    rc_wgsl_translate_result result = rc_wgsl_translate(package_test_wgsl.wgsl, arena);

    RC_CHECK_FALSE(rc_wgsl_error_is_set(result.error));
    return result.package;
}

// ---- translating ----

RC_TEST_STEP(package, bindings, fix)
{
    rc_shader_package package = translated(&fix->a);
    rc_shader_binding uniforms = rc_view_shader_binding_get(package.bindings, 0);
    rc_shader_binding colour = rc_view_shader_binding_get(package.bindings, 1);
    rc_shader_binding sampler = rc_view_shader_binding_get(package.bindings, 2);

    RC_CHECK(package.bindings.num, ==, 3u);
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
}


RC_TEST_STEP(package, entry_points, fix)
{
    rc_shader_package package = translated(&fix->a);
    rc_shader_entry_point vs = rc_view_shader_entry_point_get(package.entry_points, 0);
    rc_shader_entry_point fs = rc_view_shader_entry_point_get(package.entry_points, 1);

    RC_CHECK(package.entry_points.num, ==, 2u);
    RC_CHECK(vs.name, ==, RC_STR("vs"));
    RC_CHECK(vs.stage, ==, RC_SHADER_STAGE_VERTEX);
    RC_CHECK(vs.inputs.num, ==, 2u);
    RC_CHECK(rc_view_shader_location_get(vs.inputs, 0).format, ==, RC_SHADER_FORMAT_FLOAT32X3);
    RC_CHECK(rc_view_shader_location_get(vs.inputs, 1).location, ==, 1u);
    RC_CHECK(rc_view_shader_location_get(vs.inputs, 1).format, ==, RC_SHADER_FORMAT_FLOAT32X2);
    RC_CHECK(vs.outputs.num, ==, 0u);
    RC_CHECK(vs.blocks.num, ==, 1u);
    RC_CHECK(rc_view_shader_block_get(vs.blocks, 0).name, ==, RC_STR("rc_ubo_0_0"));
    RC_CHECK(rc_view_shader_block_get(vs.blocks, 0).binding, ==, 0u);
    RC_CHECK(vs.samplers.num, ==, 0u);
    RC_CHECK(fs.stage, ==, RC_SHADER_STAGE_FRAGMENT);
    RC_CHECK(fs.outputs.num, ==, 1u);
    RC_CHECK(rc_view_shader_location_get(fs.outputs, 0).format, ==, RC_SHADER_FORMAT_FLOAT32X4);
    RC_CHECK(fs.samplers.num, ==, 1u);
    RC_CHECK(rc_view_shader_sampler_get(fs.samplers, 0).name, ==, RC_STR("rc_t0_1_s0_2"));
    RC_CHECK(rc_view_shader_sampler_get(fs.samplers, 0).texture, ==, 1u);
    RC_CHECK(rc_view_shader_sampler_get(fs.samplers, 0).sampler, ==, 2u);
    RC_CHECK_TRUE(rc_str_starts_with(fs.glsl_es, RC_STR("#version 300 es\n")));
    RC_CHECK_TRUE(rc_str_starts_with(fs.glsl, RC_STR("#version 460\n")));
}


RC_TEST_STEP(package, find, fix)
{
    rc_shader_package package = translated(&fix->a);

    RC_CHECK(rc_shader_package_find(package, RC_STR("fs")), ==, 1u);
    RC_CHECK(rc_shader_package_find(package, RC_STR("vs")), ==, 0u);
    RC_CHECK(rc_shader_package_find(package, RC_STR("main")), ==, RC_INDEX_NONE);
}


RC_TEST_STEP(package, errors_point_into_the_source, fix)
{
    rc_wgsl_translate_result result = rc_wgsl_translate(RC_STR("fn f() {\n  let x = nope;\n}"),
        &fix->a);

    RC_CHECK(result.error.message, ==, RC_STR("there is nothing called 'nope'"));
    RC_CHECK(result.error.line, ==, 2u);
    RC_CHECK(result.error.column, ==, 11u);
    RC_CHECK(result.package.entry_points.num, ==, 0u);
}


RC_TEST_STEP(package, a_target_that_cannot_say_it_fails, fix)
{
    rc_wgsl_translate_result result = rc_wgsl_translate(RC_STR(
        "@group(0) @binding(0) var<storage> data: array<f32>;\n"
        "@fragment fn fs() -> @location(0) vec4f { return vec4(data[0]); }\n"), &fix->a);

    RC_CHECK(result.error.message, ==, RC_STR("storage buffers can't be written in GLSL ES 3.00"));
}


RC_TEST_STEP(package, the_source_is_its_own, fix)
{
    char source[] = "@fragment fn fs() -> @location(0) vec4f { return vec4(1.0); }";
    rc_wgsl_translate_result result = rc_wgsl_translate(rc_str_from_cstr(source), &fix->a);

    // Scribbling over the caller's copy leaves the package's alone
    source[0] = 'X';
    RC_CHECK_TRUE(result.package.wgsl.data[0] == '@');
    RC_CHECK(rc_view_shader_entry_point_get(result.package.entry_points, 0).name, ==, RC_STR("fs"));
}

// ---- as C ----

RC_TEST_STEP(package, the_built_header_matches, fix)
{
    rc_shader_package built = package_test_wgsl;
    rc_shader_package made = translated(&fix->a);

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
        RC_CHECK(a.glsl_es, ==, b.glsl_es);
        RC_CHECK(a.glsl, ==, b.glsl);
        RC_CHECK(a.stage, ==, b.stage);
        RC_CHECK(a.samplers.num, ==, b.samplers.num);
        RC_CHECK(a.blocks.num, ==, b.blocks.num);
        RC_CHECK(a.inputs.num, ==, b.inputs.num);
        RC_CHECK(a.outputs.num, ==, b.outputs.num);
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

    rc_shader_package_append_c(&c, translated(&fix->a), RC_STR("my_shader"), &fix->a);
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR("#ifndef RC_SHADER_PACKAGE_MY_SHADER_H_\n")));
    RC_CHECK_TRUE(rc_str_contains(c.view,
        RC_STR("static const rc_shader_binding my_shader_bindings[] = {")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR(".type = RC_SHADER_BINDING_TYPE_UNIFORM_BUFFER")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR("static const rc_shader_package my_shader = {")));
    RC_CHECK_TRUE(rc_str_contains(c.view,
        RC_STR(".samplers = {.data = my_shader_1_samplers, .num = 1}")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR(".samplers = {0}")));
    RC_CHECK_TRUE(rc_str_contains(c.view, RC_STR("\\\" \\\\ \\?\\? and a tab:\\t.")));
}
