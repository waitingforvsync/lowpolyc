// test_lexer.c
//
// Tests for the WGSL lexer, template list discovery included.

#include "lexer.h"

#include "richc/mstr.h"
#include "richc/test.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(lexer) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(lexer, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(lexer, fix)
{
    rc_arena_release(&fix->a);
}


// The tokens of source as text, separated by spaces, leaving out END. A template list's brackets
// show as <T and T>, so they stand out from comparisons.
static rc_str texts(rc_str source, rc_arena *arena)
{
    rc_wgsl_lex_result lexed = rc_wgsl_lex(source, arena);
    rc_mstr out = rc_mstr_make(64, arena);

    if (rc_wgsl_error_is_set(lexed.error)) {
        return lexed.error.message;
    }

    for (uint32_t i = 0; i + 1 < lexed.tokens.num; i++) {
        rc_wgsl_token token = rc_view_wgsl_token_get(lexed.tokens, i);

        if (i > 0) {
            rc_mstr_append_char(&out, ' ', arena);
        }

        if (token.type == RC_WGSL_TOKEN_TYPE_TEMPLATE_START) {
            rc_mstr_append(&out, RC_STR("<T"), arena);
        }
        else if (token.type == RC_WGSL_TOKEN_TYPE_TEMPLATE_END) {
            rc_mstr_append(&out, RC_STR("T>"), arena);
        }
        else {
            rc_mstr_append(&out, rc_wgsl_token_text(source, token), arena);
        }
    }
    return out.view;
}


// The type of the only token in source, or none if it lexes to anything else.
static rc_wgsl_token_type only_type(rc_str source, rc_arena *arena)
{
    rc_wgsl_lex_result lexed = rc_wgsl_lex(source, arena);

    if (lexed.tokens.num != 2 || rc_view_wgsl_token_get(lexed.tokens, 0).len != source.len) {
        return RC_WGSL_TOKEN_TYPE_NONE;
    }

    return (rc_wgsl_token_type)rc_view_wgsl_token_get(lexed.tokens, 0).type;
}


// The error lexing source gives, which must be one.
static rc_wgsl_error error_of(rc_str source, rc_arena *arena)
{
    rc_wgsl_lex_result lexed = rc_wgsl_lex(source, arena);

    RC_CHECK_TRUE(rc_wgsl_error_is_set(lexed.error));
    RC_CHECK(lexed.tokens.num, ==, 0u);
    return lexed.error;
}

// ---- blankspace and comments ----

RC_TEST_STEP(lexer, nothing_is_just_end, fix)
{
    rc_wgsl_lex_result lexed = rc_wgsl_lex(RC_STR(""), &fix->a);

    RC_CHECK_FALSE(rc_wgsl_error_is_set(lexed.error));
    RC_CHECK(lexed.tokens.num, ==, 1u);
    RC_CHECK(rc_view_wgsl_token_get(lexed.tokens, 0).type, ==, RC_WGSL_TOKEN_TYPE_END);
    RC_CHECK(rc_wgsl_lex(RC_STR(" \t\r\n\v\f"), &fix->a).tokens.num, ==, 1u);
}


RC_TEST_STEP(lexer, comments_leave_nothing, fix)
{
    RC_CHECK(texts(RC_STR("a // b\nc"), &fix->a), ==, RC_STR("a c"));
    RC_CHECK(texts(RC_STR("a // b"), &fix->a), ==, RC_STR("a"));
    RC_CHECK(texts(RC_STR("a /* b */ c"), &fix->a), ==, RC_STR("a c"));
    RC_CHECK(texts(RC_STR("a/**/c"), &fix->a), ==, RC_STR("a c"));
}


RC_TEST_STEP(lexer, block_comments_nest, fix)
{
    RC_CHECK(texts(RC_STR("a /* b /* c */ d */ e"), &fix->a), ==, RC_STR("a e"));
    RC_CHECK(texts(RC_STR("a /* // */ e"), &fix->a), ==, RC_STR("a e"));
}


RC_TEST_STEP(lexer, a_comment_that_never_ends, fix)
{
    rc_wgsl_error error = error_of(RC_STR("a\n  /* b /* c */"), &fix->a);

    RC_CHECK(error.message, ==, RC_STR("this comment never ends"));
    RC_CHECK(error.offset, ==, 4u);
    RC_CHECK(error.line, ==, 2u);
    RC_CHECK(error.column, ==, 3u);
}


RC_TEST_STEP(lexer, unicode_blankspace, fix)
{
    // Next line, the two marks, and the line and paragraph separators
    RC_CHECK(texts(RC_STR("a\xc2\x85" "b\xe2\x80\x8e" "c\xe2\x80\x8f" "d\xe2\x80\xa8" "e\xe2\x80\xa9"), &fix->a), ==, RC_STR("a b c d e"));
}

// ---- words ----

RC_TEST_STEP(lexer, identifiers, fix)
{
    RC_CHECK(only_type(RC_STR("x"), &fix->a), ==, RC_WGSL_TOKEN_TYPE_IDENT);
    RC_CHECK(only_type(RC_STR("_x"), &fix->a), ==, RC_WGSL_TOKEN_TYPE_IDENT);
    RC_CHECK(only_type(RC_STR("x_1"), &fix->a), ==, RC_WGSL_TOKEN_TYPE_IDENT);
    RC_CHECK(only_type(RC_STR("vec3f"), &fix->a), ==, RC_WGSL_TOKEN_TYPE_IDENT);
    RC_CHECK(only_type(RC_STR("Fn"), &fix->a), ==, RC_WGSL_TOKEN_TYPE_IDENT);
    RC_CHECK(only_type(RC_STR("_"), &fix->a), ==, RC_WGSL_TOKEN_TYPE_UNDERSCORE);
}


RC_TEST_STEP(lexer, every_keyword, fix)
{
    static const rc_str words[] = {
        RC_STR_INIT("alias"), RC_STR_INIT("break"), RC_STR_INIT("case"), RC_STR_INIT("const"),
        RC_STR_INIT("const_assert"), RC_STR_INIT("continue"), RC_STR_INIT("continuing"),
        RC_STR_INIT("default"), RC_STR_INIT("diagnostic"), RC_STR_INIT("discard"),
        RC_STR_INIT("else"), RC_STR_INIT("enable"), RC_STR_INIT("false"), RC_STR_INIT("fn"),
        RC_STR_INIT("for"), RC_STR_INIT("if"), RC_STR_INIT("let"), RC_STR_INIT("loop"),
        RC_STR_INIT("override"), RC_STR_INIT("requires"), RC_STR_INIT("return"),
        RC_STR_INIT("struct"), RC_STR_INIT("switch"), RC_STR_INIT("true"), RC_STR_INIT("var"),
        RC_STR_INIT("while")
    };

    // The keywords' types run in the same alphabetical order
    for (uint32_t i = 0; i < RC_COUNT(words); i++) {
        RC_CHECK(only_type(words[i], &fix->a), ==, RC_WGSL_TOKEN_TYPE_ALIAS + i);
    }
    RC_CHECK(RC_WGSL_TOKEN_TYPE_ALIAS + 25, ==, RC_WGSL_TOKEN_TYPE_WHILE);
}


RC_TEST_STEP(lexer, reserved_words, fix)
{
    RC_CHECK(error_of(RC_STR("x class"), &fix->a).message, ==, RC_STR("'class' is a reserved word"));
    RC_CHECK(error_of(RC_STR("x class"), &fix->a).offset, ==, 2u);

    // The first, the last, and some around the underscores
    RC_CHECK(error_of(RC_STR("NULL"), &fix->a).message, ==, RC_STR("'NULL' is a reserved word"));
    RC_CHECK(error_of(RC_STR("yield"), &fix->a).message, ==, RC_STR("'yield' is a reserved word"));
    RC_CHECK(error_of(RC_STR("const_cast"), &fix->a).message, ==, RC_STR("'const_cast' is a reserved word"));
    RC_CHECK(error_of(RC_STR("constexpr"), &fix->a).message, ==, RC_STR("'constexpr' is a reserved word"));

    // Near misses are fine
    RC_CHECK(only_type(RC_STR("classy"), &fix->a), ==, RC_WGSL_TOKEN_TYPE_IDENT);
    RC_CHECK(only_type(RC_STR("Null"), &fix->a), ==, RC_WGSL_TOKEN_TYPE_IDENT);
}


RC_TEST_STEP(lexer, two_underscores, fix)
{
    RC_CHECK(error_of(RC_STR("__x"), &fix->a).message, ==, RC_STR("names can't start with two underscores"));
    RC_CHECK(error_of(RC_STR("__"), &fix->a).message, ==, RC_STR("names can't start with two underscores"));
    RC_CHECK(only_type(RC_STR("x__"), &fix->a), ==, RC_WGSL_TOKEN_TYPE_IDENT);
}


RC_TEST_STEP(lexer, names_must_be_ascii, fix)
{
    rc_wgsl_error error = error_of(RC_STR("caf\xc3\xa9"), &fix->a);

    RC_CHECK(error.message, ==, RC_STR("names must be ASCII"));
    RC_CHECK(error.column, ==, 4u);
}

// ---- numbers ----

RC_TEST_STEP(lexer, integers, fix)
{
    static const rc_str ints[] = {
        RC_STR_INIT("0"), RC_STR_INIT("7"), RC_STR_INIT("123"), RC_STR_INIT("1i"),
        RC_STR_INIT("0u"), RC_STR_INIT("4294967295u"), RC_STR_INIT("0x0"),
        RC_STR_INIT("0x1F"), RC_STR_INIT("0XabCu"), RC_STR_INIT("0x1fi"), RC_STR_INIT("0x00ff")
    };

    for (uint32_t i = 0; i < RC_COUNT(ints); i++) {
        RC_CHECK(only_type(ints[i], &fix->a), ==, RC_WGSL_TOKEN_TYPE_INT);
    }
}


RC_TEST_STEP(lexer, floats, fix)
{
    static const rc_str floats[] = {
        RC_STR_INIT("1.0"), RC_STR_INIT(".5"), RC_STR_INIT("1."), RC_STR_INIT("00.5"),
        RC_STR_INIT("1e3"), RC_STR_INIT("1E-3"), RC_STR_INIT("1e+5h"), RC_STR_INIT("01e2"),
        RC_STR_INIT("1.5f"), RC_STR_INIT("1.f"), RC_STR_INIT("1f"), RC_STR_INIT("0f"),
        RC_STR_INIT("12h"), RC_STR_INIT("0x1p4"), RC_STR_INIT("0x1.8p1f"),
        RC_STR_INIT("0x.8"), RC_STR_INIT("0x8."), RC_STR_INIT("0x1P-2h")
    };

    for (uint32_t i = 0; i < RC_COUNT(floats); i++) {
        RC_CHECK(only_type(floats[i], &fix->a), ==, RC_WGSL_TOKEN_TYPE_FLOAT);
    }
}


RC_TEST_STEP(lexer, malformed_numbers, fix)
{
    rc_str leading = RC_STR("numbers can't start with 0, apart from 0 itself");
    rc_str runs_on = RC_STR("this number runs into what follows it");

    RC_CHECK(error_of(RC_STR("01"), &fix->a).message, ==, leading);
    RC_CHECK(error_of(RC_STR("00u"), &fix->a).message, ==, leading);
    RC_CHECK(error_of(RC_STR("01f"), &fix->a).message, ==, leading);
    RC_CHECK(error_of(RC_STR("0x"), &fix->a).message, ==, RC_STR("this hex number has no digits"));
    RC_CHECK(error_of(RC_STR("0x.p1"), &fix->a).message, ==, RC_STR("this hex number has no digits"));
    RC_CHECK(error_of(RC_STR("1e"), &fix->a).message, ==, RC_STR("this exponent has no digits"));
    RC_CHECK(error_of(RC_STR("1e+"), &fix->a).message, ==, RC_STR("this exponent has no digits"));
    RC_CHECK(error_of(RC_STR("0x1p"), &fix->a).message, ==, RC_STR("this exponent has no digits"));
    RC_CHECK(error_of(RC_STR("1x"), &fix->a).message, ==, runs_on);
    RC_CHECK(error_of(RC_STR("1.2.3"), &fix->a).message, ==, runs_on);
    RC_CHECK(error_of(RC_STR("1.5u"), &fix->a).message, ==, runs_on);
    RC_CHECK(error_of(RC_STR("1if"), &fix->a).message, ==, runs_on);
    RC_CHECK(error_of(RC_STR("0x1.8h"), &fix->a).message, ==, runs_on);
    RC_CHECK(error_of(RC_STR("0x1g"), &fix->a).message, ==, runs_on);
}


RC_TEST_STEP(lexer, numbers_among_other_tokens, fix)
{
    RC_CHECK(texts(RC_STR("a[0].x"), &fix->a), ==, RC_STR("a [ 0 ] . x"));
    RC_CHECK(texts(RC_STR("1+2"), &fix->a), ==, RC_STR("1 + 2"));
    RC_CHECK(texts(RC_STR("-1.5e-3f*x"), &fix->a), ==, RC_STR("- 1.5e-3f * x"));
}

// ---- punctuation ----

RC_TEST_STEP(lexer, every_punctuation, fix)
{
    static const struct {
        rc_str text;
        rc_wgsl_token_type type;
    } puncts[] = {
        {RC_STR_INIT("&"), RC_WGSL_TOKEN_TYPE_AND},
        {RC_STR_INIT("&&"), RC_WGSL_TOKEN_TYPE_AND_AND},
        {RC_STR_INIT("&="), RC_WGSL_TOKEN_TYPE_AND_EQUAL},
        {RC_STR_INIT("->"), RC_WGSL_TOKEN_TYPE_ARROW},
        {RC_STR_INIT("@"), RC_WGSL_TOKEN_TYPE_AT},
        {RC_STR_INIT("!"), RC_WGSL_TOKEN_TYPE_BANG},
        {RC_STR_INIT("!="), RC_WGSL_TOKEN_TYPE_BANG_EQUAL},
        {RC_STR_INIT(":"), RC_WGSL_TOKEN_TYPE_COLON},
        {RC_STR_INIT(","), RC_WGSL_TOKEN_TYPE_COMMA},
        {RC_STR_INIT("="), RC_WGSL_TOKEN_TYPE_EQUAL},
        {RC_STR_INIT("=="), RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL},
        {RC_STR_INIT(">"), RC_WGSL_TOKEN_TYPE_GREATER},
        {RC_STR_INIT(">="), RC_WGSL_TOKEN_TYPE_GREATER_EQUAL},
        {RC_STR_INIT("{"), RC_WGSL_TOKEN_TYPE_LEFT_BRACE},
        {RC_STR_INIT("["), RC_WGSL_TOKEN_TYPE_LEFT_BRACKET},
        {RC_STR_INIT("("), RC_WGSL_TOKEN_TYPE_LEFT_PAREN},
        {RC_STR_INIT("<"), RC_WGSL_TOKEN_TYPE_LESS},
        {RC_STR_INIT("<="), RC_WGSL_TOKEN_TYPE_LESS_EQUAL},
        {RC_STR_INIT("-"), RC_WGSL_TOKEN_TYPE_MINUS},
        {RC_STR_INIT("-="), RC_WGSL_TOKEN_TYPE_MINUS_EQUAL},
        {RC_STR_INIT("--"), RC_WGSL_TOKEN_TYPE_MINUS_MINUS},
        {RC_STR_INIT("|"), RC_WGSL_TOKEN_TYPE_OR},
        {RC_STR_INIT("|="), RC_WGSL_TOKEN_TYPE_OR_EQUAL},
        {RC_STR_INIT("||"), RC_WGSL_TOKEN_TYPE_OR_OR},
        {RC_STR_INIT("%"), RC_WGSL_TOKEN_TYPE_PERCENT},
        {RC_STR_INIT("%="), RC_WGSL_TOKEN_TYPE_PERCENT_EQUAL},
        {RC_STR_INIT("."), RC_WGSL_TOKEN_TYPE_PERIOD},
        {RC_STR_INIT("+"), RC_WGSL_TOKEN_TYPE_PLUS},
        {RC_STR_INIT("+="), RC_WGSL_TOKEN_TYPE_PLUS_EQUAL},
        {RC_STR_INIT("++"), RC_WGSL_TOKEN_TYPE_PLUS_PLUS},
        {RC_STR_INIT("}"), RC_WGSL_TOKEN_TYPE_RIGHT_BRACE},
        {RC_STR_INIT("]"), RC_WGSL_TOKEN_TYPE_RIGHT_BRACKET},
        {RC_STR_INIT(")"), RC_WGSL_TOKEN_TYPE_RIGHT_PAREN},
        {RC_STR_INIT(";"), RC_WGSL_TOKEN_TYPE_SEMICOLON},
        {RC_STR_INIT("<<"), RC_WGSL_TOKEN_TYPE_SHIFT_LEFT},
        {RC_STR_INIT("<<="), RC_WGSL_TOKEN_TYPE_SHIFT_LEFT_EQUAL},
        {RC_STR_INIT(">>"), RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT},
        {RC_STR_INIT(">>="), RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT_EQUAL},
        {RC_STR_INIT("/"), RC_WGSL_TOKEN_TYPE_SLASH},
        {RC_STR_INIT("/="), RC_WGSL_TOKEN_TYPE_SLASH_EQUAL},
        {RC_STR_INIT("*"), RC_WGSL_TOKEN_TYPE_STAR},
        {RC_STR_INIT("*="), RC_WGSL_TOKEN_TYPE_STAR_EQUAL},
        {RC_STR_INIT("~"), RC_WGSL_TOKEN_TYPE_TILDE},
        {RC_STR_INIT("^"), RC_WGSL_TOKEN_TYPE_XOR},
        {RC_STR_INIT("^="), RC_WGSL_TOKEN_TYPE_XOR_EQUAL}
    };

    for (uint32_t i = 0; i < RC_COUNT(puncts); i++) {
        RC_CHECK(only_type(puncts[i].text, &fix->a), ==, puncts[i].type);
    }
}


RC_TEST_STEP(lexer, greaters_join_only_when_touching, fix)
{
    RC_CHECK(texts(RC_STR("a>=b"), &fix->a), ==, RC_STR("a >= b"));
    RC_CHECK(texts(RC_STR("a > = b"), &fix->a), ==, RC_STR("a > = b"));
    RC_CHECK(texts(RC_STR("a >> b"), &fix->a), ==, RC_STR("a >> b"));
    RC_CHECK(texts(RC_STR("a > > b"), &fix->a), ==, RC_STR("a > > b"));
    RC_CHECK(texts(RC_STR("a >>= b"), &fix->a), ==, RC_STR("a >>= b"));
    RC_CHECK(texts(RC_STR("a >> = b"), &fix->a), ==, RC_STR("a >> = b"));
}


RC_TEST_STEP(lexer, stray_characters, fix)
{
    rc_wgsl_error error = error_of(RC_STR("fn\n  $"), &fix->a);

    RC_CHECK(error.message, ==, RC_STR("this character has no place in WGSL"));
    RC_CHECK(error.line, ==, 2u);
    RC_CHECK(error.column, ==, 3u);
    RC_CHECK(error_of(RC_STR("a # b"), &fix->a).offset, ==, 2u);
    RC_CHECK(error_of(RC_STR("\"s\""), &fix->a).offset, ==, 0u);
}


RC_TEST_STEP(lexer, lines_end_every_way, fix)
{
    // LF, CR LF and a lone CR each end one line
    RC_CHECK(error_of(RC_STR("a\nb\r\nc\rd $"), &fix->a).line, ==, 4u);
    RC_CHECK(error_of(RC_STR("a\nb\r\nc\rd $"), &fix->a).column, ==, 3u);
}

// ---- template lists ----

RC_TEST_STEP(lexer, templates, fix)
{
    RC_CHECK(texts(RC_STR("vec3<f32>(1.0)"), &fix->a), ==, RC_STR("vec3 <T f32 T> ( 1.0 )"));
    RC_CHECK(texts(RC_STR("var<private> x: i32;"), &fix->a), ==, RC_STR("var <T private T> x : i32 ;"));
    RC_CHECK(texts(RC_STR("array<f32, 4>"), &fix->a), ==, RC_STR("array <T f32 , 4 T>"));
}


RC_TEST_STEP(lexer, nested_templates_split_greaters, fix)
{
    RC_CHECK(texts(RC_STR("array<vec2<f32>>"), &fix->a), ==, RC_STR("array <T vec2 <T f32 T> T>"));
    RC_CHECK(texts(RC_STR("ptr<function, array<f32, 4>>"), &fix->a), ==, RC_STR("ptr <T function , array <T f32 , 4 T> T>"));
    RC_CHECK(texts(RC_STR("a<b<c<d>>>"), &fix->a), ==, RC_STR("a <T b <T c <T d T> T> T>"));
}


RC_TEST_STEP(lexer, comparisons_stay_comparisons, fix)
{
    RC_CHECK(texts(RC_STR("a < b"), &fix->a), ==, RC_STR("a < b"));
    RC_CHECK(texts(RC_STR("if a < b { }"), &fix->a), ==, RC_STR("if a < b { }"));
    RC_CHECK(texts(RC_STR("a < b || c > d"), &fix->a), ==, RC_STR("a < b || c > d"));
    RC_CHECK(texts(RC_STR("a < b && c > d"), &fix->a), ==, RC_STR("a < b && c > d"));
    RC_CHECK(texts(RC_STR("(a < b) == (c > d)"), &fix->a), ==, RC_STR("( a < b ) == ( c > d )"));
    RC_CHECK(texts(RC_STR("x = a < b; y = c > d;"), &fix->a), ==, RC_STR("x = a < b ; y = c > d ;"));
    RC_CHECK(texts(RC_STR("a<<b"), &fix->a), ==, RC_STR("a << b"));
    RC_CHECK(texts(RC_STR("a<=b"), &fix->a), ==, RC_STR("a <= b"));
    RC_CHECK(texts(RC_STR("a[b<c]>d"), &fix->a), ==, RC_STR("a [ b < c ] > d"));
}


RC_TEST_STEP(lexer, the_spec_s_own_surprises, fix)
{
    // WGSL reads these as template lists, and wants parentheses to say otherwise
    RC_CHECK(texts(RC_STR("f(x < y, z > w)"), &fix->a), ==, RC_STR("f ( x <T y , z T> w )"));
    RC_CHECK(texts(RC_STR("a<b>=c"), &fix->a), ==, RC_STR("a <T b T> = c"));
}


RC_TEST_STEP(lexer, templates_close_before_shifts, fix)
{
    RC_CHECK(texts(RC_STR("vec2<i32>(x >> 1)"), &fix->a), ==, RC_STR("vec2 <T i32 T> ( x >> 1 )"));
    RC_CHECK(texts(RC_STR("x >>= vec2<u32>(1)"), &fix->a), ==, RC_STR("x >>= vec2 <T u32 T> ( 1 )"));
}


RC_TEST_STEP(lexer, token_offsets, fix)
{
    rc_str source = RC_STR("  fn f() -> vec3<f32> {}");
    rc_wgsl_lex_result lexed = rc_wgsl_lex(source, &fix->a);
    rc_wgsl_token arrow = rc_view_wgsl_token_get(lexed.tokens, 4);
    rc_wgsl_token end = rc_view_wgsl_token_get(lexed.tokens, lexed.tokens.num - 1);

    RC_CHECK(rc_view_wgsl_token_get(lexed.tokens, 0).offset, ==, 2u);
    RC_CHECK(rc_view_wgsl_token_get(lexed.tokens, 0).len, ==, 2u);
    RC_CHECK(arrow.type, ==, RC_WGSL_TOKEN_TYPE_ARROW);
    RC_CHECK(rc_wgsl_token_text(source, arrow), ==, RC_STR("->"));
    RC_CHECK(rc_wgsl_token_text(source, rc_view_wgsl_token_get(lexed.tokens, 6)), ==, RC_STR("<"));
    RC_CHECK(end.type, ==, RC_WGSL_TOKEN_TYPE_END);
    RC_CHECK(end.offset, ==, source.len);
    RC_CHECK(end.len, ==, 0u);
}
