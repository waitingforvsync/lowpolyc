// gfx.c
//
// The OpenGL ES 3.0 backend behind gfx.h.
//
// Every object lives in a genpool here, and its handle indexes the pool. A GL context belongs to
// one thread, so the backend has one device at a time, and its state is file-static. The pools
// live in the device's arena, and go when it is released.
//
// Encoders record commands rather than calling GL, and submit replays them, so work reaches GL in
// the order WebGPU would run it. A submitted command array is kept for the next encoder to
// reuse, so steady frames allocate nothing.

#include "richc/gfx/gfx.h"

#include "gfx/gl/gl.h"
#include "log.h"
#include "richc/arena.h"
#include "richc/mstr.h"

// ---- commands ----

typedef enum rc_gl_command_type {
    RC_GL_COMMAND_BEGIN_RENDER_PASS,
    RC_GL_COMMAND_END_RENDER_PASS
} rc_gl_command_type;

typedef struct rc_gl_command {
    uint32_t type;  // rc_gl_command_type
    union {
        rc_gfx_render_pass_desc begin_render_pass;
    };
} rc_gl_command;

#define RC_ARRAY_TYPE rc_gl_command
#define RC_ARRAY_NAME gl_command
#include "richc/array.template.h"

#define RC_ARRAY_TYPE rc_array_gl_command
#define RC_ARRAY_NAME gl_command_list
#include "richc/array.template.h"

// ---- objects ----

// A surface: the app window, and the view of it that render passes draw to.
typedef struct rc_gl_surface {
    rc_gl_window window;
    rc_gfx_texture_view view;
} rc_gl_surface;

// A texture view. So far the only kind is a surface's.
typedef struct rc_gl_texture_view {
    rc_gfx_surface surface;
} rc_gl_texture_view;

// A command encoder, and whether it is recording a render pass.
typedef struct rc_gl_encoder {
    rc_array_gl_command commands;
    bool in_render_pass;
} rc_gl_encoder;

typedef struct rc_gl_command_buffer {
    rc_array_gl_command commands;
} rc_gl_command_buffer;

#define RC_GENPOOL_TYPE rc_gl_surface
#define RC_GENPOOL_NAME gl_surface
#define RC_GENPOOL_HANDLE rc_gfx_surface
#include "richc/genpool.template.h"

#define RC_GENPOOL_TYPE rc_gl_texture_view
#define RC_GENPOOL_NAME gl_texture_view
#define RC_GENPOOL_HANDLE rc_gfx_texture_view
#include "richc/genpool.template.h"

#define RC_GENPOOL_TYPE rc_gl_encoder
#define RC_GENPOOL_NAME gl_encoder
#define RC_GENPOOL_HANDLE rc_gfx_command_encoder
#include "richc/genpool.template.h"

#define RC_GENPOOL_TYPE rc_gl_command_buffer
#define RC_GENPOOL_NAME gl_command_buffer
#define RC_GENPOOL_HANDLE rc_gfx_command_buffer
#include "richc/genpool.template.h"

// ---- the backend ----

typedef struct rc_gl_backend {
    // The device, or none. Its queue shares its index and generation.
    rc_gfx_device device;

    // How many devices have been made, which numbers each one's generation. It outlives the
    // devices, so a handle to a released device never matches a later one.
    uint32_t devices_made;

    rc_gl_context context;
    rc_arena arena;

    rc_genpool_gl_surface surfaces;
    rc_genpool_gl_texture_view texture_views;
    rc_genpool_gl_encoder encoders;
    rc_genpool_gl_command_buffer command_buffers;

    // Emptied command arrays, for the next encoders
    rc_array_gl_command_list spare_commands;
} rc_gl_backend;

static rc_gl_backend the_backend;

// True when device is the current device.
static bool rc_gl_device_is_valid(rc_gfx_device device)
{
    return !rc_gfx_device_is_none(device) && rc_gfx_device_is_equal(device, the_backend.device);
}


// True when queue is the current device's.
static bool rc_gl_queue_is_valid(rc_gfx_queue queue)
{
    return rc_gl_device_is_valid((rc_gfx_device) {.index_ = queue.index_, .gen_ = queue.gen_});
}


#ifndef NDEBUG
// Report every GL error raised since the last check, as from where.
static void rc_gl_report_errors(rc_str where)
{
    for (GLenum error = rc_glGetError(); error != GL_NO_ERROR; error = rc_glGetError()) {
        rc_arena scratch = the_backend.arena;
        rc_mstr message = rc_mstr_from_str(where, 64, &scratch);

        rc_mstr_append(&message, RC_STR(": GL error 0x"), &scratch);
        rc_mstr_append_hex16(&message, (uint16_t)error, &scratch);
        rc_app_log(message.view);
    }
}
#endif


// ---- device ----

rc_gfx_device rc_gfx_device_make(void)
{
    if (!rc_gfx_device_is_none(the_backend.device)) {
        rc_app_log(RC_STR("rc_gfx_device_make: the GL backend has one device at a time"));
        return (rc_gfx_device) {0};
    }

    // rc_gl_context_make reports its own failures
    rc_gl_context context = rc_gl_context_make();
    if (!context.context_) {
        return (rc_gfx_device) {0};
    }

    const char *missing = rc_gl_load(&context);
    if (missing) {
        rc_arena scratch = rc_arena_make_default();
        rc_mstr message = rc_mstr_from_str(RC_STR("rc_gfx_device_make: GL has no "), 64, &scratch);

        rc_mstr_append(&message, rc_str_from_cstr(missing), &scratch);
        rc_app_log(message.view);
        rc_arena_release(&scratch);
        rc_gl_context_release(&context);
        return (rc_gfx_device) {0};
    }

    the_backend.devices_made++;
    the_backend.device = (rc_gfx_device) {.index_ = 0, .gen_ = the_backend.devices_made * 2 - 1};
    the_backend.context = context;
    the_backend.arena = rc_arena_make_default();
    return the_backend.device;
}


void rc_gfx_device_release(rc_gfx_device device)
{
    RC_ASSERT(rc_gl_device_is_valid(device));

    // The pools and command arrays all live in the arena
    rc_gl_context_release(&the_backend.context);
    rc_arena_release(&the_backend.arena);
    the_backend = (rc_gl_backend) {.devices_made = the_backend.devices_made};
}


rc_gfx_queue rc_gfx_device_queue(rc_gfx_device device)
{
    RC_ASSERT(rc_gl_device_is_valid(device));
    return (rc_gfx_queue) {.index_ = device.index_, .gen_ = device.gen_};
}


// ---- surface ----

rc_gfx_surface rc_gfx_surface_make(rc_gfx_device device)
{
    RC_ASSERT(rc_gl_device_is_valid(device));

    // rc_gl_window_make reports its own failures
    rc_gl_window window = rc_gl_window_make(&the_backend.context);
    if (!window.surface_) {
        return (rc_gfx_surface) {0};
    }

    rc_gl_backend *b = &the_backend;
    rc_gfx_surface surface = rc_genpool_gl_surface_add(&b->surfaces, (rc_gl_surface) {.window = window}, &b->arena);
    rc_gfx_texture_view view = rc_genpool_gl_texture_view_add(&b->texture_views, (rc_gl_texture_view) {.surface = surface}, &b->arena);

    rc_genpool_gl_surface_at(&b->surfaces, surface)->view = view;
    return surface;
}


void rc_gfx_surface_release(rc_gfx_surface surface)
{
    rc_gl_backend *b = &the_backend;
    rc_gl_surface *s = rc_genpool_gl_surface_at(&b->surfaces, surface);

    rc_genpool_gl_texture_view_remove(&b->texture_views, s->view);
    rc_gl_window_release(&s->window, &b->context);
    rc_genpool_gl_surface_remove(&b->surfaces, surface);
}


rc_gfx_texture_view rc_gfx_surface_current_view(rc_gfx_surface surface)
{
    return rc_genpool_gl_surface_get(&the_backend.surfaces, surface).view;
}


void rc_gfx_surface_present(rc_gfx_surface surface)
{
    rc_gl_window window = rc_genpool_gl_surface_get(&the_backend.surfaces, surface).window;

    rc_gl_window_present(window, &the_backend.context);
}


// ---- recording ----

rc_gfx_command_encoder rc_gfx_command_encoder_make(rc_gfx_device device)
{
    RC_ASSERT(rc_gl_device_is_valid(device));

    rc_gl_backend *b = &the_backend;
    rc_array_gl_command commands = {0};
    if (!rc_array_gl_command_list_is_empty(&b->spare_commands)) {
        commands = rc_array_gl_command_list_pop(&b->spare_commands);
    }
    return rc_genpool_gl_encoder_add(&b->encoders, (rc_gl_encoder) {.commands = commands}, &b->arena);
}


rc_gfx_render_pass rc_gfx_command_encoder_begin_render_pass(rc_gfx_command_encoder encoder, const rc_gfx_render_pass_desc *desc)
{
    rc_gl_backend *b = &the_backend;
    rc_gl_encoder *e = rc_genpool_gl_encoder_at(&b->encoders, encoder);

    RC_ASSERT(!e->in_render_pass);
    RC_ASSERT(desc && desc->color_count <= RC_GFX_MAX_COLOR_ATTACHMENTS);
    for (uint32_t i = 0; i < desc->color_count; i++) {
        RC_ASSERT(rc_genpool_gl_texture_view_is_valid(&b->texture_views, desc->color[i].view));
    }

    rc_gl_command command = {.type = RC_GL_COMMAND_BEGIN_RENDER_PASS, .begin_render_pass = *desc};
    rc_array_gl_command_push(&e->commands, command, &b->arena);
    e->in_render_pass = true;

    // A render pass is its encoder, seen through another type
    return (rc_gfx_render_pass) {.index_ = encoder.index_, .gen_ = encoder.gen_};
}


void rc_gfx_render_pass_end(rc_gfx_render_pass pass)
{
    rc_gl_backend *b = &the_backend;
    rc_gfx_command_encoder encoder = {.index_ = pass.index_, .gen_ = pass.gen_};
    rc_gl_encoder *e = rc_genpool_gl_encoder_at(&b->encoders, encoder);

    RC_ASSERT(e->in_render_pass);
    rc_array_gl_command_push(&e->commands, (rc_gl_command) {.type = RC_GL_COMMAND_END_RENDER_PASS}, &b->arena);
    e->in_render_pass = false;
}


rc_gfx_command_buffer rc_gfx_command_encoder_finish(rc_gfx_command_encoder encoder)
{
    rc_gl_backend *b = &the_backend;
    rc_gl_encoder e = rc_genpool_gl_encoder_get(&b->encoders, encoder);

    RC_ASSERT(!e.in_render_pass);
    rc_genpool_gl_encoder_remove(&b->encoders, encoder);
    return rc_genpool_gl_command_buffer_add(&b->command_buffers, (rc_gl_command_buffer) {.commands = e.commands}, &b->arena);
}


// ---- replay ----

// Start a render pass: bind its framebuffer and clear the attachments that ask for it.
static void rc_gl_begin_render_pass(const rc_gfx_render_pass_desc *desc)
{
    // Every view so far is a surface's, and a surface's framebuffer has the one colour buffer
    RC_ASSERT(desc->color_count <= 1);
    rc_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // Clears are limited by the scissor and the colour mask, but WebGPU's never are
    rc_glDisable(GL_SCISSOR_TEST);
    rc_glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    for (uint32_t i = 0; i < desc->color_count; i++) {
        const rc_gfx_color_attachment *attachment = &desc->color[i];

        if (attachment->load_op == RC_GFX_LOAD_OP_CLEAR) {
            rc_gfx_color c = attachment->clear_value;
            GLfloat value[4] = {c.r, c.g, c.b, c.a};

            rc_glClearBufferfv(GL_COLOR, (GLint)i, value);
        }
    }
}


// End a render pass: let GL drop the attachments it need not keep.
static void rc_gl_end_render_pass(const rc_gfx_render_pass_desc *desc)
{
    for (uint32_t i = 0; i < desc->color_count; i++) {
        if (desc->color[i].store_op == RC_GFX_STORE_OP_DISCARD) {
            // A surface's framebuffer names its colour buffer GL_COLOR
            GLenum attachment = GL_COLOR;

            rc_glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, &attachment);
        }
    }
}


// Make the GL calls the commands stand for.
static void rc_gl_replay(rc_view_gl_command commands)
{
    const rc_gfx_render_pass_desc *pass = NULL;

    for (uint32_t i = 0; i < commands.num; i++) {
        const rc_gl_command *command = rc_view_gl_command_at(commands, i);

        switch (command->type) {
            case RC_GL_COMMAND_BEGIN_RENDER_PASS:
                pass = &command->begin_render_pass;
                rc_gl_begin_render_pass(pass);
                break;

            case RC_GL_COMMAND_END_RENDER_PASS:
                RC_ASSERT(pass);
                rc_gl_end_render_pass(pass);
                pass = NULL;
                break;

            default:
                RC_UNREACHABLE();
        }
    }
}


void rc_gfx_queue_submit(rc_gfx_queue queue, rc_gfx_command_buffer commands)
{
    RC_ASSERT(rc_gl_queue_is_valid(queue));

    rc_gl_backend *b = &the_backend;
    rc_array_gl_command recorded = rc_genpool_gl_command_buffer_get(&b->command_buffers, commands).commands;

    rc_genpool_gl_command_buffer_remove(&b->command_buffers, commands);
    rc_gl_replay(recorded.view);

    // Asking GL for errors can stall it, so only debug builds do
#ifndef NDEBUG
    rc_gl_report_errors(RC_STR("rc_gfx_queue_submit"));
#endif

    // Keep the array for the next encoder
    rc_array_gl_command_reset(&recorded);
    rc_array_gl_command_list_push(&b->spare_commands, recorded, &b->arena);
}
