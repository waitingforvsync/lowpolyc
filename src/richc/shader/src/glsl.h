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

#include "writer.h"

// The GLSL program for entry, for target (one of the GLSL targets), allocated from arena; or why
// it can't be written.
rc_writer_result rc_glsl_write(rc_wgsl_module module, rc_wgsl_entry entry, rc_shader_target target, rc_arena *arena);

#endif // RC_SHADER_GLSL_H_
