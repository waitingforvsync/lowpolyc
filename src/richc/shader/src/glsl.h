// glsl.h
//
// Private to richc_shader: writing one entry point of a checked module as a GLSL program, in GLSL
// ES 3.00 for WebGL2 and OpenGL ES, or GLSL 4.60 for desktop OpenGL.
//
// GLSL has no separate samplers, so each texture and sampler used together become one combined
// sampler, named for both their bindings. Buffers become blocks named for their binding too, so a
// backend can find everything by name. Inputs and outputs are named by location: rc_attr_N for
// vertex inputs, rc_var_N between the stages, and rc_frag_N for fragment outputs.
//
// GLSL ES 3.00 maps WebGPU's 0 to 1 clip-space depth onto GL's -1 to 1 at the end of each vertex
// shader. GLSL 4.60 leaves it alone, for a backend that sets glClipControl.

#ifndef RC_SHADER_GLSL_H_
#define RC_SHADER_GLSL_H_

#include "check.h"

typedef enum rc_glsl_dialect {
    RC_GLSL_DIALECT_NONE = 0,
    RC_GLSL_DIALECT_ES300,
    RC_GLSL_DIALECT_460
} rc_glsl_dialect;

// A texture and a sampler used together, as module-scope VAR nodes. sampler is 0 for a texture
// only ever read without one, by textureLoad and the queries.
typedef struct rc_glsl_pair {
    uint32_t texture;
    uint32_t sampler;
} rc_glsl_pair;

#define RC_ARRAY_TYPE rc_glsl_pair
#define RC_ARRAY_NAME glsl_pair
#include "richc/array.template.h"

typedef struct rc_glsl_result {
    rc_str code;
    rc_view_glsl_pair samplers;  // the combined samplers it declares
    rc_view_wgsl_index buffers;  // the uniform and storage VAR nodes it declares blocks for
    rc_wgsl_error error;
} rc_glsl_result;

// The GLSL program for the entry point entry, allocated from arena; or why it can't be written.
rc_glsl_result rc_glsl_write(rc_wgsl_module module, uint32_t entry, rc_glsl_dialect dialect,
    rc_arena *arena);

// Append the name of the combined sampler pair, as rc_t0_1 or rc_t0_1_s0_2.
void rc_glsl_append_sampler_name(rc_mstr *out, rc_wgsl_module module, rc_glsl_pair pair,
    rc_arena *arena);

// Append the name of the block for the buffer var, as rc_ubo_0_1 or rc_ssbo_0_1.
void rc_glsl_append_block_name(rc_mstr *out, rc_wgsl_module module, uint32_t var,
    rc_arena *arena);

#endif // RC_SHADER_GLSL_H_
