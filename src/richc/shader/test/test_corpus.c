// test_corpus.c
//
// Translates every shader in data/corpus, each a whole program of the kind richc's users write,
// and saves their GLSL in glsl_out for the build to check with a GLSL compiler if it has one.

#include "richc/shader/wgsl.h"

#include "richc/file.h"
#include "richc/mstr.h"
#include "richc/print.h"
#include "richc/test.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(corpus) {
    rc_arena a;
    rc_arena scratch;
};


RC_TEST_GROUP_INIT(corpus, fix)
{
    fix->a = rc_arena_make_default();
    fix->scratch = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(corpus, fix)
{
    rc_arena_release(&fix->scratch);
    rc_arena_release(&fix->a);
}


// Save one program, its file named for the shader and entry point, with the target and the stage
// as extensions.
static void save(rc_str shader, rc_shader_entry_point entry, rc_shader_target target, rc_arena *arena)
{
    rc_mstr path = rc_mstr_from_str(RC_STR("glsl_out/"), 64, arena);
    bool es = target == RC_SHADER_TARGET_GLSL_ES300;
    rc_str code = rc_view_bytes_as_str(rc_shader_entry_point_program(entry, target).code);

    rc_mstr_append(&path, shader, arena);
    rc_mstr_append_char(&path, '_', arena);
    rc_mstr_append(&path, entry.name, arena);
    rc_mstr_append(&path, es ? RC_STR(".es") : RC_STR(".gl"), arena);
    rc_mstr_append(&path, entry.stage == RC_SHADER_STAGE_VERTEX ? RC_STR(".vert") : RC_STR(".frag"), arena);
    RC_CHECK(rc_file_save_text(path.view, code), ==, RC_FILE_ERROR_NONE);
}


// The number of entry points in data/corpus/<shader>.wgsl, after translating it and saving its
// programs; 0 if it doesn't translate.
static uint32_t translate(const char *shader, rc_arena *arena, rc_arena *scratch)
{
    rc_str name = rc_str_from_cstr(shader);
    rc_mstr path = rc_mstr_from_str(RC_STR("data/corpus/"), 64, arena);
    rc_file_load_text_result file;
    rc_wgsl_translate_result result;

    rc_mstr_append(&path, name, arena);
    rc_mstr_append(&path, RC_STR(".wgsl"), arena);
    file = rc_file_load_text(path.view, 0, arena);
    RC_CHECK(file.error, ==, RC_FILE_ERROR_NONE);
    result = rc_wgsl_translate(file.text.view, RC_SHADER_TARGET_BIT(GLSL_ES300) | RC_SHADER_TARGET_BIT(GLSL_460), arena, *scratch);
    if (rc_wgsl_error_is_set(result.error)) {
        rc_mstr message = rc_mstr_from_str(RC_STR("    "), 0, arena);

        rc_mstr_append(&message, path.view, arena);
        rc_mstr_append_char(&message, ':', arena);
        rc_mstr_append_u32(&message, result.error.line, arena);
        rc_mstr_append_char(&message, ':', arena);
        rc_mstr_append_u32(&message, result.error.column, arena);
        rc_mstr_append(&message, RC_STR(": "), arena);
        rc_mstr_append(&message, result.error.message, arena);
        rc_mstr_append_char(&message, '\n', arena);
        rc_print(message.view);
        return 0;
    }

    for (uint32_t i = 0; i < result.package.entry_points.num; i++) {
        rc_shader_entry_point entry = rc_view_shader_entry_point_get(result.package.entry_points, i);

        save(name, entry, RC_SHADER_TARGET_GLSL_ES300, arena);
        save(name, entry, RC_SHADER_TARGET_GLSL_460, arena);
    }
    return result.package.entry_points.num;
}


RC_TEST_STEP(corpus, triangle, fix)
{
    RC_CHECK(translate("triangle", &fix->a, &fix->scratch), ==, 2u);
}


RC_TEST_STEP(corpus, textured_cube, fix)
{
    RC_CHECK(translate("textured_cube", &fix->a, &fix->scratch), ==, 2u);
}


RC_TEST_STEP(corpus, shadow, fix)
{
    RC_CHECK(translate("shadow", &fix->a, &fix->scratch), ==, 2u);
}


RC_TEST_STEP(corpus, kitchen_sink, fix)
{
    RC_CHECK(translate("kitchen_sink", &fix->a, &fix->scratch), ==, 2u);
}


RC_TEST_STEP(corpus, glsl_clashes, fix)
{
    RC_CHECK(translate("glsl_clashes", &fix->a, &fix->scratch), ==, 2u);
}
