// gfx.h
//
// The GPU, through WebGPU's object model with richc's names: a device and its queue, a surface
// to draw to, and command encoders whose render passes are recorded, finished and submitted.
// Each backend (OpenGL ES 3 / WebGL2 first) keeps WebGPU's behaviour, whatever it sits on.
//
// Objects are handles: small typed structs, where {0} is none. make creates one and release
// destroys it, except where noted (finish consumes its encoder, submit its command buffer).

#ifndef RC_GFX_GFX_H_
#define RC_GFX_GFX_H_

#include "richc/macros.h"

// ---- handles ----

// Generational handles from richc/handle.template.h, so each has _is_none and _is_equal. A handle
// to a released object is caught when it is used.

#define RC_HANDLE_TYPE rc_gfx_device
#include "richc/handle.template.h"

#define RC_HANDLE_TYPE rc_gfx_queue
#include "richc/handle.template.h"

#define RC_HANDLE_TYPE rc_gfx_surface
#include "richc/handle.template.h"

#define RC_HANDLE_TYPE rc_gfx_texture_view
#include "richc/handle.template.h"

#define RC_HANDLE_TYPE rc_gfx_command_encoder
#include "richc/handle.template.h"

#define RC_HANDLE_TYPE rc_gfx_render_pass
#include "richc/handle.template.h"

#define RC_HANDLE_TYPE rc_gfx_command_buffer
#include "richc/handle.template.h"

// ---- device ----

// The GPU, ready to use, or none if there is no usable GPU; why is reported on stderr (the
// console, on wasm). On wasm the page has already set up the context. The GL backend has one
// device at a time.
rc_gfx_device rc_gfx_device_make(void);

// Destroy the device. Everything made from it must already be released.
void rc_gfx_device_release(rc_gfx_device device);

// The device's one queue, which lives as long as the device.
rc_gfx_queue rc_gfx_device_queue(rc_gfx_device device);

// ---- surface ----

// The app window's surface (the canvas, on wasm), or none if it cannot be made; why is reported.
// Presenting waits for the display, so it paces the frames.
rc_gfx_surface rc_gfx_surface_make(rc_gfx_device device);

// Destroy the surface.
void rc_gfx_surface_release(rc_gfx_surface surface);

// A view of the texture to draw this frame into, valid until present.
rc_gfx_texture_view rc_gfx_surface_current_view(rc_gfx_surface surface);

// Show this frame's texture.
void rc_gfx_surface_present(rc_gfx_surface surface);

// ---- commands ----

// A colour, each channel 0 to 1. It is written to an attachment as it is, and only converted if
// the attachment's format is sRGB.
typedef struct rc_gfx_color {
    float r;
    float g;
    float b;
    float a;
} rc_gfx_color;

// What a render pass does with an attachment's contents first. Zero clears.
typedef enum rc_gfx_load_op {
    RC_GFX_LOAD_OP_CLEAR,
    RC_GFX_LOAD_OP_LOAD
} rc_gfx_load_op;

// What a render pass does with an attachment's contents at the end. Zero keeps them.
typedef enum rc_gfx_store_op {
    RC_GFX_STORE_OP_STORE,
    RC_GFX_STORE_OP_DISCARD
} rc_gfx_store_op;

// WebGPU's limit on colour attachments per render pass.
#define RC_GFX_MAX_COLOR_ATTACHMENTS 8

typedef struct rc_gfx_color_attachment {
    rc_gfx_texture_view view;
    uint8_t load_op;   // rc_gfx_load_op
    uint8_t store_op;  // rc_gfx_store_op
    rc_gfx_color clear_value;
} rc_gfx_color_attachment;

typedef struct rc_gfx_render_pass_desc {
    rc_gfx_color_attachment color[RC_GFX_MAX_COLOR_ATTACHMENTS];
    uint32_t color_count;
} rc_gfx_render_pass_desc;

// A new, empty command encoder.
rc_gfx_command_encoder rc_gfx_command_encoder_make(rc_gfx_device device);

// Start recording a render pass. Nothing else may be recorded on the encoder until it ends.
rc_gfx_render_pass rc_gfx_command_encoder_begin_render_pass(rc_gfx_command_encoder encoder, const rc_gfx_render_pass_desc *desc);

// Finish recording the render pass.
void rc_gfx_render_pass_end(rc_gfx_render_pass pass);

// Turn everything recorded into a command buffer, consuming the encoder.
rc_gfx_command_buffer rc_gfx_command_encoder_finish(rc_gfx_command_encoder encoder);

// Run a command buffer on the GPU, consuming it.
void rc_gfx_queue_submit(rc_gfx_queue queue, rc_gfx_command_buffer commands);

#endif // RC_GFX_GFX_H_
