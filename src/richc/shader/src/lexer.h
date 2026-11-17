// lexer.h
//
// Private to richc_shader: turning WGSL source into tokens.
//
// A token is a type and the run of source it covers, so its text is always at hand for errors
// and literals. Blankspace and comments leave no tokens, and the last token is always END.
//
// WGSL's '<' and '>' both compare and bracket template lists (vec3<f32>, array<i32, 4>). The
// lexer works out which is which with the spec's template list discovery, so the parser sees
// TEMPLATE_START and TEMPLATE_END where a list opens and closes, and never has to guess. That is
// why '>>', '>=' and '>>=' are only joined up after discovery: in array<vec2<f32>> each '>' closes
// a list of its own.

#ifndef RC_SHADER_LEXER_H_
#define RC_SHADER_LEXER_H_

#include "richc/shader/wgsl.h"

typedef enum rc_wgsl_token_type {
    RC_WGSL_TOKEN_TYPE_NONE = 0,
    RC_WGSL_TOKEN_TYPE_END,
    RC_WGSL_TOKEN_TYPE_IDENT,

    // Literals: an integer, maybe with an i or u suffix, and a float, maybe with an f or h suffix
    RC_WGSL_TOKEN_TYPE_INT,
    RC_WGSL_TOKEN_TYPE_FLOAT,

    // The keywords, in alphabetical order
    RC_WGSL_TOKEN_TYPE_ALIAS,
    RC_WGSL_TOKEN_TYPE_BREAK,
    RC_WGSL_TOKEN_TYPE_CASE,
    RC_WGSL_TOKEN_TYPE_CONST,
    RC_WGSL_TOKEN_TYPE_CONST_ASSERT,
    RC_WGSL_TOKEN_TYPE_CONTINUE,
    RC_WGSL_TOKEN_TYPE_CONTINUING,
    RC_WGSL_TOKEN_TYPE_DEFAULT,
    RC_WGSL_TOKEN_TYPE_DIAGNOSTIC,
    RC_WGSL_TOKEN_TYPE_DISCARD,
    RC_WGSL_TOKEN_TYPE_ELSE,
    RC_WGSL_TOKEN_TYPE_ENABLE,
    RC_WGSL_TOKEN_TYPE_FALSE,
    RC_WGSL_TOKEN_TYPE_FN,
    RC_WGSL_TOKEN_TYPE_FOR,
    RC_WGSL_TOKEN_TYPE_IF,
    RC_WGSL_TOKEN_TYPE_LET,
    RC_WGSL_TOKEN_TYPE_LOOP,
    RC_WGSL_TOKEN_TYPE_OVERRIDE,
    RC_WGSL_TOKEN_TYPE_REQUIRES,
    RC_WGSL_TOKEN_TYPE_RETURN,
    RC_WGSL_TOKEN_TYPE_STRUCT,
    RC_WGSL_TOKEN_TYPE_SWITCH,
    RC_WGSL_TOKEN_TYPE_TRUE,
    RC_WGSL_TOKEN_TYPE_VAR,
    RC_WGSL_TOKEN_TYPE_WHILE,

    // Punctuation, named for its characters, in alphabetical order
    RC_WGSL_TOKEN_TYPE_AND,
    RC_WGSL_TOKEN_TYPE_AND_AND,
    RC_WGSL_TOKEN_TYPE_AND_EQUAL,
    RC_WGSL_TOKEN_TYPE_ARROW,
    RC_WGSL_TOKEN_TYPE_AT,
    RC_WGSL_TOKEN_TYPE_BANG,
    RC_WGSL_TOKEN_TYPE_BANG_EQUAL,
    RC_WGSL_TOKEN_TYPE_COLON,
    RC_WGSL_TOKEN_TYPE_COMMA,
    RC_WGSL_TOKEN_TYPE_EQUAL,
    RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL,
    RC_WGSL_TOKEN_TYPE_GREATER,
    RC_WGSL_TOKEN_TYPE_GREATER_EQUAL,
    RC_WGSL_TOKEN_TYPE_LEFT_BRACE,
    RC_WGSL_TOKEN_TYPE_LEFT_BRACKET,
    RC_WGSL_TOKEN_TYPE_LEFT_PAREN,
    RC_WGSL_TOKEN_TYPE_LESS,
    RC_WGSL_TOKEN_TYPE_LESS_EQUAL,
    RC_WGSL_TOKEN_TYPE_MINUS,
    RC_WGSL_TOKEN_TYPE_MINUS_EQUAL,
    RC_WGSL_TOKEN_TYPE_MINUS_MINUS,
    RC_WGSL_TOKEN_TYPE_OR,
    RC_WGSL_TOKEN_TYPE_OR_EQUAL,
    RC_WGSL_TOKEN_TYPE_OR_OR,
    RC_WGSL_TOKEN_TYPE_PERCENT,
    RC_WGSL_TOKEN_TYPE_PERCENT_EQUAL,
    RC_WGSL_TOKEN_TYPE_PERIOD,
    RC_WGSL_TOKEN_TYPE_PLUS,
    RC_WGSL_TOKEN_TYPE_PLUS_EQUAL,
    RC_WGSL_TOKEN_TYPE_PLUS_PLUS,
    RC_WGSL_TOKEN_TYPE_RIGHT_BRACE,
    RC_WGSL_TOKEN_TYPE_RIGHT_BRACKET,
    RC_WGSL_TOKEN_TYPE_RIGHT_PAREN,
    RC_WGSL_TOKEN_TYPE_SEMICOLON,
    RC_WGSL_TOKEN_TYPE_SHIFT_LEFT,
    RC_WGSL_TOKEN_TYPE_SHIFT_LEFT_EQUAL,
    RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT,
    RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT_EQUAL,
    RC_WGSL_TOKEN_TYPE_SLASH,
    RC_WGSL_TOKEN_TYPE_SLASH_EQUAL,
    RC_WGSL_TOKEN_TYPE_STAR,
    RC_WGSL_TOKEN_TYPE_STAR_EQUAL,
    RC_WGSL_TOKEN_TYPE_TILDE,
    RC_WGSL_TOKEN_TYPE_UNDERSCORE,
    RC_WGSL_TOKEN_TYPE_XOR,
    RC_WGSL_TOKEN_TYPE_XOR_EQUAL,

    // A '<' that opens a template list, and the '>' that closes it
    RC_WGSL_TOKEN_TYPE_TEMPLATE_START,
    RC_WGSL_TOKEN_TYPE_TEMPLATE_END
} rc_wgsl_token_type;

typedef struct rc_wgsl_token {
    uint32_t offset;  // bytes into the source
    uint32_t len;
    uint8_t type;     // rc_wgsl_token_type
} rc_wgsl_token;

#define RC_ARRAY_TYPE rc_wgsl_token
#define RC_ARRAY_NAME wgsl_token
#include "richc/array.template.h"

typedef struct rc_wgsl_lex_result {
    rc_view_wgsl_token tokens;
    rc_wgsl_error error;
} rc_wgsl_lex_result;

// The tokens of source, ending with END, allocated from arena. On an error, the tokens are {0}.
rc_wgsl_lex_result rc_wgsl_lex(rc_str source, rc_arena *arena);

// The text a token covers.
static inline rc_str rc_wgsl_token_text(rc_str source, rc_wgsl_token token)
{
    return rc_str_substr(source, token.offset, token.offset + token.len);
}

// The value of a hex digit.
static inline uint32_t rc_wgsl_hex_digit(char c)
{
    return c <= '9' ? (uint32_t)(c - '0') : (uint32_t)((c | 0x20) - 'a' + 10);
}

// True if a number literal's text is in hex: 0x or 0X, then the digits.
static inline bool rc_wgsl_is_hex_literal(rc_str text)
{
    return text.len > 1 && text.data[0] == '0' && (text.data[1] | 0x20) == 'x';
}

// The plain operator a compound assignment's token applies: + for +=.
rc_wgsl_token_type rc_wgsl_compound_operator(rc_wgsl_token_type op);

// True for a keyword's token type.
static inline bool rc_wgsl_token_is_keyword(rc_wgsl_token_type type)
{
    return type >= RC_WGSL_TOKEN_TYPE_ALIAS && type <= RC_WGSL_TOKEN_TYPE_WHILE;
}

#endif // RC_SHADER_LEXER_H_
