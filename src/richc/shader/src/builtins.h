// builtins.h
//
// Private to richc_shader: the names WGSL declares before any shader does. They are types and
// the names that build types (vec3, array, texture_2d), enumerants (address spaces, access modes
// and texel formats) and built-in functions.
//
// A shader's own declarations may reuse these names, and then hide them, so the checker looks
// here only after its own scopes. Names WGSL defines but we don't translate yet are here too,
// marked unsupported, so the error says so rather than calling them unknown.

#ifndef RC_SHADER_BUILTINS_H_
#define RC_SHADER_BUILTINS_H_

#include "richc/str.h"

typedef enum rc_wgsl_predeclared_type {
    RC_WGSL_PREDECLARED_TYPE_NONE = 0,
    RC_WGSL_PREDECLARED_TYPE_GENERATOR,
    RC_WGSL_PREDECLARED_TYPE_ENUMERANT,
    RC_WGSL_PREDECLARED_TYPE_FUNCTION,
    RC_WGSL_PREDECLARED_TYPE_UNSUPPORTED
} rc_wgsl_predeclared_type;

// What a predeclared type name makes, given its template list.
typedef enum rc_wgsl_generator {
    RC_WGSL_GENERATOR_NONE = 0,
    RC_WGSL_GENERATOR_SCALAR,
    RC_WGSL_GENERATOR_VECTOR,
    RC_WGSL_GENERATOR_MATRIX,
    RC_WGSL_GENERATOR_ARRAY,
    RC_WGSL_GENERATOR_ATOMIC,
    RC_WGSL_GENERATOR_POINTER,
    RC_WGSL_GENERATOR_TEXTURE,
    RC_WGSL_GENERATOR_STORAGE_TEXTURE
} rc_wgsl_generator;

// Which set an enumerant belongs to.
typedef enum rc_wgsl_enumerant {
    RC_WGSL_ENUMERANT_NONE = 0,
    RC_WGSL_ENUMERANT_SPACE,
    RC_WGSL_ENUMERANT_ACCESS,
    RC_WGSL_ENUMERANT_FORMAT
} rc_wgsl_enumerant;

// The built-in functions we translate.
typedef enum rc_wgsl_builtin {
    RC_WGSL_BUILTIN_NONE = 0,
    RC_WGSL_BUILTIN_ABS,
    RC_WGSL_BUILTIN_ACOS,
    RC_WGSL_BUILTIN_ACOSH,
    RC_WGSL_BUILTIN_ALL,
    RC_WGSL_BUILTIN_ANY,
    RC_WGSL_BUILTIN_ARRAY_LENGTH,
    RC_WGSL_BUILTIN_ASIN,
    RC_WGSL_BUILTIN_ASINH,
    RC_WGSL_BUILTIN_ATAN,
    RC_WGSL_BUILTIN_ATAN2,
    RC_WGSL_BUILTIN_ATANH,
    RC_WGSL_BUILTIN_BITCAST,
    RC_WGSL_BUILTIN_CEIL,
    RC_WGSL_BUILTIN_CLAMP,
    RC_WGSL_BUILTIN_COS,
    RC_WGSL_BUILTIN_COSH,
    RC_WGSL_BUILTIN_CROSS,
    RC_WGSL_BUILTIN_DEGREES,
    RC_WGSL_BUILTIN_DETERMINANT,
    RC_WGSL_BUILTIN_DISTANCE,
    RC_WGSL_BUILTIN_DOT,
    RC_WGSL_BUILTIN_DPDX,
    RC_WGSL_BUILTIN_DPDX_COARSE,
    RC_WGSL_BUILTIN_DPDX_FINE,
    RC_WGSL_BUILTIN_DPDY,
    RC_WGSL_BUILTIN_DPDY_COARSE,
    RC_WGSL_BUILTIN_DPDY_FINE,
    RC_WGSL_BUILTIN_EXP,
    RC_WGSL_BUILTIN_EXP2,
    RC_WGSL_BUILTIN_FACE_FORWARD,
    RC_WGSL_BUILTIN_FLOOR,
    RC_WGSL_BUILTIN_FMA,
    RC_WGSL_BUILTIN_FRACT,
    RC_WGSL_BUILTIN_FWIDTH,
    RC_WGSL_BUILTIN_FWIDTH_COARSE,
    RC_WGSL_BUILTIN_FWIDTH_FINE,
    RC_WGSL_BUILTIN_INVERSE_SQRT,
    RC_WGSL_BUILTIN_LENGTH,
    RC_WGSL_BUILTIN_LOG,
    RC_WGSL_BUILTIN_LOG2,
    RC_WGSL_BUILTIN_MAX,
    RC_WGSL_BUILTIN_MIN,
    RC_WGSL_BUILTIN_MIX,
    RC_WGSL_BUILTIN_NORMALIZE,
    RC_WGSL_BUILTIN_PACK2X16FLOAT,
    RC_WGSL_BUILTIN_PACK2X16SNORM,
    RC_WGSL_BUILTIN_PACK2X16UNORM,
    RC_WGSL_BUILTIN_POW,
    RC_WGSL_BUILTIN_RADIANS,
    RC_WGSL_BUILTIN_REFLECT,
    RC_WGSL_BUILTIN_REFRACT,
    RC_WGSL_BUILTIN_ROUND,
    RC_WGSL_BUILTIN_SATURATE,
    RC_WGSL_BUILTIN_SELECT,
    RC_WGSL_BUILTIN_SIGN,
    RC_WGSL_BUILTIN_SIN,
    RC_WGSL_BUILTIN_SINH,
    RC_WGSL_BUILTIN_SMOOTHSTEP,
    RC_WGSL_BUILTIN_SQRT,
    RC_WGSL_BUILTIN_STEP,
    RC_WGSL_BUILTIN_TAN,
    RC_WGSL_BUILTIN_TANH,
    RC_WGSL_BUILTIN_TEXTURE_DIMENSIONS,
    RC_WGSL_BUILTIN_TEXTURE_LOAD,
    RC_WGSL_BUILTIN_TEXTURE_NUM_LAYERS,
    RC_WGSL_BUILTIN_TEXTURE_NUM_LEVELS,
    RC_WGSL_BUILTIN_TEXTURE_NUM_SAMPLES,
    RC_WGSL_BUILTIN_TEXTURE_SAMPLE,
    RC_WGSL_BUILTIN_TEXTURE_SAMPLE_BIAS,
    RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE,
    RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE_LEVEL,
    RC_WGSL_BUILTIN_TEXTURE_SAMPLE_GRAD,
    RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL,
    RC_WGSL_BUILTIN_TRANSPOSE,
    RC_WGSL_BUILTIN_TRUNC,
    RC_WGSL_BUILTIN_UNPACK2X16FLOAT,
    RC_WGSL_BUILTIN_UNPACK2X16SNORM,
    RC_WGSL_BUILTIN_UNPACK2X16UNORM
} rc_wgsl_builtin;

// The shapes of signature the built-ins share, which the checker checks their arguments by.
typedef enum rc_wgsl_rule {
    RC_WGSL_RULE_NONE = 0,

    // (T, ...) -> T for a float scalar or vector T
    RC_WGSL_RULE_FLOAT,

    // (T, ...) -> T for a numeric scalar or vector T
    RC_WGSL_RULE_NUMERIC,

    // (T) -> T for a float or signed integer scalar or vector T
    RC_WGSL_RULE_SIGNED,

    // (T) -> T for an f32 scalar or vector T, in fragment shaders only
    RC_WGSL_RULE_DERIVATIVE,

    // (T, ...) -> the scalar of T, a float scalar or vector
    RC_WGSL_RULE_LENGTH,

    // (vecN<T>, vecN<T>) -> T, numeric
    RC_WGSL_RULE_DOT,

    // (vec3<T>, vec3<T>) -> vec3<T>, float
    RC_WGSL_RULE_CROSS,

    // (vecN<T>, ...) -> vecN<T>, float
    RC_WGSL_RULE_FLOAT_VECTOR,

    // (vecN<T>, vecN<T>, T) -> vecN<T>, float
    RC_WGSL_RULE_REFRACT,

    // (T, T, T) -> T, or (vecN<T>, vecN<T>, T) -> vecN<T>, float
    RC_WGSL_RULE_MIX,

    // (matNxN<T>) -> T
    RC_WGSL_RULE_DETERMINANT,

    // (matCxR<T>) -> matRxC<T>
    RC_WGSL_RULE_TRANSPOSE,

    // (bool or vecN<bool>) -> bool
    RC_WGSL_RULE_REDUCE,

    // (T, T, bool) -> T, or (vecN<T>, vecN<T>, vecN<bool>) -> vecN<T>
    RC_WGSL_RULE_SELECT,

    // (vec2<f32>) -> u32, and back
    RC_WGSL_RULE_PACK,
    RC_WGSL_RULE_UNPACK,

    // bitcast<T>(e) for 32-bit T and e of the same width
    RC_WGSL_RULE_BITCAST,

    // (ptr<storage, array<T>>) -> u32
    RC_WGSL_RULE_ARRAY_LENGTH,

    // Each texture function its own way
    RC_WGSL_RULE_TEXTURE
} rc_wgsl_rule;

// One predeclared name. Which fields mean anything depends on its type:
//
//   GENERATOR    a type's name: generator, and element (a shorthand's scalar, or 0 when the
//                template list gives it). A vector's width, a matrix's columns and rows, and a
//                texture's dim and texture_class.
//   ENUMERANT    enumerant (which set it is from) and value
//   FUNCTION     builtin
typedef struct rc_wgsl_predeclared {
    rc_str name;
    uint8_t type;           // rc_wgsl_predeclared_type
    uint8_t generator;      // rc_wgsl_generator
    uint8_t element;        // rc_wgsl_type_id
    uint8_t width;
    uint8_t columns;
    uint8_t rows;
    uint8_t dim;            // rc_shader_dimension
    uint8_t texture_class;  // rc_wgsl_texture_class
    uint8_t enumerant;      // rc_wgsl_enumerant
    uint8_t value;
    uint8_t builtin;        // rc_wgsl_builtin
} rc_wgsl_predeclared;

// True for the texture functions.
bool rc_wgsl_builtin_is_texture(rc_wgsl_builtin builtin);

// True for the texture functions that take a sampler.
bool rc_wgsl_builtin_is_sampled(rc_wgsl_builtin builtin);

// The predeclared name, or one of type NONE if there is no such name.
rc_wgsl_predeclared rc_wgsl_predeclared_find(rc_str name);

// A built-in function: its name as WGSL writes it, its rule, and how many arguments it takes (for
// the rules that fix it).
typedef struct rc_wgsl_builtin_info {
    rc_str name;
    uint8_t rule;  // rc_wgsl_rule
    uint8_t args;
} rc_wgsl_builtin_info;

// What the built-in function is.
rc_wgsl_builtin_info rc_wgsl_builtin_info_of(rc_wgsl_builtin builtin);

// The function's name as WGSL writes it, for errors.
static inline rc_str rc_wgsl_builtin_name(rc_wgsl_builtin builtin)
{
    return rc_wgsl_builtin_info_of(builtin).name;
}

#endif // RC_SHADER_BUILTINS_H_
