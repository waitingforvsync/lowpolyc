// placeholder.c
//
// Stands in behind gfx.h on wasm until the GL backend comes there, so everything links.
// The calls do nothing.

#include "richc/gfx/gfx.h"

rc_gfx_device rc_gfx_device_make(void)
{
    return (rc_gfx_device) {0};
}


void rc_gfx_device_release(rc_gfx_device device)
{
    (void)device;
}


rc_gfx_queue rc_gfx_device_queue(rc_gfx_device device)
{
    (void)device;
    return (rc_gfx_queue) {0};
}


rc_gfx_surface rc_gfx_surface_make(rc_gfx_device device)
{
    (void)device;
    return (rc_gfx_surface) {0};
}


void rc_gfx_surface_release(rc_gfx_surface surface)
{
    (void)surface;
}


rc_gfx_texture_view rc_gfx_surface_current_view(rc_gfx_surface surface)
{
    (void)surface;
    return (rc_gfx_texture_view) {0};
}


void rc_gfx_surface_present(rc_gfx_surface surface)
{
    (void)surface;
}


rc_gfx_command_encoder rc_gfx_command_encoder_make(rc_gfx_device device)
{
    (void)device;
    return (rc_gfx_command_encoder) {0};
}


rc_gfx_render_pass rc_gfx_command_encoder_begin_render_pass(rc_gfx_command_encoder encoder, const rc_gfx_render_pass_desc *desc)
{
    (void)encoder;
    RC_ASSERT(desc && desc->color_count <= RC_GFX_MAX_COLOR_ATTACHMENTS);
    return (rc_gfx_render_pass) {0};
}


void rc_gfx_render_pass_end(rc_gfx_render_pass pass)
{
    (void)pass;
}


rc_gfx_command_buffer rc_gfx_command_encoder_finish(rc_gfx_command_encoder encoder)
{
    (void)encoder;
    return (rc_gfx_command_buffer) {0};
}


void rc_gfx_queue_submit(rc_gfx_queue queue, rc_gfx_command_buffer commands)
{
    (void)queue;
    (void)commands;
}
