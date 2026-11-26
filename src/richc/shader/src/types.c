// types.c
//
// The type table, interned through a hash index.

#include "types.h"

#include "richc/hash.h"
#include "richc/mstr.h"

// ---- the table ----

// A type of the given type with nothing else to it: a scalar or a sampler.
static rc_wgsl_typeinfo rc_wgsl_typeinfo_simple(rc_wgsl_typeinfo_type type)
{
    return (rc_wgsl_typeinfo) {.type = (uint8_t)type};
}


// A type's kind and dimensions, one byte each, packed into a word.
static uint32_t rc_wgsl_typeinfo_shape(rc_wgsl_typeinfo t)
{
    return (uint32_t)t.type | (uint32_t)t.width << 8 | (uint32_t)t.columns << 16 | (uint32_t)t.rows << 24;
}


// A type's address space, access mode, texture dimension and texture class, packed into a word.
static uint32_t rc_wgsl_typeinfo_memory(rc_wgsl_typeinfo t)
{
    return (uint32_t)t.space | (uint32_t)t.access << 8 | (uint32_t)t.dim << 16 | (uint32_t)t.texture_class << 24;
}


// A type's texel format and whether it can be copied, packed into a word.
static uint32_t rc_wgsl_typeinfo_extra(rc_wgsl_typeinfo t)
{
    return (uint32_t)t.format | (uint32_t)t.uncopyable << 8;
}


// True if a and b are the same type, field for field. The packed fields are a byte each, so
// comparing them packed is the same as comparing them one by one.
static bool rc_wgsl_typeinfo_is_equal(rc_wgsl_typeinfo a, rc_wgsl_typeinfo b)
{
    if (a.element != b.element || a.count != b.count) {
        return false;
    }

    if (a.store_type != b.store_type || a.sampled_type != b.sampled_type) {
        return false;
    }

    if (a.decl != b.decl || rc_wgsl_typeinfo_shape(a) != rc_wgsl_typeinfo_shape(b)) {
        return false;
    }

    return rc_wgsl_typeinfo_memory(a) == rc_wgsl_typeinfo_memory(b) && rc_wgsl_typeinfo_extra(a) == rc_wgsl_typeinfo_extra(b);
}


// The hash of every field is_equal compares.
static uint32_t rc_wgsl_typeinfo_hash(rc_wgsl_typeinfo t)
{
    uint32_t hash = rc_hash_combine(rc_hash_u32(t.element), rc_hash_u32(t.count));

    hash = rc_hash_combine(hash, rc_hash_u32(t.store_type ^ t.sampled_type << 16));
    hash = rc_hash_combine(hash, rc_hash_u32(t.decl));
    hash = rc_hash_combine(hash, rc_hash_u32(rc_wgsl_typeinfo_shape(t)));
    hash = rc_hash_combine(hash, rc_hash_u32(rc_wgsl_typeinfo_memory(t)));
    return rc_hash_combine(hash, rc_hash_u32(rc_wgsl_typeinfo_extra(t)));
}

#define RC_HASH_INDEX_KEY_TYPE rc_wgsl_typeinfo
#define RC_HASH_INDEX_DATA_TYPE rc_view_wgsl_typeinfo
#define RC_HASH_INDEX_KEY_AT(data, index) (rc_view_wgsl_typeinfo_get(data, index))
#define RC_HASH_INDEX_HASH(key) rc_wgsl_typeinfo_hash(key)
#define RC_HASH_INDEX_EQUAL(a, b) rc_wgsl_typeinfo_is_equal(a, b)
#define RC_HASH_INDEX_NAME wgsl_typeinfo
#include "richc/hash_index.template.h"

rc_wgsl_types rc_wgsl_types_make(rc_arena *arena)
{
    static const uint8_t fixed[] = {
        [RC_WGSL_TYPE_ID_NONE] = RC_WGSL_TYPEINFO_TYPE_NONE,
        [RC_WGSL_TYPE_ID_BOOL] = RC_WGSL_TYPEINFO_TYPE_BOOL,
        [RC_WGSL_TYPE_ID_ABSTRACT_INT] = RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT,
        [RC_WGSL_TYPE_ID_ABSTRACT_FLOAT] = RC_WGSL_TYPEINFO_TYPE_ABSTRACT_FLOAT,
        [RC_WGSL_TYPE_ID_I32] = RC_WGSL_TYPEINFO_TYPE_I32,
        [RC_WGSL_TYPE_ID_U32] = RC_WGSL_TYPEINFO_TYPE_U32,
        [RC_WGSL_TYPE_ID_F32] = RC_WGSL_TYPEINFO_TYPE_F32,
        [RC_WGSL_TYPE_ID_SAMPLER] = RC_WGSL_TYPEINFO_TYPE_SAMPLER,
        [RC_WGSL_TYPE_ID_SAMPLER_COMPARISON] = RC_WGSL_TYPEINFO_TYPE_SAMPLER_COMPARISON
    };
    rc_wgsl_types types = {
        .infos = rc_array_wgsl_typeinfo_make(64, arena),
        .index = rc_hash_index_make(64, arena)
    };

    for (uint32_t i = 0; i < RC_COUNT(fixed); i++) {
        rc_wgsl_typeinfo_type type = (rc_wgsl_typeinfo_type)fixed[i];

        rc_wgsl_type_intern(&types, rc_wgsl_typeinfo_simple(type), arena);
    }
    return types;
}


rc_wgsl_types rc_wgsl_types_from_view(rc_view_wgsl_typeinfo view, rc_arena *arena)
{
    rc_wgsl_types types = {
        .infos = rc_array_wgsl_typeinfo_from_view(view, view.num + 16, arena),
        .index = rc_hash_index_make(view.num + 16, arena)
    };

    for (uint32_t i = 0; i < view.num; i++) {
        rc_hash_index_wgsl_typeinfo_add(&types.index, types.infos.view, i, arena);
    }
    return types;
}


uint32_t rc_wgsl_type_intern(rc_wgsl_types *types, rc_wgsl_typeinfo type, rc_arena *arena)
{
    uint32_t i = rc_hash_index_wgsl_typeinfo_find_or_add(&types->index, types->infos.view, type, types->infos.num, arena);

    if (i == types->infos.num) {
        rc_array_wgsl_typeinfo_push(&types->infos, type, arena);
    }

    return i;
}


uint8_t rc_wgsl_type_depth_around(rc_view_wgsl_typeinfo types, uint32_t element)
{
    uint8_t depth = rc_wgsl_typeinfo_of(types, element).depth;

    return depth < UINT8_MAX ? (uint8_t)(depth + 1) : UINT8_MAX;
}


uint32_t rc_wgsl_type_vector(rc_wgsl_types *types, uint32_t element, uint32_t width, rc_arena *arena)
{
    rc_wgsl_typeinfo type = {
        .element = element,
        .type = RC_WGSL_TYPEINFO_TYPE_VECTOR,
        .width = (uint8_t)width,
        .depth = 1
    };

    RC_ASSERT(width >= 2 && width <= 4);
    return rc_wgsl_type_intern(types, type, arena);
}


uint32_t rc_wgsl_type_matrix(rc_wgsl_types *types, uint32_t element, uint32_t columns, uint32_t rows, rc_arena *arena)
{
    rc_wgsl_typeinfo type = {
        .element = element,
        .type = RC_WGSL_TYPEINFO_TYPE_MATRIX,
        .columns = (uint8_t)columns,
        .rows = (uint8_t)rows,
        .depth = 1
    };

    RC_ASSERT(columns >= 2 && columns <= 4);
    RC_ASSERT(rows >= 2 && rows <= 4);
    return rc_wgsl_type_intern(types, type, arena);
}


uint32_t rc_wgsl_type_array(rc_wgsl_types *types, uint32_t element, uint32_t count, rc_arena *arena)
{
    rc_wgsl_typeinfo type = {
        .element = element,
        .count = count,
        .type = RC_WGSL_TYPEINFO_TYPE_ARRAY,
        .depth = rc_wgsl_type_depth_around(types->infos.view, element)
    };

    return rc_wgsl_type_intern(types, type, arena);
}


uint32_t rc_wgsl_type_pointer(rc_wgsl_types *types, uint32_t store_type, rc_wgsl_space space, rc_wgsl_access access, rc_arena *arena)
{
    rc_wgsl_typeinfo type = {
        .store_type = store_type,
        .type = RC_WGSL_TYPEINFO_TYPE_POINTER,
        .space = (uint8_t)space,
        .access = (uint8_t)access,
        .depth = rc_wgsl_type_depth_around(types->infos.view, store_type)
    };

    return rc_wgsl_type_intern(types, type, arena);
}


uint32_t rc_wgsl_type_with_scalar(rc_wgsl_types *types, uint32_t id, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_typeinfo type = rc_array_wgsl_typeinfo_get(&types->infos, id);

    switch ((rc_wgsl_typeinfo_type)type.type) {
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
        return rc_wgsl_type_vector(types, scalar, type.width, arena);
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
        return rc_wgsl_type_matrix(types, scalar, type.columns, type.rows, arena);
    case RC_WGSL_TYPEINFO_TYPE_ARRAY:
        return rc_wgsl_type_array(types, rc_wgsl_type_with_scalar(types, type.element, scalar, arena), type.count, arena);
    default:
        return scalar;
    }
}


uint32_t rc_wgsl_type_column(rc_wgsl_types *types, uint32_t matrix, rc_arena *arena)
{
    rc_wgsl_typeinfo type = rc_array_wgsl_typeinfo_get(&types->infos, matrix);

    return rc_wgsl_type_vector(types, type.element, type.rows, arena);
}

// ---- questions ----

bool rc_wgsl_type_is_scalar(rc_view_wgsl_typeinfo types, uint32_t id)
{
    rc_wgsl_typeinfo_type type = rc_wgsl_typeinfo_type_of(types, id);

    return type >= RC_WGSL_TYPEINFO_TYPE_BOOL && type <= RC_WGSL_TYPEINFO_TYPE_F32;
}


bool rc_wgsl_type_is_numeric_scalar(rc_view_wgsl_typeinfo types, uint32_t id)
{
    rc_wgsl_typeinfo_type type = rc_wgsl_typeinfo_type_of(types, id);

    return type >= RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT && type <= RC_WGSL_TYPEINFO_TYPE_F32;
}


bool rc_wgsl_type_is_integer_scalar(rc_view_wgsl_typeinfo types, uint32_t id)
{
    rc_wgsl_typeinfo_type type = rc_wgsl_typeinfo_type_of(types, id);

    switch (type) {
    case RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT:
    case RC_WGSL_TYPEINFO_TYPE_I32:
    case RC_WGSL_TYPEINFO_TYPE_U32:
        return true;
    default:
        return false;
    }
}


bool rc_wgsl_type_is_float_scalar(rc_view_wgsl_typeinfo types, uint32_t id)
{
    rc_wgsl_typeinfo_type type = rc_wgsl_typeinfo_type_of(types, id);

    return type == RC_WGSL_TYPEINFO_TYPE_ABSTRACT_FLOAT || type == RC_WGSL_TYPEINFO_TYPE_F32;
}


uint32_t rc_wgsl_type_scalar_of(rc_view_wgsl_typeinfo types, uint32_t id)
{
    rc_wgsl_typeinfo type = rc_wgsl_typeinfo_of(types, id);

    if (type.type == RC_WGSL_TYPEINFO_TYPE_VECTOR || type.type == RC_WGSL_TYPEINFO_TYPE_MATRIX) {
        return type.element;
    }

    return rc_wgsl_type_is_scalar(types, id) ? id : 0;
}


uint32_t rc_wgsl_type_leaf_scalar(rc_view_wgsl_typeinfo types, uint32_t id)
{
    rc_wgsl_typeinfo type = rc_wgsl_typeinfo_of(types, id);

    if (type.type == RC_WGSL_TYPEINFO_TYPE_ARRAY) {
        return rc_wgsl_type_leaf_scalar(types, type.element);
    }

    return rc_wgsl_type_scalar_of(types, id);
}


bool rc_wgsl_type_is_scalar_or_vector(rc_view_wgsl_typeinfo types, uint32_t id)
{
    return rc_wgsl_type_is_scalar(types, id) || rc_wgsl_typeinfo_type_of(types, id) == RC_WGSL_TYPEINFO_TYPE_VECTOR;
}


uint32_t rc_wgsl_type_width_of(rc_view_wgsl_typeinfo types, uint32_t id)
{
    rc_wgsl_typeinfo type = rc_wgsl_typeinfo_of(types, id);

    return type.type == RC_WGSL_TYPEINFO_TYPE_VECTOR ? type.width : 1;
}


uint32_t rc_wgsl_dimension_coords(rc_shader_dimension dim)
{
    switch (dim) {
    case RC_SHADER_DIMENSION_1D:
        return 1;
    case RC_SHADER_DIMENSION_2D:
    case RC_SHADER_DIMENSION_2D_ARRAY:
        return 2;
    default:
        return 3;
    }
}


uint32_t rc_wgsl_type_element_count(rc_view_wgsl_typeinfo types, uint32_t id)
{
    rc_wgsl_typeinfo t = rc_wgsl_typeinfo_of(types, id);

    switch ((rc_wgsl_typeinfo_type)t.type) {
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
        return t.width;
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
        return t.columns;
    default:
        return t.count;
    }
}


bool rc_wgsl_type_is_abstract(rc_view_wgsl_typeinfo types, uint32_t id)
{
    rc_wgsl_typeinfo type = rc_wgsl_typeinfo_of(types, id);

    switch ((rc_wgsl_typeinfo_type)type.type) {
    case RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT:
    case RC_WGSL_TYPEINFO_TYPE_ABSTRACT_FLOAT:
        return true;
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
    case RC_WGSL_TYPEINFO_TYPE_ARRAY:
        return rc_wgsl_type_is_abstract(types, type.element);
    default:
        return false;
    }
}


bool rc_wgsl_type_is_constructible(rc_view_wgsl_typeinfo types, uint32_t id)
{
    rc_wgsl_typeinfo type = rc_wgsl_typeinfo_of(types, id);

    switch ((rc_wgsl_typeinfo_type)type.type) {
    case RC_WGSL_TYPEINFO_TYPE_BOOL:
    case RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT:
    case RC_WGSL_TYPEINFO_TYPE_ABSTRACT_FLOAT:
    case RC_WGSL_TYPEINFO_TYPE_I32:
    case RC_WGSL_TYPEINFO_TYPE_U32:
    case RC_WGSL_TYPEINFO_TYPE_F32:
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
        return true;
    case RC_WGSL_TYPEINFO_TYPE_ARRAY:
        return type.count != 0 && rc_wgsl_type_is_constructible(types, type.element);
    case RC_WGSL_TYPEINFO_TYPE_STRUCT:
        return !type.uncopyable;
    default:
        return false;
    }
}


uint32_t rc_wgsl_type_concretise(rc_wgsl_types *types, uint32_t id, rc_arena *arena)
{
    rc_wgsl_typeinfo type = rc_array_wgsl_typeinfo_get(&types->infos, id);

    switch ((rc_wgsl_typeinfo_type)type.type) {
    case RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT:
        return RC_WGSL_TYPE_ID_I32;
    case RC_WGSL_TYPEINFO_TYPE_ABSTRACT_FLOAT:
        return RC_WGSL_TYPE_ID_F32;
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
        return rc_wgsl_type_with_scalar(types, id, rc_wgsl_type_concretise(types, type.element, arena), arena);
    case RC_WGSL_TYPEINFO_TYPE_ARRAY:
        return rc_wgsl_type_array(types, rc_wgsl_type_concretise(types, type.element, arena), type.count, arena);
    default:
        return id;
    }
}


// True if the scalar from converts to the scalar to.
static bool rc_wgsl_scalar_converts(rc_view_wgsl_typeinfo types, uint32_t from, uint32_t to)
{
    rc_wgsl_typeinfo_type source = rc_wgsl_typeinfo_type_of(types, from);
    rc_wgsl_typeinfo_type target = rc_wgsl_typeinfo_type_of(types, to);

    if (from == to) {
        return true;
    }

    // An abstract integer converts to any other numeric scalar
    if (source == RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT) {
        return target != RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT && rc_wgsl_type_is_numeric_scalar(types, to);
    }

    return source == RC_WGSL_TYPEINFO_TYPE_ABSTRACT_FLOAT && target == RC_WGSL_TYPEINFO_TYPE_F32;
}


bool rc_wgsl_type_converts(rc_view_wgsl_typeinfo types, uint32_t from, uint32_t to)
{
    rc_wgsl_typeinfo a = rc_wgsl_typeinfo_of(types, from);
    rc_wgsl_typeinfo b = rc_wgsl_typeinfo_of(types, to);

    if (from == to) {
        return true;
    }

    if (a.type != b.type) {
        return rc_wgsl_type_is_scalar(types, from) && rc_wgsl_scalar_converts(types, from, to);
    }

    switch ((rc_wgsl_typeinfo_type)a.type) {
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
        return rc_wgsl_typeinfo_shape(a) == rc_wgsl_typeinfo_shape(b) && rc_wgsl_scalar_converts(types, a.element, b.element);
    case RC_WGSL_TYPEINFO_TYPE_ARRAY:
        return a.count == b.count && rc_wgsl_type_converts(types, a.element, b.element);
    default:
        return false;
    }
}

// ---- names ----

// The name of an address space, as WGSL writes it.
static rc_str rc_wgsl_space_name(rc_wgsl_space space)
{
    static const rc_str names[] = {
        [RC_WGSL_SPACE_NONE] = RC_STR_INIT(""),
        [RC_WGSL_SPACE_FUNCTION] = RC_STR_INIT("function"),
        [RC_WGSL_SPACE_PRIVATE] = RC_STR_INIT("private"),
        [RC_WGSL_SPACE_WORKGROUP] = RC_STR_INIT("workgroup"),
        [RC_WGSL_SPACE_UNIFORM] = RC_STR_INIT("uniform"),
        [RC_WGSL_SPACE_STORAGE] = RC_STR_INIT("storage"),
        [RC_WGSL_SPACE_HANDLE] = RC_STR_INIT("handle")
    };

    RC_ASSERT((uint32_t)space < RC_COUNT(names));
    return names[space];
}


// The name of an access mode, as WGSL writes it.
static rc_str rc_wgsl_access_name(rc_wgsl_access access)
{
    static const rc_str names[] = {
        [RC_WGSL_ACCESS_NONE] = RC_STR_INIT(""),
        [RC_WGSL_ACCESS_READ] = RC_STR_INIT("read"),
        [RC_WGSL_ACCESS_WRITE] = RC_STR_INIT("write"),
        [RC_WGSL_ACCESS_READ_WRITE] = RC_STR_INIT("read_write")
    };

    RC_ASSERT((uint32_t)access < RC_COUNT(names));
    return names[access];
}


// The name of a texture's dimension, as a suffix: 2d_array.
static rc_str rc_wgsl_dim_name(rc_shader_dimension dim)
{
    static const rc_str names[] = {
        [RC_SHADER_DIMENSION_NONE] = RC_STR_INIT(""),
        [RC_SHADER_DIMENSION_1D] = RC_STR_INIT("1d"),
        [RC_SHADER_DIMENSION_2D] = RC_STR_INIT("2d"),
        [RC_SHADER_DIMENSION_2D_ARRAY] = RC_STR_INIT("2d_array"),
        [RC_SHADER_DIMENSION_3D] = RC_STR_INIT("3d"),
        [RC_SHADER_DIMENSION_CUBE] = RC_STR_INIT("cube"),
        [RC_SHADER_DIMENSION_CUBE_ARRAY] = RC_STR_INIT("cube_array")
    };

    RC_ASSERT((uint32_t)dim < RC_COUNT(names));
    return names[dim];
}


// The name of a texel format.
static rc_str rc_wgsl_format_name(rc_wgsl_format format)
{
    static const rc_str names[] = {
        [RC_WGSL_FORMAT_NONE] = RC_STR_INIT(""),
        [RC_WGSL_FORMAT_RGBA8UNORM] = RC_STR_INIT("rgba8unorm"),
        [RC_WGSL_FORMAT_RGBA8SNORM] = RC_STR_INIT("rgba8snorm"),
        [RC_WGSL_FORMAT_RGBA8UINT] = RC_STR_INIT("rgba8uint"),
        [RC_WGSL_FORMAT_RGBA8SINT] = RC_STR_INIT("rgba8sint"),
        [RC_WGSL_FORMAT_RGBA16UINT] = RC_STR_INIT("rgba16uint"),
        [RC_WGSL_FORMAT_RGBA16SINT] = RC_STR_INIT("rgba16sint"),
        [RC_WGSL_FORMAT_RGBA16FLOAT] = RC_STR_INIT("rgba16float"),
        [RC_WGSL_FORMAT_R32UINT] = RC_STR_INIT("r32uint"),
        [RC_WGSL_FORMAT_R32SINT] = RC_STR_INIT("r32sint"),
        [RC_WGSL_FORMAT_R32FLOAT] = RC_STR_INIT("r32float"),
        [RC_WGSL_FORMAT_RG32UINT] = RC_STR_INIT("rg32uint"),
        [RC_WGSL_FORMAT_RG32SINT] = RC_STR_INIT("rg32sint"),
        [RC_WGSL_FORMAT_RG32FLOAT] = RC_STR_INIT("rg32float"),
        [RC_WGSL_FORMAT_RGBA32UINT] = RC_STR_INIT("rgba32uint"),
        [RC_WGSL_FORMAT_RGBA32SINT] = RC_STR_INIT("rgba32sint"),
        [RC_WGSL_FORMAT_RGBA32FLOAT] = RC_STR_INIT("rgba32float"),
        [RC_WGSL_FORMAT_BGRA8UNORM] = RC_STR_INIT("bgra8unorm")
    };

    RC_ASSERT((uint32_t)format < RC_COUNT(names));
    return names[format];
}


// Append a texture type's name.
static void rc_wgsl_append_texture_name(rc_mstr *out, rc_view_wgsl_typeinfo types, rc_wgsl_typeinfo type, rc_wgsl_ast ast, rc_arena *arena)
{
    static const rc_str prefixes[] = {
        [RC_WGSL_TEXTURE_CLASS_NONE] = RC_STR_INIT(""),
        [RC_WGSL_TEXTURE_CLASS_SAMPLED] = RC_STR_INIT("texture_"),
        [RC_WGSL_TEXTURE_CLASS_MULTISAMPLED] = RC_STR_INIT("texture_multisampled_"),
        [RC_WGSL_TEXTURE_CLASS_DEPTH] = RC_STR_INIT("texture_depth_"),
        [RC_WGSL_TEXTURE_CLASS_DEPTH_MULTISAMPLED] = RC_STR_INIT("texture_depth_multisampled_")
    };

    RC_ASSERT(type.texture_class < RC_COUNT(prefixes));
    rc_mstr_append(out, prefixes[type.texture_class], arena);
    rc_mstr_append(out, rc_wgsl_dim_name((rc_shader_dimension)type.dim), arena);
    if (type.texture_class == RC_WGSL_TEXTURE_CLASS_SAMPLED || type.texture_class == RC_WGSL_TEXTURE_CLASS_MULTISAMPLED) {
        rc_mstr_append(out, RC_STR("<"), arena);
        rc_wgsl_type_append_name(out, types, type.sampled_type, ast, arena);
        rc_mstr_append(out, RC_STR(">"), arena);
    }
}


// Append a template list's opening, then the element's name: "vec3<f32" before the ">".
static void rc_wgsl_append_generic(rc_mstr *out, rc_str name, rc_view_wgsl_typeinfo types,
    uint32_t element, rc_wgsl_ast ast, rc_arena *arena)
{
    rc_mstr_append(out, name, arena);
    rc_mstr_append(out, RC_STR("<"), arena);
    rc_wgsl_type_append_name(out, types, element, ast, arena);
}


uint32_t rc_wgsl_type_common(rc_view_wgsl_typeinfo types, uint32_t a, uint32_t b)
{
    if (rc_wgsl_type_converts(types, a, b)) {
        return b;
    }

    return rc_wgsl_type_converts(types, b, a) ? a : 0;
}


void rc_wgsl_type_append_name(rc_mstr *out, rc_view_wgsl_typeinfo types, uint32_t id, rc_wgsl_ast ast, rc_arena *arena)
{
    rc_wgsl_typeinfo type = rc_wgsl_typeinfo_of(types, id);

    switch ((rc_wgsl_typeinfo_type)type.type) {
    case RC_WGSL_TYPEINFO_TYPE_NONE:
        rc_mstr_append(out, RC_STR("nothing"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_BOOL:
        rc_mstr_append(out, RC_STR("bool"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT:
        rc_mstr_append(out, RC_STR("an integer"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_ABSTRACT_FLOAT:
        rc_mstr_append(out, RC_STR("a number"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_I32:
        rc_mstr_append(out, RC_STR("i32"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_U32:
        rc_mstr_append(out, RC_STR("u32"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_F32:
        rc_mstr_append(out, RC_STR("f32"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
        rc_mstr_append(out, RC_STR("vec"), arena);
        rc_mstr_append_u32(out, type.width, arena);
        rc_wgsl_append_generic(out, RC_STR(""), types, type.element, ast, arena);
        rc_mstr_append(out, RC_STR(">"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
        rc_mstr_append(out, RC_STR("mat"), arena);
        rc_mstr_append_u32(out, type.columns, arena);
        rc_mstr_append(out, RC_STR("x"), arena);
        rc_mstr_append_u32(out, type.rows, arena);
        rc_wgsl_append_generic(out, RC_STR(""), types, type.element, ast, arena);
        rc_mstr_append(out, RC_STR(">"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_ARRAY:
        rc_wgsl_append_generic(out, RC_STR("array"), types, type.element, ast, arena);
        if (type.count != 0) {
            rc_mstr_append(out, RC_STR(", "), arena);
            rc_mstr_append_u32(out, type.count, arena);
        }

        rc_mstr_append(out, RC_STR(">"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_STRUCT:
        rc_mstr_append(out, rc_wgsl_ast_text(ast, type.decl), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_ATOMIC:
        rc_wgsl_append_generic(out, RC_STR("atomic"), types, type.element, ast, arena);
        rc_mstr_append(out, RC_STR(">"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_POINTER:
        rc_mstr_append(out, RC_STR("ptr<"), arena);
        rc_mstr_append(out, rc_wgsl_space_name((rc_wgsl_space)type.space), arena);
        rc_mstr_append(out, RC_STR(", "), arena);
        rc_wgsl_type_append_name(out, types, type.store_type, ast, arena);
        rc_mstr_append(out, RC_STR(", "), arena);
        rc_mstr_append(out, rc_wgsl_access_name((rc_wgsl_access)type.access), arena);
        rc_mstr_append(out, RC_STR(">"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_SAMPLER:
        rc_mstr_append(out, RC_STR("sampler"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_SAMPLER_COMPARISON:
        rc_mstr_append(out, RC_STR("sampler_comparison"), arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_TEXTURE:
        rc_wgsl_append_texture_name(out, types, type, ast, arena);
        break;
    case RC_WGSL_TYPEINFO_TYPE_STORAGE_TEXTURE:
        rc_mstr_append(out, RC_STR("texture_storage_"), arena);
        rc_mstr_append(out, rc_wgsl_dim_name((rc_shader_dimension)type.dim), arena);
        rc_mstr_append(out, RC_STR("<"), arena);
        rc_mstr_append(out, rc_wgsl_format_name((rc_wgsl_format)type.format), arena);
        rc_mstr_append(out, RC_STR(", "), arena);
        rc_mstr_append(out, rc_wgsl_access_name((rc_wgsl_access)type.access), arena);
        rc_mstr_append(out, RC_STR(">"), arena);
        break;
    }
}
