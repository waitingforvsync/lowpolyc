// parser.h
//
// Private to richc_shader: WGSL tokens to a syntax tree (ast.h).
//
// The parser follows WGSL's grammar, including the operators it will not mix without parentheses
// (a & b | c, a + b << c, a < b < c), and refuses the parts of WGSL we do not translate yet:
// enable and requires directives, and override declarations. A diagnostic directive or attribute
// is checked and dropped, since nothing we do depends on it.
//
// Nesting is limited, so deeply nested input is an error rather than a stack overflow.

#ifndef RC_SHADER_PARSER_H_
#define RC_SHADER_PARSER_H_

#include "ast.h"

typedef struct rc_wgsl_parse_result {
    rc_wgsl_ast ast;
    rc_wgsl_error error;
} rc_wgsl_parse_result;

// The tree for tokens, lexed from source, allocated from arena; or the first error.
rc_wgsl_parse_result rc_wgsl_parse(rc_str source, rc_view_wgsl_token tokens, rc_arena *arena);

#endif // RC_SHADER_PARSER_H_
