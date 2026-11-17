// layout.c
//
// Memory layouts. Each rule follows its specification's tables: WGSL's alignment and size of
// host-shareable types, with @align and @size, and GLSL's std140 and std430 block layouts, which
// ignore those attributes.

#include "layout.h"

// value, or UINT32_MAX if it is more: the size of anything too large to lay out.
static uint32_t rc_wgsl_saturate(uint64_t value)
{
    return value > UINT32_MAX ? UINT32_MAX : (uint32_t)value;
}


// value rounded up to a multiple of align.
static uint32_t rc_wgsl_round_up(uint32_t value, uint32_t align)
{
    return rc_wgsl_saturate(((uint64_t)value + align - 1) / align * align);
}


// The value of the field node's attribute, or 0 if it has none.
static uint32_t rc_wgsl_field_attribute(rc_wgsl_module module, uint32_t field, rc_wgsl_attribute attribute)
{
    return (uint32_t)rc_wgsl_attribute_value(module.ast, module.sema, rc_wgsl_ast_node(module.ast, field).lhs, attribute, 0);
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
static uint32_t rc_wgsl_array_stride(rc_wgsl_module module, uint32_t element, rc_wgsl_layout_rule rule)
{
    rc_wgsl_layout layout = rc_wgsl_layout_of(module, element, rule);
    uint32_t stride = rc_wgsl_round_up(layout.size, layout.align);

    return rule == RC_WGSL_LAYOUT_RULE_STD140 ? rc_wgsl_round_up(stride, 16) : stride;
}


// The layout of a struct field, with its @align and @size under WGSL's rules.
static rc_wgsl_layout rc_wgsl_field_layout(rc_wgsl_module module, uint32_t field, rc_wgsl_layout_rule rule)
{
    rc_wgsl_layout layout = rc_wgsl_layout_of(module, rc_wgsl_module_sema(module, field).type, rule);
    uint32_t align = rc_wgsl_field_attribute(module, field, RC_WGSL_ATTRIBUTE_ALIGN);
    uint32_t size = rc_wgsl_field_attribute(module, field, RC_WGSL_ATTRIBUTE_SIZE);

    if (rule == RC_WGSL_LAYOUT_RULE_WGSL && align != 0) {
        layout.align = align;
    }

    if (rule == RC_WGSL_LAYOUT_RULE_WGSL && size != 0) {
        layout.size = size;
    }

    return layout;
}


// Where a struct's fields have reached: the offset of the last one placed, where it ends, and the
// alignment the largest needs.
typedef struct rc_wgsl_struct_walk {
    uint32_t offset;
    uint32_t end;
    uint32_t align;
} rc_wgsl_struct_walk;

// walk, with the next field placed by rule after the rest.
static rc_wgsl_struct_walk rc_wgsl_place_field(rc_wgsl_module module, rc_wgsl_struct_walk walk, uint32_t field, rc_wgsl_layout_rule rule)
{
    rc_wgsl_layout layout = rc_wgsl_field_layout(module, field, rule);
    uint32_t offset = rc_wgsl_round_up(walk.end, layout.align);

    return (rc_wgsl_struct_walk) {
        .offset = offset,
        .end = rc_wgsl_saturate((uint64_t)offset + layout.size),
        .align = layout.align > walk.align ? layout.align : walk.align
    };
}


// The layout of a struct whose fields walk has placed, by rule.
static rc_wgsl_layout rc_wgsl_struct_end(rc_wgsl_struct_walk walk, rc_wgsl_layout_rule rule)
{
    uint32_t align = rule == RC_WGSL_LAYOUT_RULE_STD140 ? rc_wgsl_round_up(walk.align, 16) : walk.align;

    return (rc_wgsl_layout) {
        .align = align,
        .size = rc_wgsl_round_up(walk.end, align)
    };
}


// The layout of the struct declared at decl.
static rc_wgsl_layout rc_wgsl_struct_layout(rc_wgsl_module module, uint32_t decl, rc_wgsl_layout_rule rule)
{
    rc_view_wgsl_index fields = rc_wgsl_ast_struct_fields(module.ast, decl);
    rc_wgsl_struct_walk walk = {.align = 1};

    for (uint32_t i = 0; i < fields.num; i++) {
        walk = rc_wgsl_place_field(module, walk, rc_view_wgsl_index_get(fields, i), rule);
    }
    return rc_wgsl_struct_end(walk, rule);
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
        uint32_t align = rule == RC_WGSL_LAYOUT_RULE_STD140 ? rc_wgsl_round_up(element.align, 16) : element.align;

        return (rc_wgsl_layout) {
            .align = align,
            .size = rc_wgsl_saturate((uint64_t)(t.count != 0 ? t.count : 1) * rc_wgsl_array_stride(module, t.element, rule))
        };
    }
    case RC_WGSL_TYPEINFO_TYPE_STRUCT:
        return rc_wgsl_struct_layout(module, t.decl, rule);
    default:
        return (rc_wgsl_layout) {.align = 4, .size = 4};
    }
}


// The mismatch at field, which has moved if WGSL's rules and rule place it apart: the field itself
// if it moved or its type differs as a whole, whatever differs within its type, or else 0.
static uint32_t rc_wgsl_field_mismatch(rc_wgsl_module module, uint32_t field, bool moved, rc_wgsl_layout_rule rule)
{
    uint32_t type = rc_wgsl_module_sema(module, field).type;
    uint32_t inner = moved ? RC_INDEX_NONE : rc_wgsl_layout_mismatch(module, type, rule);

    return inner == RC_INDEX_NONE ? field : inner;
}


// The mismatch in a struct: the first field placed differently, or within which something is,
// or the struct if only its size differs. A field's own size matters only through where the next
// one goes and the struct's size, so @size that just pads to where GLSL would go anyway is fine.
static uint32_t rc_wgsl_struct_mismatch(rc_wgsl_module module, uint32_t decl, rc_wgsl_layout_rule rule)
{
    rc_view_wgsl_index fields = rc_wgsl_ast_struct_fields(module.ast, decl);
    rc_wgsl_struct_walk wgsl = {.align = 1};
    rc_wgsl_struct_walk other = {.align = 1};

    for (uint32_t i = 0; i < fields.num; i++) {
        uint32_t field = rc_view_wgsl_index_get(fields, i);
        rc_wgsl_struct_walk wgsl_next = rc_wgsl_place_field(module, wgsl, field, RC_WGSL_LAYOUT_RULE_WGSL);
        rc_wgsl_struct_walk other_next = rc_wgsl_place_field(module, other, field, rule);
        uint32_t mismatch = rc_wgsl_field_mismatch(module, field, wgsl_next.offset != other_next.offset, rule);

        if (mismatch != 0) {
            return mismatch;
        }

        wgsl = wgsl_next;
        other = other_next;
    }
    if (rc_wgsl_struct_end(wgsl, RC_WGSL_LAYOUT_RULE_WGSL).size != rc_wgsl_struct_end(other, rule).size) {
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
        if (rc_wgsl_array_stride(module, t.element, RC_WGSL_LAYOUT_RULE_WGSL) != rc_wgsl_array_stride(module, t.element, rule)) {
            return RC_INDEX_NONE;
        }

        return rc_wgsl_layout_mismatch(module, t.element, rule);
    case RC_WGSL_TYPEINFO_TYPE_MATRIX:
        return rc_wgsl_column_stride(t.rows, RC_WGSL_LAYOUT_RULE_WGSL) != rc_wgsl_column_stride(t.rows, rule) ? RC_INDEX_NONE : 0;
    default:
        return 0;
    }
}
