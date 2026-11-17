// shader.h
//
// A shader package: one WGSL module, translated for the targets richc's gfx backends need, with
// what they need to know to bind its resources.
//
// The shader tool writes packages into C headers when a program builds (rc_add_shaders in CMake),
// and rc_wgsl_translate makes them while it runs. A package is plain data, with views into arrays
// and strings beside it, so a generated one sits in static storage.
//
// Bindings are the module's own, in the order it declares them. Each entry point has a program for
// each target the package was made for, and each program's slots say where it finds the bindings
// it uses: GLSL by the names of its blocks and combined samplers.

#ifndef RC_SHADER_SHADER_H_
#define RC_SHADER_SHADER_H_

#include "richc/bytes.h"

// The index that stands for no binding.
#define RC_SHADER_NONE 0xffffu

// The languages a package can hold programs in.
typedef enum rc_shader_target {
    RC_SHADER_TARGET_NONE = 0,
    RC_SHADER_TARGET_GLSL_ES300,  // GLSL ES 3.00, for WebGL2 and OpenGL ES 3
    RC_SHADER_TARGET_GLSL_460     // GLSL 4.60, for desktop OpenGL
} rc_shader_target;

// The bit for a target in a set of them, as RC_SHADER_TARGET_BIT(GLSL_460).
#define RC_SHADER_TARGET_BIT(name) (1u << RC_SHADER_TARGET_##name)

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

// Where a program finds a binding: in GLSL, the block or combined sampler called name. A
// combined sampler is two bindings, a texture and the sampler GLSL combines with it.
typedef struct rc_shader_slot {
    rc_str name;
    uint16_t binding;  // its binding's index
    uint16_t sampler;  // a combined sampler's sampler binding, or RC_SHADER_NONE
} rc_shader_slot;

// A value at a location, and its type.
typedef struct rc_shader_location {
    uint8_t location;
    uint8_t format;  // rc_shader_format
} rc_shader_location;

#define RC_ARRAY_TYPE rc_shader_binding
#define RC_ARRAY_NAME shader_binding
#include "richc/array.template.h"

#define RC_ARRAY_TYPE rc_shader_slot
#define RC_ARRAY_NAME shader_slot
#include "richc/array.template.h"

#define RC_ARRAY_TYPE rc_shader_location
#define RC_ARRAY_NAME shader_location
#include "richc/array.template.h"

// One entry point's program for one target, and where it finds the bindings it uses. The code is
// text for GLSL, with no terminator.
typedef struct rc_shader_program {
    rc_view_bytes code;
    rc_view_shader_slot slots;
    uint8_t target;  // rc_shader_target
} rc_shader_program;

#define RC_ARRAY_TYPE rc_shader_program
#define RC_ARRAY_NAME shader_program
#include "richc/array.template.h"

// One entry point, with its program for each target the package has, in the targets' order.
typedef struct rc_shader_entry_point {
    rc_str name;
    rc_view_shader_program programs;
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
    uint32_t targets;  // an RC_SHADER_TARGET_BIT for each target it has programs for
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

// The entry point's program for target, or one with no code if the package has none.
static inline rc_shader_program rc_shader_entry_point_program(rc_shader_entry_point entry, rc_shader_target target)
{
    for (uint32_t i = 0; i < entry.programs.num; i++) {
        if (rc_view_shader_program_get(entry.programs, i).target == target) {
            return rc_view_shader_program_get(entry.programs, i);
        }
    }
    return (rc_shader_program) {0};
}

#endif // RC_SHADER_SHADER_H_
