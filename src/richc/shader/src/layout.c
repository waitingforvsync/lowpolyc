// layout.c
//
// Memory layouts. Each rule follows its specification's tables: WGSL's alignment and size of
// host-shareable types, with @align and @size, and GLSL's std140 and std430 block layouts, which
// ignore those attributes.

#include "layout.h"

// value rounded up to a multiple of align.
static uint32_t rc_wgsl_round_up(uint32_t value, uint32_t align)
{
    return (value + align - 1) / align * align;
}


// The value of @name(n) on the field node, or 0 if it has none.
static uint32_t rc_wgsl_field_attribute(rc_wgsl_module module, uint32_t field, rc_str name)
{
    uint32_t attribute = rc_wgsl_attribute_find(module.ast, rc_wgsl_ast_node(module.ast, field).lhs,
        name);
    rc_view_wgsl_index args = rc_wgsl_ast_list(module.ast,
        rc_wgsl_ast_node(module.ast, attribute).lhs);

    if (attribute == 0 || args.num == 0) {
        return 0;
    }
    return (uint32_t)rc_wgsl_module_sema(module, rc_view_wgsl_index_get(args, 0)).value;
}


// The layout of a vector of width four-byte scalars.
static rc_wgsl_layout rc_wgsl_vector_layout(uint32_t width)
{
    return (rc_wgsl_layout) {
        .align = width == 2 ? 8 : 16,
        .size = 4 * width
    };
}


// The distance between a matrix's columns of rows each.
static uint32_t rc_wgsl_column_stride(uint32_t rows, rc_wgsl_layout_rule rule)
{
    rc_wgsl_layout column = rc_wgsl_vector_layout(rows);
    uint32_t stride = rc_wgsl_round_up(column.size, column.align);

    return rule == RC_WGSL_LAYOUT_RULE_STD140 ? rc_wgsl_round_up(stride, 16) : stride;
}


// The distance between an array's elements of type element.
static uint32_t rc_wgsl_array_stride(rc_wgsl_module module, uint32_t element,
    rc_wgsl_layout_rule rule)
{
    rc_wgsl_layout layout = rc_wgsl_layout_of(module, element, rule);
    uint32_t stride = rc_wgsl_round_up(layout.size, layout.align);

    return rule == RC_WGSL_LAYOUT_RULE_STD140 ? rc_wgsl_round_up(stride, 16) : stride;
}


// The layout of a struct field, with its @align and @size under WGSL's rules.
static rc_wgsl_layout rc_wgsl_field_layout(rc_wgsl_module module, uint32_t field,
    rc_wgsl_layout_rule rule)
{
    rc_wgsl_layout layout = rc_wgsl_layout_of(module, rc_wgsl_module_sema(module, field).type,
        rule);
    uint32_t align = rc_wgsl_field_attribute(module, field, RC_STR("align"));
    uint32_t size = rc_wgsl_field_attribute(module, field, RC_STR("size"));

    if (rule == RC_WGSL_LAYOUT_RULE_WGSL && align != 0) {
        layout.align = align;
    }
    if (rule == RC_WGSL_LAYOUT_RULE_WGSL && size != 0) {
        layout.size = size;
    }
    return layout;
}


// Where a struct's fields end, and the alignment the largest needs, after count fields.
typedef struct rc_wgsl_struct_walk {
    uint32_t offset;  // of the last field walked
    uint32_t end;
    uint32_t align;
} rc_wgsl_struct_walk;

// Walk the first count fields of the struct declared at decl.
static rc_wgsl_struct_walk rc_wgsl_walk_fields(rc_wgsl_module module, uint32_t decl,
    uint32_t count, rc_wgsl_layout_rule rule)
{
    rc_view_wgsl_index fields = rc_wgsl_ast_list(module.ast, rc_wgsl_ast_node(module.ast, decl).lhs);
    rc_wgsl_struct_walk walk = {.align = 1};

    for (uint32_t i = 0; i < count && i < fields.num; i++) {
        rc_wgsl_layout layout = rc_wgsl_field_layout(module, rc_view_wgsl_index_get(fields, i), rule);

        walk.offset = rc_wgsl_round_up(walk.end, layout.align);
        walk.end = walk.offset + layout.size;
        walk.align = layout.align > walk.align ? layout.align : walk.align;
    }
    return walk;
}


// The layout of the struct declared at decl.
static rc_wgsl_layout rc_wgsl_struct_layout(rc_wgsl_module module, uint32_t decl,
    rc_wgsl_layout_rule rule)
{
    rc_wgsl_struct_walk walk = rc_wgsl_walk_fields(module, decl, UINT32_MAX, rule);
    uint32_t align = rule == RC_WGSL_LAYOUT_RULE_STD140 ? rc_wgsl_round_up(walk.align, 16) : walk.align;

    return (rc_wgsl_layout) {
        .align = align,
        .size = rc_wgsl_round_up(walk.end, align)
    };
}


rc_wgsl_layout rc_wgsl_layout_of(rc_wgsl_module module, uint32_t type, rc_wgsl_layout_rule rule)
{
    rc_wgsl_typeinfo t = rc_wgsl_typeinfo_of(module.types, type);

    switch ((rc_wgsl_typeinfo_type)t.type) {
    case RC_WGSL_TYPEINFO_TYPE_VECTOR:
        return rc_wgsl_vector_layout(t.width);
    case RC_WGSL_TYPEINFO_TYPE_MATRIX: {
        uint32_t stride = rc_wgsl_column_stride(t.rows, rule);
        uint32_t align = rule == RC_WGSL_LAYOUT_RULE_STD140 ? 16 : rc_wgsl_vector_layout(t.rows).align;

        return (rc_wgsl_layout) {.align = align, .size = t.columns * stride};
    }
    case RC_WGSL_TYPEINFO_TYPE_ARRAY: {
        rc_wgsl_layout element = rc_wgsl_layout_of(module, t.element, rule);
        uint32_t align = rule == RC_WGSL_LAYOUT_RULE_STD140 ? rc_wgsl_round_up(element.align, 16)
                                                       : element.align;

        return (rc_wgsl_layout) {
            .align = align,
            .size = (t.count != 0 ? t.count : 1) * rc_wgsl_array_stride(module, t.element, rule)
        };
    }
    case RC_WGSL_TYPEINFO_TYPE_STRUCT:
        return rc_wgsl_struct_layout(module, t.decl, rule);
    default:
        return (rc_wgsl_layout) {.align = 4, .size = 4};
    }
}


// The mismatch in a struct: the first field placed differently, or within which something is,
// or the struct if only its size differs. A field's own size matters only through where the next
// one goes and the struct's size, so @size that just pads to where GLSL would go anyway is fine.
static uint32_t rc_wgsl_struct_mismatch(rc_wgsl_module module, uint32_t decl,
    rc_wgsl_layout_rule rule)
{
    rc_view_wgsl_index fields = rc_wgsl_ast_list(module.ast, rc_wgsl_ast_node(module.ast, decl).lhs);

    for (uint32_t i = 0; i < fields.num; i++) {
        uint32_t field = rc_view_wgsl_index_get(fields, i);
        uint32_t inner;

        if (rc_wgsl_walk_fields(module, decl, i + 1, RC_WGSL_LAYOUT_RULE_WGSL).offset
            != rc_wgsl_walk_fields(module, decl, i + 1, rule).offset) {
            return field;
        }
        inner = rc_wgsl_layout_mismatch(module, rc_wgsl_module_sema(module, field).type, rule);
        if (inner != 0) {
            return inner == RC_INDEX_NONE ? field : inner;
        }
    }
    if (rc_wgsl_layout_of(module, rc_wgsl_module_sema(module, decl).type, RC_WGSL_LAYOUT_RULE_WGSL).size
        != rc_wgsl_layout_of(module, rc_wgsl_module_sema(module, decl).type, rule).size) {
        return decl;
    }
    return 0;
}


uint32_t rc_wgsl_layout_mismatch(rc_wgsl_module module, uint32_t type, rc_wgsl_layout_rule rule)
{
    rc_wgsl_typeinfo t = rc_wgsl_typeinfo_of(module.types, type);

    switch ((rc_wgsl_typeinfo_type)t.type) {
    case RC_WGSL_TYPEINFO_TYPE_STRUCT:
        return rc_wgsl_struct_mismatch(module, t.decl, rule);
    case RC_WGSL_TYPEINFO_TYPE_ARRAY:
        if (rc_wgsl_array_stride(module, t.element, RC_WGSL_LAYOUT_RULE_WGSL)
            != rc_wgsl_array_stride(module, t.element, rule)) {
            return RC_INDEX_NONE;
        }
        return rc_wgsl_layout_mismatch(module, t.element, rule);
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
        return rc_wgsl_column_stride(t.rows, RC_WGSL_LAYOUT_RULE_WGSL)
            != rc_wgsl_column_stride(t.rows, rule) ? RC_INDEX_NONE : 0;
    default:
        return 0;
    }
}
