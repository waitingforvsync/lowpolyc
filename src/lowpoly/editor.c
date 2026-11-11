// editor.c
//
// The mesh editor's top level. For now it clears the window to one of two colours, and space
// swaps between them.

#include "editor.h"

#include "richc/app/app.h"
#include "richc/gfx/gfx.h"

typedef struct editor {
    rc_gfx_device device;
    rc_gfx_queue queue;
    rc_gfx_surface surface;
    bool alt_colour;
} editor;

// On wasm, rc_app_run returns before the first frame, so the state cannot live on the stack.
static editor the_editor;

// The colour to clear the window to.
static rc_gfx_color editor_clear_colour(const editor *ed)
{
    if (ed->alt_colour) {
        return (rc_gfx_color) {.r = 0.10f, .g = 0.20f, .b = 0.45f, .a = 1.0f};
    }
    return (rc_gfx_color) {.r = 0.12f, .g = 0.12f, .b = 0.12f, .a = 1.0f};
}


static void editor_init(void *user)
{
    editor *ed = user;

    ed->device = rc_gfx_device_make();
    ed->queue = rc_gfx_device_queue(ed->device);
    ed->surface = rc_gfx_surface_make(ed->device);
}


static void editor_event(void *user, const rc_app_event *event)
{
    editor *ed = user;

    if (event->type == RC_APP_EVENT_KEY_DOWN && event->key == RC_KEY_SPACE) {
        ed->alt_colour = !ed->alt_colour;
    }
}


static void editor_frame(void *user)
{
    editor *ed = user;
    rc_gfx_command_encoder encoder = rc_gfx_command_encoder_make(ed->device);
    rc_gfx_render_pass pass = rc_gfx_command_encoder_begin_render_pass(encoder, &(rc_gfx_render_pass_desc) {
        .color = {{
            .view = rc_gfx_surface_current_view(ed->surface),
            .load_op = RC_GFX_LOAD_OP_CLEAR,
            .clear_value = editor_clear_colour(ed)
        }},
        .color_count = 1
    });

    rc_gfx_render_pass_end(pass);
    rc_gfx_queue_submit(ed->queue, rc_gfx_command_encoder_finish(encoder));
    rc_gfx_surface_present(ed->surface);
}


static void editor_deinit(void *user)
{
    editor *ed = user;

    rc_gfx_surface_release(ed->surface);
    rc_gfx_device_release(ed->device);
}


int32_t editor_run(void)
{
    return rc_app_run(&(rc_app_desc) {
        .title = RC_STR("Lowpoly"),
        .width = 1280,
        .height = 720,
        .user = &the_editor,
        .init = editor_init,
        .event = editor_event,
        .frame = editor_frame,
        .deinit = editor_deinit
    });
}


#ifdef LOWPOLY_TESTS

#include "richc/test.h"


RC_TEST(editor, space_toggles_colour)
{
    editor ed = {0};
    rc_app_event space = {.type = RC_APP_EVENT_KEY_DOWN, .key = RC_KEY_SPACE};
    rc_gfx_color first = editor_clear_colour(&ed);

    editor_event(&ed, &space);
    RC_CHECK(editor_clear_colour(&ed).b, !=, first.b);
    editor_event(&ed, &space);
    RC_CHECK(editor_clear_colour(&ed).b, ==, first.b);
}


RC_TEST(editor, other_events_leave_colour)
{
    editor ed = {0};
    rc_app_event release = {.type = RC_APP_EVENT_KEY_UP, .key = RC_KEY_SPACE};
    rc_app_event other = {.type = RC_APP_EVENT_KEY_DOWN, .key = RC_KEY_UNKNOWN};

    editor_event(&ed, &release);
    editor_event(&ed, &other);
    RC_CHECK_FALSE(ed.alt_colour);
}

#endif
