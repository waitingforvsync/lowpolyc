// main.c
//
// richc's shader tool: translates a WGSL file into a C header holding its shader package, for the
// build to compile in. CMake's rc_add_shaders runs it for each shader.
//
//   richc_shader_tool input.wgsl output.h [name]
//
// The package's name defaults to the input's file name made into a C identifier, so basic.wgsl
// gives basic_wgsl. Errors give the file, line and column, then the line with a caret beneath.

#include "richc/arena.h"
#include "richc/file.h"
#include "richc/mstr.h"
#include "richc/print.h"
#include "richc/shader/wgsl.h"

// The last part of path, after any slashes.
static rc_str rc_tool_file_name(rc_str path)
{
    uint32_t start = 0;

    for (uint32_t i = 0; i < path.len; i++) {
        if (path.data[i] == '/' || path.data[i] == '\\') {
            start = i + 1;
        }
    }
    return rc_str_drop_first(path, start);
}


// True for a character a C identifier can hold.
static bool rc_tool_is_identifier_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}


// The file name in path made into a C identifier: other characters become underscores, and a
// leading digit gets one before it.
static rc_str rc_tool_identifier(rc_str path, rc_arena *arena)
{
    rc_str file = rc_tool_file_name(path);
    rc_mstr name = rc_mstr_make(file.len + 2, arena);

    if (file.len == 0 || (file.data[0] >= '0' && file.data[0] <= '9')) {
        rc_mstr_append_char(&name, '_', arena);
    }
    for (uint32_t i = 0; i < file.len; i++) {
        rc_mstr_append_char(&name, rc_tool_is_identifier_char(file.data[i]) ? file.data[i] : '_',
            arena);
    }
    return name.view;
}


// The line of source that offset falls in, without its line break.
static rc_str rc_tool_line_at(rc_str source, uint32_t offset)
{
    uint32_t start = offset;
    uint32_t end = offset;

    while (start > 0 && source.data[start - 1] != '\n' && source.data[start - 1] != '\r') {
        start--;
    }
    while (end < source.len && source.data[end] != '\n' && source.data[end] != '\r') {
        end++;
    }
    return rc_str_substr(source, start, end);
}


// Print a translation error as path:line:column, then the line and a caret under the place.
static void rc_tool_print_error(rc_str path, rc_str source, rc_wgsl_error error, rc_arena *arena)
{
    rc_mstr out = rc_mstr_from_str(path, 256, arena);
    rc_str line = rc_tool_line_at(source, error.offset);

    rc_mstr_append_char(&out, ':', arena);
    rc_mstr_append_u32(&out, error.line, arena);
    rc_mstr_append_char(&out, ':', arena);
    rc_mstr_append_u32(&out, error.column, arena);
    rc_mstr_append(&out, RC_STR(": error: "), arena);
    rc_mstr_append(&out, error.message, arena);
    rc_mstr_append(&out, RC_STR("\n    "), arena);
    rc_mstr_append(&out, line, arena);
    rc_mstr_append(&out, RC_STR("\n    "), arena);

    // Tabs stay tabs, so the caret lines up however they are shown
    for (uint32_t i = 0; i + 1 < error.column && i < line.len; i++) {
        rc_mstr_append_char(&out, line.data[i] == '\t' ? '\t' : ' ', arena);
    }
    rc_mstr_append(&out, RC_STR("^\n"), arena);
    rc_print_err(out.view);
}


// Print what went wrong reading or writing the file at path.
static void rc_tool_print_file_error(rc_str path, rc_file_error error, rc_arena *arena)
{
    static const rc_str reasons[] = {
        [RC_FILE_ERROR_NONE] = RC_STR_INIT(""),
        [RC_FILE_ERROR_NOT_FOUND] = RC_STR_INIT("there's no such file"),
        [RC_FILE_ERROR_ACCESS_DENIED] = RC_STR_INIT("access is denied"),
        [RC_FILE_ERROR_TOO_LARGE] = RC_STR_INIT("the file is too large"),
        [RC_FILE_ERROR_IO] = RC_STR_INIT("the file can't be read or written"),
        [RC_FILE_ERROR_BAD_NAME] = RC_STR_INIT("the name isn't a usable path")
    };
    rc_mstr out = rc_mstr_from_str(path, 128, arena);

    rc_mstr_append(&out, RC_STR(": error: "), arena);
    RC_ASSERT(error < RC_COUNT(reasons));
    rc_mstr_append(&out, reasons[error], arena);
    rc_mstr_append_char(&out, '\n', arena);
    rc_print_err(out.view);
}


// Translate input into the header output, as the package name. Returns the exit code.
static int rc_tool_run(rc_str input, rc_str output, rc_str name, rc_arena *arena)
{
    rc_file_load_text_result loaded = rc_file_load_text(input, 0, arena);
    rc_wgsl_translate_result translated;
    rc_mstr header = {0};
    rc_file_error saved;

    if (loaded.error != RC_FILE_ERROR_NONE) {
        rc_tool_print_file_error(input, (rc_file_error)loaded.error, arena);
        return 1;
    }
    translated = rc_wgsl_translate(loaded.text.view, arena);
    if (rc_wgsl_error_is_set(translated.error)) {
        rc_tool_print_error(input, loaded.text.view, translated.error, arena);
        return 1;
    }
    rc_shader_package_append_c(&header, translated.package, name, arena);
    saved = rc_file_save_text(output, header.view);
    if (saved != RC_FILE_ERROR_NONE) {
        rc_tool_print_file_error(output, saved, arena);
        return 1;
    }
    return 0;
}


int main(int argc, char **argv)
{
    rc_arena arena;
    int result;

    if (argc < 3 || argc > 4) {
        rc_print_err(RC_STR("usage: richc_shader_tool input.wgsl output.h [name]\n"));
        return 2;
    }
    arena = rc_arena_make_default();
    result = rc_tool_run(rc_str_from_cstr(argv[1]), rc_str_from_cstr(argv[2]),
        argc == 4 ? rc_str_from_cstr(argv[3]) : rc_tool_identifier(rc_str_from_cstr(argv[1]), &arena),
        &arena);
    rc_arena_release(&arena);
    return result;
}
