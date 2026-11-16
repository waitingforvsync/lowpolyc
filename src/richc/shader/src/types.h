// types.h
//
// Private to richc_shader: WGSL's types, interned in a table and named by index.
//
// Each distinct type is stored once, so two types are equal exactly when their indices are. The
// scalar types and the samplers sit at fixed indices (rc_wgsl_type_id); the rest are added as the
// checker meets them. Index 0 is no type at all: what a function returning nothing gives, and what
// a failed check leaves behind.
//
// The abstract numeric types are WGSL's types for literals with no suffix. They convert to any
// concrete type that can hold their value, and the checker settles each one on a concrete type
// before the emitters see it.

#ifndef RC_SHADER_TYPES_H_
#define RC_SHADER_TYPES_H_

#include "ast.h"

typedef struct rc_mstr rc_mstr;

typedef enum rc_wgsl_typeinfo_type {
    RC_WGSL_TYPEINFO_TYPE_NONE = 0,
    RC_WGSL_TYPEINFO_TYPE_BOOL,
    RC_WGSL_TYPEINFO_TYPE_ABSTRACT_INT,
    RC_WGSL_TYPEINFO_TYPE_ABSTRACT_FLOAT,
    RC_WGSL_TYPEINFO_TYPE_I32,
    RC_WGSL_TYPEINFO_TYPE_U32,
    RC_WGSL_TYPEINFO_TYPE_F32,
    RC_WGSL_TYPEINFO_TYPE_VECTOR,
    RC_WGSL_TYPEINFO_TYPE_MATRIX,
    RC_WGSL_TYPEINFO_TYPE_ARRAY,
    RC_WGSL_TYPEINFO_TYPE_STRUCT,
    RC_WGSL_TYPEINFO_TYPE_ATOMIC,
    RC_WGSL_TYPEINFO_TYPE_POINTER,
    RC_WGSL_TYPEINFO_TYPE_SAMPLER,
    RC_WGSL_TYPEINFO_TYPE_SAMPLER_COMPARISON,
    RC_WGSL_TYPEINFO_TYPE_TEXTURE,
    RC_WGSL_TYPEINFO_TYPE_STORAGE_TEXTURE
} rc_wgsl_typeinfo_type;

// The types at fixed indices.
typedef enum rc_wgsl_type_id {
    RC_WGSL_TYPE_ID_NONE = 0,
    RC_WGSL_TYPE_ID_BOOL,
    RC_WGSL_TYPE_ID_ABSTRACT_INT,
    RC_WGSL_TYPE_ID_ABSTRACT_FLOAT,
    RC_WGSL_TYPE_ID_I32,
    RC_WGSL_TYPE_ID_U32,
    RC_WGSL_TYPE_ID_F32,
    RC_WGSL_TYPE_ID_SAMPLER,
    RC_WGSL_TYPE_ID_SAMPLER_COMPARISON,
    RC_WGSL_TYPE_ID_FIRST_FREE
} rc_wgsl_type_id;

typedef enum rc_wgsl_space {
    RC_WGSL_SPACE_NONE = 0,
    RC_WGSL_SPACE_FUNCTION,
    RC_WGSL_SPACE_PRIVATE,
    RC_WGSL_SPACE_WORKGROUP,
    RC_WGSL_SPACE_UNIFORM,
    RC_WGSL_SPACE_STORAGE,
    RC_WGSL_SPACE_HANDLE
} rc_wgsl_space;

typedef enum rc_wgsl_access {
    RC_WGSL_ACCESS_NONE = 0,
    RC_WGSL_ACCESS_READ,
    RC_WGSL_ACCESS_WRITE,
    RC_WGSL_ACCESS_READ_WRITE
} rc_wgsl_access;

typedef enum rc_wgsl_dim {
    RC_WGSL_DIM_NONE = 0,
    RC_WGSL_DIM_1D,
    RC_WGSL_DIM_2D,
    RC_WGSL_DIM_2D_ARRAY,
    RC_WGSL_DIM_3D,
    RC_WGSL_DIM_CUBE,
    RC_WGSL_DIM_CUBE_ARRAY
} rc_wgsl_dim;

// What a texture holds. A sampled or multisampled texture's element is its sampled type.
typedef enum rc_wgsl_texture_class {
    RC_WGSL_TEXTURE_CLASS_NONE = 0,
    RC_WGSL_TEXTURE_CLASS_SAMPLED,
    RC_WGSL_TEXTURE_CLASS_MULTISAMPLED,
    RC_WGSL_TEXTURE_CLASS_DEPTH,
    RC_WGSL_TEXTURE_CLASS_DEPTH_MULTISAMPLED
} rc_wgsl_texture_class;

typedef enum rc_wgsl_format {
    RC_WGSL_FORMAT_NONE = 0,
    RC_WGSL_FORMAT_RGBA8UNORM,
    RC_WGSL_FORMAT_RGBA8SNORM,
    RC_WGSL_FORMAT_RGBA8UINT,
    RC_WGSL_FORMAT_RGBA8SINT,
    RC_WGSL_FORMAT_RGBA16UINT,
    RC_WGSL_FORMAT_RGBA16SINT,
    RC_WGSL_FORMAT_RGBA16FLOAT,
    RC_WGSL_FORMAT_R32UINT,
    RC_WGSL_FORMAT_R32SINT,
    RC_WGSL_FORMAT_R32FLOAT,
    RC_WGSL_FORMAT_RG32UINT,
    RC_WGSL_FORMAT_RG32SINT,
    RC_WGSL_FORMAT_RG32FLOAT,
    RC_WGSL_FORMAT_RGBA32UINT,
    RC_WGSL_FORMAT_RGBA32SINT,
    RC_WGSL_FORMAT_RGBA32FLOAT,
    RC_WGSL_FORMAT_BGRA8UNORM
} rc_wgsl_format;

// One type. Which fields mean anything depends on its type:
//
//   VECTOR           element (a scalar), width (2 to 4)
//   MATRIX           element (a float scalar), columns, rows
//   ARRAY            element, count (0 when sized at run time)
//   STRUCT           decl (the STRUCT node), uncopyable (holding an atomic or a runtime-sized
//                    array, so it can't be constructed or copied)
//   ATOMIC           element (i32 or u32)
//   POINTER          element (the store type), space, access
//   TEXTURE          element (the sampled type, for sampled and multisampled), dim,
//                    texture_class
//   STORAGE_TEXTURE  dim, format, access
typedef struct rc_wgsl_typeinfo {
    uint32_t element;
    uint32_t count;
    uint32_t decl;
    uint8_t type;           // rc_wgsl_typeinfo_type
    uint8_t width;
    uint8_t columns;
    uint8_t rows;
    uint8_t space;          // rc_wgsl_space
    uint8_t access;         // rc_wgsl_access
    uint8_t dim;            // rc_wgsl_dim
    uint8_t texture_class;  // rc_wgsl_texture_class
    uint8_t format;         // rc_wgsl_format
    uint8_t uncopyable;
} rc_wgsl_typeinfo;

#define RC_ARRAY_TYPE rc_wgsl_typeinfo
#define RC_ARRAY_NAME wgsl_typeinfo
#include "richc/array.template.h"

// ---- the table ----

// A table holding the types at fixed indices.
rc_array_wgsl_typeinfo rc_wgsl_types_make(rc_arena *arena);

// The index of type, adding it if it is new.
uint32_t rc_wgsl_type_intern(rc_array_wgsl_typeinfo *types, rc_wgsl_typeinfo type, rc_arena *arena);

// The type at index id.
static inline rc_wgsl_typeinfo rc_wgsl_typeinfo_of(rc_view_wgsl_typeinfo types, uint32_t id)
{
    return rc_view_wgsl_typeinfo_get(types, id);
}

// Whether the type at index id is a vector, a struct and so on.
static inline rc_wgsl_typeinfo_type rc_wgsl_typeinfo_type_of(rc_view_wgsl_typeinfo types, uint32_t id)
{
    return (rc_wgsl_typeinfo_type)rc_wgsl_typeinfo_of(types, id).type;
}

// The vector of width elements.
uint32_t rc_wgsl_type_vector(rc_array_wgsl_typeinfo *types, uint32_t element, uint32_t width,
    rc_arena *arena);

// The matrix of columns by rows elements.
uint32_t rc_wgsl_type_matrix(rc_array_wgsl_typeinfo *types, uint32_t element, uint32_t columns,
    uint32_t rows, rc_arena *arena);

// The array of count elements, or a runtime-sized one if count is 0.
uint32_t rc_wgsl_type_array(rc_array_wgsl_typeinfo *types, uint32_t element, uint32_t count,
    rc_arena *arena);

// The pointer to element in space, with access.
uint32_t rc_wgsl_type_pointer(rc_array_wgsl_typeinfo *types, uint32_t element,
    rc_wgsl_space space, rc_wgsl_access access, rc_arena *arena);

// The type with the same shape as id (a scalar, vector, matrix, or array of them) but with scalar
// elements.
uint32_t rc_wgsl_type_with_scalar(rc_array_wgsl_typeinfo *types, uint32_t id, uint32_t scalar,
    rc_arena *arena);

// A matrix's column vector.
uint32_t rc_wgsl_type_column(rc_array_wgsl_typeinfo *types, uint32_t matrix, rc_arena *arena);

// ---- questions ----

// True for bool, the abstract types, i32, u32 and f32.
bool rc_wgsl_type_is_scalar(rc_view_wgsl_typeinfo types, uint32_t id);

// True for the abstract types, i32, u32 and f32.
bool rc_wgsl_type_is_numeric_scalar(rc_view_wgsl_typeinfo types, uint32_t id);

// True for AbstractInt, i32 and u32.
bool rc_wgsl_type_is_integer_scalar(rc_view_wgsl_typeinfo types, uint32_t id);

// True for AbstractFloat and f32.
bool rc_wgsl_type_is_float_scalar(rc_view_wgsl_typeinfo types, uint32_t id);

// The scalar a scalar, vector or matrix is made of, or 0 for anything else.
uint32_t rc_wgsl_type_scalar_of(rc_view_wgsl_typeinfo types, uint32_t id);

// The scalar at the bottom of a scalar, vector, matrix, or array of them; or 0 for anything else.
uint32_t rc_wgsl_type_leaf_scalar(rc_view_wgsl_typeinfo types, uint32_t id);

// True for a scalar, or a vector of them.
bool rc_wgsl_type_is_scalar_or_vector(rc_view_wgsl_typeinfo types, uint32_t id);

// A vector's width, or 1 for anything else.
uint32_t rc_wgsl_type_width_of(rc_view_wgsl_typeinfo types, uint32_t id);

// True if the type is or contains an abstract type.
bool rc_wgsl_type_is_abstract(rc_view_wgsl_typeinfo types, uint32_t id);

// True for the types that can be made by a constructor and copied: scalars, vectors, matrices,
// fixed-size arrays of them and structs of them.
bool rc_wgsl_type_is_constructible(rc_view_wgsl_typeinfo types, uint32_t id);

// The concrete type an abstract one becomes when nothing says otherwise: i32 for AbstractInt and
// f32 for AbstractFloat, through vectors, matrices and arrays. Concrete types are unchanged.
uint32_t rc_wgsl_type_concretise(rc_array_wgsl_typeinfo *types, uint32_t id, rc_arena *arena);

// True if a value of type from can become type to: the same type, or abstract numbers that
// convert, through vectors, matrices and arrays alike.
bool rc_wgsl_type_converts(rc_view_wgsl_typeinfo types, uint32_t from, uint32_t to);

// Append the type's name as WGSL writes it, for errors. Structs are named from the tree.
void rc_wgsl_type_append_name(rc_mstr *out, rc_view_wgsl_typeinfo types, uint32_t id, rc_wgsl_ast ast,
    rc_arena *arena);

#endif // RC_SHADER_TYPES_H_
