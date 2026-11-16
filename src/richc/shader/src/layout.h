// layout.h
//
// Private to richc_shader: where values sit in memory shared with the host, by WGSL's rules and by
// GLSL's std140 and std430.
//
// A WGSL buffer and the GLSL block written for it must agree byte for byte. WGSL's storage layout
// is std430, give or take @align and @size. Its uniform layout matches std140 for every struct
// WGSL accepts, except that matrices with two rows pack their columns 8 bytes apart, where std140
// puts them 16 apart. So we lay each buffer's type out both ways and compare.

#ifndef RC_SHADER_LAYOUT_H_
#define RC_SHADER_LAYOUT_H_

#include "check.h"

typedef enum rc_wgsl_layout_rule {
    RC_WGSL_LAYOUT_RULE_NONE = 0,
    RC_WGSL_LAYOUT_RULE_WGSL,
    RC_WGSL_LAYOUT_RULE_STD140,
    RC_WGSL_LAYOUT_RULE_STD430
} rc_wgsl_layout_rule;

// A type's alignment and size in bytes. A runtime-sized array's size is one element's.
typedef struct rc_wgsl_layout {
    uint32_t align;
    uint32_t size;
} rc_wgsl_layout;

// The layout of type by rule.
rc_wgsl_layout rc_wgsl_layout_of(rc_wgsl_module module, uint32_t type, rc_wgsl_layout_rule rule);

// Where type laid out by WGSL's rules and by rule first differ: a struct field, or the struct
// itself if only its size differs. RC_INDEX_NONE if type itself differs (an array's stride or a
// matrix's columns) with no field inside it to blame, and 0 if they agree.
uint32_t rc_wgsl_layout_mismatch(rc_wgsl_module module, uint32_t type, rc_wgsl_layout_rule rule);

#endif // RC_SHADER_LAYOUT_H_
