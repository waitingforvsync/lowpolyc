// shader.h
//
// A shader package: one WGSL module, translated for each of richc's gfx backends, with what they
// need to know to bind its resources.
//
// The shader tool writes packages into C headers when a program builds (rc_add_shaders in CMake),
// and rc_wgsl_translate makes them while it runs. A package is plain data, with views into arrays
// and strings beside it, so a generated one sits in static storage.
//
// Bindings are the module's own, in the order it declares them. Each entry point's GLSL names its
// blocks and combined samplers for their bindings; a backend finds them by those names, and binds
// what the package says belongs there.

#ifndef RC_SHADER_SHADER_H_
#define RC_SHADER_SHADER_H_

#include "richc/str.h"

// The index that stands for no binding.
#define RC_SHADER_NONE 0xffffu

typedef enum rc_shader_stage {
    RC_SHADER_STAGE_NONE = 0,
    RC_SHADER_STAGE_VERTEX,
    RC_SHADER_STAGE_FRAGMENT
} rc_shader_stage;

typedef enum rc_shader_binding_type {
    RC_SHADER_BINDING_TYPE_NONE = 0,
    RC_SHADER_BINDING_TYPE_UNIFORM_BUFFER,
    RC_SHADER_BINDING_TYPE_STORAGE_BUFFER,
    RC_SHADER_BINDING_TYPE_READ_ONLY_STORAGE_BUFFER,
    RC_SHADER_BINDING_TYPE_SAMPLER,
    RC_SHADER_BINDING_TYPE_COMPARISON_SAMPLER,
    RC_SHADER_BINDING_TYPE_TEXTURE
} rc_shader_binding_type;

typedef enum rc_shader_dimension {
    RC_SHADER_DIMENSION_NONE = 0,
    RC_SHADER_DIMENSION_1D,
    RC_SHADER_DIMENSION_2D,
    RC_SHADER_DIMENSION_2D_ARRAY,
    RC_SHADER_DIMENSION_3D,
    RC_SHADER_DIMENSION_CUBE,
    RC_SHADER_DIMENSION_CUBE_ARRAY
} rc_shader_dimension;

typedef enum rc_shader_sample {
    RC_SHADER_SAMPLE_NONE = 0,
    RC_SHADER_SAMPLE_FLOAT,
    RC_SHADER_SAMPLE_DEPTH,
    RC_SHADER_SAMPLE_SINT,
    RC_SHADER_SAMPLE_UINT
} rc_shader_sample;

// The type of a value at a location: a vertex input or a fragment output.
typedef enum rc_shader_format {
    RC_SHADER_FORMAT_NONE = 0,
    RC_SHADER_FORMAT_FLOAT32,
    RC_SHADER_FORMAT_FLOAT32X2,
    RC_SHADER_FORMAT_FLOAT32X3,
    RC_SHADER_FORMAT_FLOAT32X4,
    RC_SHADER_FORMAT_SINT32,
    RC_SHADER_FORMAT_SINT32X2,
    RC_SHADER_FORMAT_SINT32X3,
    RC_SHADER_FORMAT_SINT32X4,
    RC_SHADER_FORMAT_UINT32,
    RC_SHADER_FORMAT_UINT32X2,
    RC_SHADER_FORMAT_UINT32X3,
    RC_SHADER_FORMAT_UINT32X4
} rc_shader_format;

// A resource the module binds. A buffer's min_size is its least size in bytes, counting one
// element of an array sized at run time.
typedef struct rc_shader_binding {
    uint32_t min_size;
    uint16_t binding;
    uint8_t group;
    uint8_t type;          // rc_shader_binding_type
    uint8_t stages;        // a bit for each rc_shader_stage that uses it
    uint8_t dimension;     // a texture's rc_shader_dimension
    uint8_t sample_type;   // a texture's rc_shader_sample
    uint8_t multisampled;  // 1 for a multisampled texture
} rc_shader_binding;

// A texture and sampler that GLSL combines into one sampler, by name.
typedef struct rc_shader_sampler {
    rc_str name;
    uint16_t texture;  // its binding's index
    uint16_t sampler;  // its binding's index, or RC_SHADER_NONE for a texture read without one
} rc_shader_sampler;

// A buffer's GLSL block, by name.
typedef struct rc_shader_block {
    rc_str name;
    uint16_t binding;  // its binding's index
} rc_shader_block;

// A value at a location, and its type.
typedef struct rc_shader_location {
    uint8_t location;
    uint8_t format;  // rc_shader_format
} rc_shader_location;

#define RC_ARRAY_TYPE rc_shader_binding
#define RC_ARRAY_NAME shader_binding
#include "richc/array.template.h"

#define RC_ARRAY_TYPE rc_shader_sampler
#define RC_ARRAY_NAME shader_sampler
#include "richc/array.template.h"

#define RC_ARRAY_TYPE rc_shader_block
#define RC_ARRAY_NAME shader_block
#include "richc/array.template.h"

#define RC_ARRAY_TYPE rc_shader_location
#define RC_ARRAY_NAME shader_location
#include "richc/array.template.h"

// One entry point, with its program in each dialect of GLSL and what that program binds.
typedef struct rc_shader_entry_point {
    rc_str name;
    rc_str glsl_es;                    // GLSL ES 3.00, for WebGL2 and OpenGL ES 3
    rc_str glsl;                       // GLSL 4.60, for desktop OpenGL
    rc_view_shader_sampler samplers;
    rc_view_shader_block blocks;
    rc_view_shader_location inputs;    // a vertex shader's attributes
    rc_view_shader_location outputs;   // a fragment shader's colour targets
    uint8_t stage;                     // rc_shader_stage
} rc_shader_entry_point;

#define RC_ARRAY_TYPE rc_shader_entry_point
#define RC_ARRAY_NAME shader_entry_point
#include "richc/array.template.h"

typedef struct rc_shader_package {
    rc_str wgsl;  // the module, for WebGPU
    rc_view_shader_binding bindings;
    rc_view_shader_entry_point entry_points;
} rc_shader_package;

// The index of the entry point called name, or RC_INDEX_NONE if there is none.
static inline uint32_t rc_shader_package_find(rc_shader_package package, rc_str name)
{
    for (uint32_t i = 0; i < package.entry_points.num; i++) {
        if (rc_str_is_equal(rc_view_shader_entry_point_get(package.entry_points, i).name, name)) {
            return i;
        }
    }
    return RC_INDEX_NONE;
}

#endif // RC_SHADER_SHADER_H_
