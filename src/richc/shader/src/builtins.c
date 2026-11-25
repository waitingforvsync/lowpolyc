// builtins.c
//
// The predeclared names, in one table sorted for a binary search.

#include "builtins.h"

#include "types.h"

#define RC_ARRAY_TYPE rc_wgsl_predeclared
#define RC_ARRAY_NAME wgsl_predeclared
#include "richc/array.template.h"

#define RC_LOWER_BOUND_TYPE rc_wgsl_predeclared
#define RC_LOWER_BOUND_NAME rc_lower_bound_wgsl_predeclared
#define RC_LOWER_BOUND_VIEW rc_view_wgsl_predeclared
#define RC_LOWER_BOUND_KEY(p) ((p).name)
#define RC_LOWER_BOUND_KEY_TYPE rc_str
#define RC_LOWER_BOUND_CMP(a, b) (rc_str_compare((a), (b)) < 0)
#include "richc/algorithm/lower_bound.template.h"

// Rows of the table, one macro for each sort of row. A type's element is an rc_wgsl_type_id
// without its prefix. A depth texture's sampled type is f32 by its very nature, so its class says
// it.
#define RC_WGSL_FN_(text, id) { \
    .name = RC_STR_INIT(text), \
    .type = RC_WGSL_PREDECLARED_TYPE_FUNCTION, \
    .builtin = RC_WGSL_BUILTIN_##id \
}

#define RC_WGSL_UNSUPPORTED_(text) { \
    .name = RC_STR_INIT(text), \
    .type = RC_WGSL_PREDECLARED_TYPE_UNSUPPORTED \
}

#define RC_WGSL_ENUMERANT_(text, set, id) { \
    .name = RC_STR_INIT(text), \
    .type = RC_WGSL_PREDECLARED_TYPE_ENUMERANT, \
    .enumerant = RC_WGSL_ENUMERANT_##set, \
    .value = id \
}

#define RC_WGSL_GENERIC_(text, id) { \
    .name = RC_STR_INIT(text), \
    .type = RC_WGSL_PREDECLARED_TYPE_GENERATOR, \
    .generator = RC_WGSL_GENERATOR_##id \
}

#define RC_WGSL_SCALAR_(text, scalar) { \
    .name = RC_STR_INIT(text), \
    .type = RC_WGSL_PREDECLARED_TYPE_GENERATOR, \
    .generator = RC_WGSL_GENERATOR_SCALAR, \
    .element = RC_WGSL_TYPE_ID_##scalar \
}

#define RC_WGSL_VECTOR_(text, scalar, size) { \
    .name = RC_STR_INIT(text), \
    .type = RC_WGSL_PREDECLARED_TYPE_GENERATOR, \
    .generator = RC_WGSL_GENERATOR_VECTOR, \
    .element = RC_WGSL_TYPE_ID_##scalar, \
    .width = size \
}

#define RC_WGSL_MATRIX_(text, scalar, c, r) { \
    .name = RC_STR_INIT(text), \
    .type = RC_WGSL_PREDECLARED_TYPE_GENERATOR, \
    .generator = RC_WGSL_GENERATOR_MATRIX, \
    .element = RC_WGSL_TYPE_ID_##scalar, \
    .columns = c, \
    .rows = r \
}

#define RC_WGSL_TEXTURE_(text, shape, class) { \
    .name = RC_STR_INIT(text), \
    .type = RC_WGSL_PREDECLARED_TYPE_GENERATOR, \
    .generator = RC_WGSL_GENERATOR_TEXTURE, \
    .dim = RC_SHADER_DIMENSION_##shape, \
    .texture_class = RC_WGSL_TEXTURE_CLASS_##class \
}

#define RC_WGSL_STORAGE_TEXTURE_(text, shape) { \
    .name = RC_STR_INIT(text), \
    .type = RC_WGSL_PREDECLARED_TYPE_GENERATOR, \
    .generator = RC_WGSL_GENERATOR_STORAGE_TEXTURE, \
    .dim = RC_SHADER_DIMENSION_##shape \
}

// Every predeclared name, in byte order.
static const rc_wgsl_predeclared rc_wgsl_predeclared_names[] = {
    RC_WGSL_FN_("abs", ABS),
    RC_WGSL_FN_("acos", ACOS),
    RC_WGSL_FN_("acosh", ACOSH),
    RC_WGSL_FN_("all", ALL),
    RC_WGSL_FN_("any", ANY),
    RC_WGSL_GENERIC_("array", ARRAY),
    RC_WGSL_FN_("arrayLength", ARRAY_LENGTH),
    RC_WGSL_FN_("asin", ASIN),
    RC_WGSL_FN_("asinh", ASINH),
    RC_WGSL_FN_("atan", ATAN),
    RC_WGSL_FN_("atan2", ATAN2),
    RC_WGSL_FN_("atanh", ATANH),
    RC_WGSL_GENERIC_("atomic", ATOMIC),
    RC_WGSL_UNSUPPORTED_("atomicAdd"),
    RC_WGSL_UNSUPPORTED_("atomicAnd"),
    RC_WGSL_UNSUPPORTED_("atomicCompareExchangeWeak"),
    RC_WGSL_UNSUPPORTED_("atomicExchange"),
    RC_WGSL_UNSUPPORTED_("atomicLoad"),
    RC_WGSL_UNSUPPORTED_("atomicMax"),
    RC_WGSL_UNSUPPORTED_("atomicMin"),
    RC_WGSL_UNSUPPORTED_("atomicOr"),
    RC_WGSL_UNSUPPORTED_("atomicStore"),
    RC_WGSL_UNSUPPORTED_("atomicSub"),
    RC_WGSL_UNSUPPORTED_("atomicXor"),
    RC_WGSL_ENUMERANT_("bgra8unorm", FORMAT, RC_WGSL_FORMAT_BGRA8UNORM),
    RC_WGSL_FN_("bitcast", BITCAST),
    RC_WGSL_SCALAR_("bool", BOOL),
    RC_WGSL_FN_("ceil", CEIL),
    RC_WGSL_FN_("clamp", CLAMP),
    RC_WGSL_FN_("cos", COS),
    RC_WGSL_FN_("cosh", COSH),
    RC_WGSL_UNSUPPORTED_("countLeadingZeros"),
    RC_WGSL_UNSUPPORTED_("countOneBits"),
    RC_WGSL_UNSUPPORTED_("countTrailingZeros"),
    RC_WGSL_FN_("cross", CROSS),
    RC_WGSL_FN_("degrees", DEGREES),
    RC_WGSL_FN_("determinant", DETERMINANT),
    RC_WGSL_FN_("distance", DISTANCE),
    RC_WGSL_FN_("dot", DOT),
    RC_WGSL_UNSUPPORTED_("dot4I8Packed"),
    RC_WGSL_UNSUPPORTED_("dot4U8Packed"),
    RC_WGSL_FN_("dpdx", DPDX),
    RC_WGSL_FN_("dpdxCoarse", DPDX_COARSE),
    RC_WGSL_FN_("dpdxFine", DPDX_FINE),
    RC_WGSL_FN_("dpdy", DPDY),
    RC_WGSL_FN_("dpdyCoarse", DPDY_COARSE),
    RC_WGSL_FN_("dpdyFine", DPDY_FINE),
    RC_WGSL_FN_("exp", EXP),
    RC_WGSL_FN_("exp2", EXP2),
    RC_WGSL_UNSUPPORTED_("extractBits"),
    RC_WGSL_UNSUPPORTED_("f16"),
    RC_WGSL_SCALAR_("f32", F32),
    RC_WGSL_FN_("faceForward", FACE_FORWARD),
    RC_WGSL_UNSUPPORTED_("firstLeadingBit"),
    RC_WGSL_UNSUPPORTED_("firstTrailingBit"),
    RC_WGSL_FN_("floor", FLOOR),
    RC_WGSL_FN_("fma", FMA),
    RC_WGSL_FN_("fract", FRACT),
    RC_WGSL_UNSUPPORTED_("frexp"),
    RC_WGSL_ENUMERANT_("function", SPACE, RC_WGSL_SPACE_FUNCTION),
    RC_WGSL_FN_("fwidth", FWIDTH),
    RC_WGSL_FN_("fwidthCoarse", FWIDTH_COARSE),
    RC_WGSL_FN_("fwidthFine", FWIDTH_FINE),
    RC_WGSL_SCALAR_("i32", I32),
    RC_WGSL_UNSUPPORTED_("insertBits"),
    RC_WGSL_FN_("inverseSqrt", INVERSE_SQRT),
    RC_WGSL_UNSUPPORTED_("ldexp"),
    RC_WGSL_FN_("length", LENGTH),
    RC_WGSL_FN_("log", LOG),
    RC_WGSL_FN_("log2", LOG2),
    RC_WGSL_MATRIX_("mat2x2", NONE, 2, 2),
    RC_WGSL_MATRIX_("mat2x2f", F32, 2, 2),
    RC_WGSL_UNSUPPORTED_("mat2x2h"),
    RC_WGSL_MATRIX_("mat2x3", NONE, 2, 3),
    RC_WGSL_MATRIX_("mat2x3f", F32, 2, 3),
    RC_WGSL_UNSUPPORTED_("mat2x3h"),
    RC_WGSL_MATRIX_("mat2x4", NONE, 2, 4),
    RC_WGSL_MATRIX_("mat2x4f", F32, 2, 4),
    RC_WGSL_UNSUPPORTED_("mat2x4h"),
    RC_WGSL_MATRIX_("mat3x2", NONE, 3, 2),
    RC_WGSL_MATRIX_("mat3x2f", F32, 3, 2),
    RC_WGSL_UNSUPPORTED_("mat3x2h"),
    RC_WGSL_MATRIX_("mat3x3", NONE, 3, 3),
    RC_WGSL_MATRIX_("mat3x3f", F32, 3, 3),
    RC_WGSL_UNSUPPORTED_("mat3x3h"),
    RC_WGSL_MATRIX_("mat3x4", NONE, 3, 4),
    RC_WGSL_MATRIX_("mat3x4f", F32, 3, 4),
    RC_WGSL_UNSUPPORTED_("mat3x4h"),
    RC_WGSL_MATRIX_("mat4x2", NONE, 4, 2),
    RC_WGSL_MATRIX_("mat4x2f", F32, 4, 2),
    RC_WGSL_UNSUPPORTED_("mat4x2h"),
    RC_WGSL_MATRIX_("mat4x3", NONE, 4, 3),
    RC_WGSL_MATRIX_("mat4x3f", F32, 4, 3),
    RC_WGSL_UNSUPPORTED_("mat4x3h"),
    RC_WGSL_MATRIX_("mat4x4", NONE, 4, 4),
    RC_WGSL_MATRIX_("mat4x4f", F32, 4, 4),
    RC_WGSL_UNSUPPORTED_("mat4x4h"),
    RC_WGSL_FN_("max", MAX),
    RC_WGSL_FN_("min", MIN),
    RC_WGSL_FN_("mix", MIX),
    RC_WGSL_UNSUPPORTED_("modf"),
    RC_WGSL_FN_("normalize", NORMALIZE),
    RC_WGSL_FN_("pack2x16float", PACK2X16FLOAT),
    RC_WGSL_FN_("pack2x16snorm", PACK2X16SNORM),
    RC_WGSL_FN_("pack2x16unorm", PACK2X16UNORM),
    RC_WGSL_UNSUPPORTED_("pack4x8snorm"),
    RC_WGSL_UNSUPPORTED_("pack4x8unorm"),
    RC_WGSL_UNSUPPORTED_("pack4xI8"),
    RC_WGSL_UNSUPPORTED_("pack4xI8Clamp"),
    RC_WGSL_UNSUPPORTED_("pack4xU8"),
    RC_WGSL_UNSUPPORTED_("pack4xU8Clamp"),
    RC_WGSL_FN_("pow", POW),
    RC_WGSL_ENUMERANT_("private", SPACE, RC_WGSL_SPACE_PRIVATE),
    RC_WGSL_GENERIC_("ptr", POINTER),
    RC_WGSL_UNSUPPORTED_("quantizeToF16"),
    RC_WGSL_ENUMERANT_("r32float", FORMAT, RC_WGSL_FORMAT_R32FLOAT),
    RC_WGSL_ENUMERANT_("r32sint", FORMAT, RC_WGSL_FORMAT_R32SINT),
    RC_WGSL_ENUMERANT_("r32uint", FORMAT, RC_WGSL_FORMAT_R32UINT),
    RC_WGSL_FN_("radians", RADIANS),
    RC_WGSL_ENUMERANT_("read", ACCESS, RC_WGSL_ACCESS_READ),
    RC_WGSL_ENUMERANT_("read_write", ACCESS, RC_WGSL_ACCESS_READ_WRITE),
    RC_WGSL_FN_("reflect", REFLECT),
    RC_WGSL_FN_("refract", REFRACT),
    RC_WGSL_UNSUPPORTED_("reverseBits"),
    RC_WGSL_ENUMERANT_("rg32float", FORMAT, RC_WGSL_FORMAT_RG32FLOAT),
    RC_WGSL_ENUMERANT_("rg32sint", FORMAT, RC_WGSL_FORMAT_RG32SINT),
    RC_WGSL_ENUMERANT_("rg32uint", FORMAT, RC_WGSL_FORMAT_RG32UINT),
    RC_WGSL_ENUMERANT_("rgba16float", FORMAT, RC_WGSL_FORMAT_RGBA16FLOAT),
    RC_WGSL_ENUMERANT_("rgba16sint", FORMAT, RC_WGSL_FORMAT_RGBA16SINT),
    RC_WGSL_ENUMERANT_("rgba16uint", FORMAT, RC_WGSL_FORMAT_RGBA16UINT),
    RC_WGSL_ENUMERANT_("rgba32float", FORMAT, RC_WGSL_FORMAT_RGBA32FLOAT),
    RC_WGSL_ENUMERANT_("rgba32sint", FORMAT, RC_WGSL_FORMAT_RGBA32SINT),
    RC_WGSL_ENUMERANT_("rgba32uint", FORMAT, RC_WGSL_FORMAT_RGBA32UINT),
    RC_WGSL_ENUMERANT_("rgba8sint", FORMAT, RC_WGSL_FORMAT_RGBA8SINT),
    RC_WGSL_ENUMERANT_("rgba8snorm", FORMAT, RC_WGSL_FORMAT_RGBA8SNORM),
    RC_WGSL_ENUMERANT_("rgba8uint", FORMAT, RC_WGSL_FORMAT_RGBA8UINT),
    RC_WGSL_ENUMERANT_("rgba8unorm", FORMAT, RC_WGSL_FORMAT_RGBA8UNORM),
    RC_WGSL_FN_("round", ROUND),
    RC_WGSL_SCALAR_("sampler", SAMPLER),
    RC_WGSL_SCALAR_("sampler_comparison", SAMPLER_COMPARISON),
    RC_WGSL_FN_("saturate", SATURATE),
    RC_WGSL_FN_("select", SELECT),
    RC_WGSL_FN_("sign", SIGN),
    RC_WGSL_FN_("sin", SIN),
    RC_WGSL_FN_("sinh", SINH),
    RC_WGSL_FN_("smoothstep", SMOOTHSTEP),
    RC_WGSL_FN_("sqrt", SQRT),
    RC_WGSL_FN_("step", STEP),
    RC_WGSL_ENUMERANT_("storage", SPACE, RC_WGSL_SPACE_STORAGE),
    RC_WGSL_UNSUPPORTED_("storageBarrier"),
    RC_WGSL_FN_("tan", TAN),
    RC_WGSL_FN_("tanh", TANH),
    RC_WGSL_UNSUPPORTED_("textureBarrier"),
    RC_WGSL_FN_("textureDimensions", TEXTURE_DIMENSIONS),
    RC_WGSL_UNSUPPORTED_("textureGather"),
    RC_WGSL_UNSUPPORTED_("textureGatherCompare"),
    RC_WGSL_FN_("textureLoad", TEXTURE_LOAD),
    RC_WGSL_FN_("textureNumLayers", TEXTURE_NUM_LAYERS),
    RC_WGSL_FN_("textureNumLevels", TEXTURE_NUM_LEVELS),
    RC_WGSL_FN_("textureNumSamples", TEXTURE_NUM_SAMPLES),
    RC_WGSL_FN_("textureSample", TEXTURE_SAMPLE),
    RC_WGSL_UNSUPPORTED_("textureSampleBaseClampToEdge"),
    RC_WGSL_FN_("textureSampleBias", TEXTURE_SAMPLE_BIAS),
    RC_WGSL_FN_("textureSampleCompare", TEXTURE_SAMPLE_COMPARE),
    RC_WGSL_FN_("textureSampleCompareLevel", TEXTURE_SAMPLE_COMPARE_LEVEL),
    RC_WGSL_FN_("textureSampleGrad", TEXTURE_SAMPLE_GRAD),
    RC_WGSL_FN_("textureSampleLevel", TEXTURE_SAMPLE_LEVEL),
    RC_WGSL_UNSUPPORTED_("textureStore"),
    RC_WGSL_TEXTURE_("texture_1d", 1D, SAMPLED),
    RC_WGSL_TEXTURE_("texture_2d", 2D, SAMPLED),
    RC_WGSL_TEXTURE_("texture_2d_array", 2D_ARRAY, SAMPLED),
    RC_WGSL_TEXTURE_("texture_3d", 3D, SAMPLED),
    RC_WGSL_TEXTURE_("texture_cube", CUBE, SAMPLED),
    RC_WGSL_TEXTURE_("texture_cube_array", CUBE_ARRAY, SAMPLED),
    RC_WGSL_TEXTURE_("texture_depth_2d", 2D, DEPTH),
    RC_WGSL_TEXTURE_("texture_depth_2d_array", 2D_ARRAY, DEPTH),
    RC_WGSL_TEXTURE_("texture_depth_cube", CUBE, DEPTH),
    RC_WGSL_TEXTURE_("texture_depth_cube_array", CUBE_ARRAY, DEPTH),
    RC_WGSL_TEXTURE_("texture_depth_multisampled_2d", 2D, DEPTH_MULTISAMPLED),
    RC_WGSL_UNSUPPORTED_("texture_external"),
    RC_WGSL_TEXTURE_("texture_multisampled_2d", 2D, MULTISAMPLED),
    RC_WGSL_STORAGE_TEXTURE_("texture_storage_1d", 1D),
    RC_WGSL_STORAGE_TEXTURE_("texture_storage_2d", 2D),
    RC_WGSL_STORAGE_TEXTURE_("texture_storage_2d_array", 2D_ARRAY),
    RC_WGSL_STORAGE_TEXTURE_("texture_storage_3d", 3D),
    RC_WGSL_FN_("transpose", TRANSPOSE),
    RC_WGSL_FN_("trunc", TRUNC),
    RC_WGSL_SCALAR_("u32", U32),
    RC_WGSL_ENUMERANT_("uniform", SPACE, RC_WGSL_SPACE_UNIFORM),
    RC_WGSL_FN_("unpack2x16float", UNPACK2X16FLOAT),
    RC_WGSL_FN_("unpack2x16snorm", UNPACK2X16SNORM),
    RC_WGSL_FN_("unpack2x16unorm", UNPACK2X16UNORM),
    RC_WGSL_UNSUPPORTED_("unpack4x8snorm"),
    RC_WGSL_UNSUPPORTED_("unpack4x8unorm"),
    RC_WGSL_UNSUPPORTED_("unpack4xI8"),
    RC_WGSL_UNSUPPORTED_("unpack4xU8"),
    RC_WGSL_VECTOR_("vec2", NONE, 2),
    RC_WGSL_VECTOR_("vec2f", F32, 2),
    RC_WGSL_UNSUPPORTED_("vec2h"),
    RC_WGSL_VECTOR_("vec2i", I32, 2),
    RC_WGSL_VECTOR_("vec2u", U32, 2),
    RC_WGSL_VECTOR_("vec3", NONE, 3),
    RC_WGSL_VECTOR_("vec3f", F32, 3),
    RC_WGSL_UNSUPPORTED_("vec3h"),
    RC_WGSL_VECTOR_("vec3i", I32, 3),
    RC_WGSL_VECTOR_("vec3u", U32, 3),
    RC_WGSL_VECTOR_("vec4", NONE, 4),
    RC_WGSL_VECTOR_("vec4f", F32, 4),
    RC_WGSL_UNSUPPORTED_("vec4h"),
    RC_WGSL_VECTOR_("vec4i", I32, 4),
    RC_WGSL_VECTOR_("vec4u", U32, 4),
    RC_WGSL_ENUMERANT_("workgroup", SPACE, RC_WGSL_SPACE_WORKGROUP),
    RC_WGSL_UNSUPPORTED_("workgroupBarrier"),
    RC_WGSL_UNSUPPORTED_("workgroupUniformLoad"),
    RC_WGSL_ENUMERANT_("write", ACCESS, RC_WGSL_ACCESS_WRITE),
};

rc_wgsl_predeclared rc_wgsl_predeclared_find(rc_str name)
{
    rc_view_wgsl_predeclared names = RC_VIEW(rc_wgsl_predeclared_names);
    uint32_t i = rc_lower_bound_wgsl_predeclared(names, name);

    if (i < names.num && rc_str_is_equal(rc_view_wgsl_predeclared_get(names, i).name, name)) {
        return rc_view_wgsl_predeclared_get(names, i);
    }

    return (rc_wgsl_predeclared) {0};
}


rc_wgsl_builtin_info rc_wgsl_builtin_info_of(rc_wgsl_builtin builtin)
{
    static const rc_wgsl_builtin_info infos[] = {
        [RC_WGSL_BUILTIN_NONE] = {RC_STR_INIT(""), RC_WGSL_RULE_NONE, 0},
        [RC_WGSL_BUILTIN_ABS] = {RC_STR_INIT("abs"), RC_WGSL_RULE_NUMERIC, 1},
        [RC_WGSL_BUILTIN_ACOS] = {RC_STR_INIT("acos"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_ACOSH] = {RC_STR_INIT("acosh"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_ALL] = {RC_STR_INIT("all"), RC_WGSL_RULE_REDUCE, 1},
        [RC_WGSL_BUILTIN_ANY] = {RC_STR_INIT("any"), RC_WGSL_RULE_REDUCE, 1},
        [RC_WGSL_BUILTIN_ARRAY_LENGTH] = {RC_STR_INIT("arrayLength"), RC_WGSL_RULE_ARRAY_LENGTH, 1},
        [RC_WGSL_BUILTIN_ASIN] = {RC_STR_INIT("asin"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_ASINH] = {RC_STR_INIT("asinh"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_ATAN] = {RC_STR_INIT("atan"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_ATAN2] = {RC_STR_INIT("atan2"), RC_WGSL_RULE_FLOAT, 2},
        [RC_WGSL_BUILTIN_ATANH] = {RC_STR_INIT("atanh"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_BITCAST] = {RC_STR_INIT("bitcast"), RC_WGSL_RULE_BITCAST, 1},
        [RC_WGSL_BUILTIN_CEIL] = {RC_STR_INIT("ceil"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_CLAMP] = {RC_STR_INIT("clamp"), RC_WGSL_RULE_NUMERIC, 3},
        [RC_WGSL_BUILTIN_COS] = {RC_STR_INIT("cos"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_COSH] = {RC_STR_INIT("cosh"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_CROSS] = {RC_STR_INIT("cross"), RC_WGSL_RULE_CROSS, 2},
        [RC_WGSL_BUILTIN_DEGREES] = {RC_STR_INIT("degrees"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_DETERMINANT] = {RC_STR_INIT("determinant"), RC_WGSL_RULE_DETERMINANT, 1},
        [RC_WGSL_BUILTIN_DISTANCE] = {RC_STR_INIT("distance"), RC_WGSL_RULE_LENGTH, 2},
        [RC_WGSL_BUILTIN_DOT] = {RC_STR_INIT("dot"), RC_WGSL_RULE_DOT, 2},
        [RC_WGSL_BUILTIN_DPDX] = {RC_STR_INIT("dpdx"), RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_DPDX_COARSE] = {RC_STR_INIT("dpdxCoarse"), RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_DPDX_FINE] = {RC_STR_INIT("dpdxFine"), RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_DPDY] = {RC_STR_INIT("dpdy"), RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_DPDY_COARSE] = {RC_STR_INIT("dpdyCoarse"), RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_DPDY_FINE] = {RC_STR_INIT("dpdyFine"), RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_EXP] = {RC_STR_INIT("exp"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_EXP2] = {RC_STR_INIT("exp2"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_FACE_FORWARD] = {RC_STR_INIT("faceForward"), RC_WGSL_RULE_FLOAT_VECTOR, 3},
        [RC_WGSL_BUILTIN_FLOOR] = {RC_STR_INIT("floor"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_FMA] = {RC_STR_INIT("fma"), RC_WGSL_RULE_FLOAT, 3},
        [RC_WGSL_BUILTIN_FRACT] = {RC_STR_INIT("fract"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_FWIDTH] = {RC_STR_INIT("fwidth"), RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_FWIDTH_COARSE] = {RC_STR_INIT("fwidthCoarse"), RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_FWIDTH_FINE] = {RC_STR_INIT("fwidthFine"), RC_WGSL_RULE_DERIVATIVE, 1},
        [RC_WGSL_BUILTIN_INVERSE_SQRT] = {RC_STR_INIT("inverseSqrt"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_LENGTH] = {RC_STR_INIT("length"), RC_WGSL_RULE_LENGTH, 1},
        [RC_WGSL_BUILTIN_LOG] = {RC_STR_INIT("log"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_LOG2] = {RC_STR_INIT("log2"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_MAX] = {RC_STR_INIT("max"), RC_WGSL_RULE_NUMERIC, 2},
        [RC_WGSL_BUILTIN_MIN] = {RC_STR_INIT("min"), RC_WGSL_RULE_NUMERIC, 2},
        [RC_WGSL_BUILTIN_MIX] = {RC_STR_INIT("mix"), RC_WGSL_RULE_MIX, 3},
        [RC_WGSL_BUILTIN_NORMALIZE] = {RC_STR_INIT("normalize"), RC_WGSL_RULE_FLOAT_VECTOR, 1},
        [RC_WGSL_BUILTIN_PACK2X16FLOAT] = {RC_STR_INIT("pack2x16float"), RC_WGSL_RULE_PACK, 1},
        [RC_WGSL_BUILTIN_PACK2X16SNORM] = {RC_STR_INIT("pack2x16snorm"), RC_WGSL_RULE_PACK, 1},
        [RC_WGSL_BUILTIN_PACK2X16UNORM] = {RC_STR_INIT("pack2x16unorm"), RC_WGSL_RULE_PACK, 1},
        [RC_WGSL_BUILTIN_POW] = {RC_STR_INIT("pow"), RC_WGSL_RULE_FLOAT, 2},
        [RC_WGSL_BUILTIN_RADIANS] = {RC_STR_INIT("radians"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_REFLECT] = {RC_STR_INIT("reflect"), RC_WGSL_RULE_FLOAT_VECTOR, 2},
        [RC_WGSL_BUILTIN_REFRACT] = {RC_STR_INIT("refract"), RC_WGSL_RULE_REFRACT, 3},
        [RC_WGSL_BUILTIN_ROUND] = {RC_STR_INIT("round"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_SATURATE] = {RC_STR_INIT("saturate"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_SELECT] = {RC_STR_INIT("select"), RC_WGSL_RULE_SELECT, 3},
        [RC_WGSL_BUILTIN_SIGN] = {RC_STR_INIT("sign"), RC_WGSL_RULE_SIGNED, 1},
        [RC_WGSL_BUILTIN_SIN] = {RC_STR_INIT("sin"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_SINH] = {RC_STR_INIT("sinh"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_SMOOTHSTEP] = {RC_STR_INIT("smoothstep"), RC_WGSL_RULE_FLOAT, 3},
        [RC_WGSL_BUILTIN_SQRT] = {RC_STR_INIT("sqrt"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_STEP] = {RC_STR_INIT("step"), RC_WGSL_RULE_FLOAT, 2},
        [RC_WGSL_BUILTIN_TAN] = {RC_STR_INIT("tan"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_TANH] = {RC_STR_INIT("tanh"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_TEXTURE_DIMENSIONS] = {RC_STR_INIT("textureDimensions"), RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_LOAD] = {RC_STR_INIT("textureLoad"), RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_NUM_LAYERS] = {RC_STR_INIT("textureNumLayers"), RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_NUM_LEVELS] = {RC_STR_INIT("textureNumLevels"), RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_NUM_SAMPLES] = {RC_STR_INIT("textureNumSamples"), RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_SAMPLE] = {RC_STR_INIT("textureSample"), RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_SAMPLE_BIAS] = {RC_STR_INIT("textureSampleBias"), RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE] = {RC_STR_INIT("textureSampleCompare"), RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE_LEVEL] = {RC_STR_INIT("textureSampleCompareLevel"), RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_SAMPLE_GRAD] = {RC_STR_INIT("textureSampleGrad"), RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL] = {RC_STR_INIT("textureSampleLevel"), RC_WGSL_RULE_TEXTURE, 0},
        [RC_WGSL_BUILTIN_TRANSPOSE] = {RC_STR_INIT("transpose"), RC_WGSL_RULE_TRANSPOSE, 1},
        [RC_WGSL_BUILTIN_TRUNC] = {RC_STR_INIT("trunc"), RC_WGSL_RULE_FLOAT, 1},
        [RC_WGSL_BUILTIN_UNPACK2X16FLOAT] = {RC_STR_INIT("unpack2x16float"), RC_WGSL_RULE_UNPACK, 1},
        [RC_WGSL_BUILTIN_UNPACK2X16SNORM] = {RC_STR_INIT("unpack2x16snorm"), RC_WGSL_RULE_UNPACK, 1},
        [RC_WGSL_BUILTIN_UNPACK2X16UNORM] = {RC_STR_INIT("unpack2x16unorm"), RC_WGSL_RULE_UNPACK, 1}
    };

    RC_ASSERT(builtin < RC_COUNT(infos));
    return infos[builtin];
}

#undef RC_WGSL_STORAGE_TEXTURE_
#undef RC_WGSL_TEXTURE_
#undef RC_WGSL_MATRIX_
#undef RC_WGSL_VECTOR_
#undef RC_WGSL_SCALAR_
#undef RC_WGSL_GENERIC_
#undef RC_WGSL_ENUMERANT_
#undef RC_WGSL_UNSUPPORTED_
#undef RC_WGSL_FN_


bool rc_wgsl_builtin_is_sampled(rc_wgsl_builtin builtin)
{
    switch (builtin) {
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE:
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_BIAS:
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE:
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE_LEVEL:
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_GRAD:
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL:
        return true;
    default:
        return false;
    }
}


bool rc_wgsl_builtin_is_texture(rc_wgsl_builtin builtin)
{
    switch (builtin) {
    case RC_WGSL_BUILTIN_TEXTURE_DIMENSIONS:
    case RC_WGSL_BUILTIN_TEXTURE_LOAD:
    case RC_WGSL_BUILTIN_TEXTURE_NUM_LAYERS:
    case RC_WGSL_BUILTIN_TEXTURE_NUM_LEVELS:
    case RC_WGSL_BUILTIN_TEXTURE_NUM_SAMPLES:
        return true;
    default:
        return rc_wgsl_builtin_is_sampled(builtin);
    }
}
