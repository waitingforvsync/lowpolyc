// lexer.c
//
// WGSL source to tokens, in three passes: scan the tokens, discover the template lists, then join
// each '>' that is left over with the '>' or '=' straight after it.
//
// Identifiers are ASCII only. WGSL allows Unicode ones, but GLSL does not, and we would rather say
// so than rename them.

#include "lexer.h"

#include "error.h"
#include "richc/mstr.h"

// ---- words ----

// A keyword or reserved word, and its token type.
// Reserved words have no token type, and are refused.
typedef struct rc_wgsl_word {
    rc_str text;
    uint8_t type;  // rc_wgsl_token_type
} rc_wgsl_word;

#define RC_ARRAY_TYPE rc_wgsl_word
#define RC_ARRAY_NAME wgsl_word
#include "richc/array.template.h"

#define RC_LOWER_BOUND_TYPE rc_wgsl_word
#define RC_LOWER_BOUND_NAME rc_lower_bound_wgsl_word
#define RC_LOWER_BOUND_VIEW rc_view_wgsl_word
#define RC_LOWER_BOUND_KEY(w) ((w).text)
#define RC_LOWER_BOUND_KEY_TYPE rc_str
#define RC_LOWER_BOUND_CMP(a, b) (rc_str_compare((a), (b)) < 0)
#include "richc/algorithm/lower_bound.template.h"

// Every keyword and reserved word, in byte order for the binary search.
static const rc_wgsl_word rc_wgsl_words[] = {
    {RC_STR_INIT("NULL"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("Self"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("abstract"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("active"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("alias"), RC_WGSL_TOKEN_TYPE_ALIAS},
    {RC_STR_INIT("alignas"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("alignof"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("as"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("asm"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("asm_fragment"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("async"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("attribute"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("auto"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("await"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("become"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("break"), RC_WGSL_TOKEN_TYPE_BREAK},
    {RC_STR_INIT("case"), RC_WGSL_TOKEN_TYPE_CASE},
    {RC_STR_INIT("cast"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("catch"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("class"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("co_await"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("co_return"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("co_yield"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("coherent"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("column_major"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("common"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("compile"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("compile_fragment"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("concept"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("const"), RC_WGSL_TOKEN_TYPE_CONST},
    {RC_STR_INIT("const_assert"), RC_WGSL_TOKEN_TYPE_CONST_ASSERT},
    {RC_STR_INIT("const_cast"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("consteval"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("constexpr"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("constinit"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("continue"), RC_WGSL_TOKEN_TYPE_CONTINUE},
    {RC_STR_INIT("continuing"), RC_WGSL_TOKEN_TYPE_CONTINUING},
    {RC_STR_INIT("crate"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("debugger"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("decltype"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("default"), RC_WGSL_TOKEN_TYPE_DEFAULT},
    {RC_STR_INIT("delete"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("demote"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("demote_to_helper"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("diagnostic"), RC_WGSL_TOKEN_TYPE_DIAGNOSTIC},
    {RC_STR_INIT("discard"), RC_WGSL_TOKEN_TYPE_DISCARD},
    {RC_STR_INIT("do"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("dynamic_cast"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("else"), RC_WGSL_TOKEN_TYPE_ELSE},
    {RC_STR_INIT("enable"), RC_WGSL_TOKEN_TYPE_ENABLE},
    {RC_STR_INIT("enum"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("explicit"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("export"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("extends"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("extern"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("external"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("fallthrough"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("false"), RC_WGSL_TOKEN_TYPE_FALSE},
    {RC_STR_INIT("filter"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("final"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("finally"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("fn"), RC_WGSL_TOKEN_TYPE_FN},
    {RC_STR_INIT("for"), RC_WGSL_TOKEN_TYPE_FOR},
    {RC_STR_INIT("friend"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("from"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("fxgroup"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("get"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("goto"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("groupshared"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("highp"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("if"), RC_WGSL_TOKEN_TYPE_IF},
    {RC_STR_INIT("impl"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("implements"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("import"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("inline"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("instanceof"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("interface"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("layout"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("let"), RC_WGSL_TOKEN_TYPE_LET},
    {RC_STR_INIT("loop"), RC_WGSL_TOKEN_TYPE_LOOP},
    {RC_STR_INIT("lowp"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("macro"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("macro_rules"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("match"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("mediump"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("meta"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("mod"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("module"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("move"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("mut"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("mutable"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("namespace"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("new"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("nil"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("noexcept"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("noinline"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("nointerpolation"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("non_coherent"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("noncoherent"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("noperspective"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("null"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("nullptr"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("of"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("operator"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("override"), RC_WGSL_TOKEN_TYPE_OVERRIDE},
    {RC_STR_INIT("package"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("packoffset"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("partition"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("pass"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("patch"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("pixelfragment"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("precise"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("precision"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("premerge"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("priv"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("protected"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("pub"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("public"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("readonly"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("ref"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("regardless"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("register"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("reinterpret_cast"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("require"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("requires"), RC_WGSL_TOKEN_TYPE_REQUIRES},
    {RC_STR_INIT("resource"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("restrict"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("return"), RC_WGSL_TOKEN_TYPE_RETURN},
    {RC_STR_INIT("self"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("set"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("shared"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("sizeof"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("smooth"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("snorm"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("static"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("static_assert"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("static_cast"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("std"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("struct"), RC_WGSL_TOKEN_TYPE_STRUCT},
    {RC_STR_INIT("subroutine"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("super"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("switch"), RC_WGSL_TOKEN_TYPE_SWITCH},
    {RC_STR_INIT("target"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("template"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("this"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("thread_local"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("throw"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("trait"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("true"), RC_WGSL_TOKEN_TYPE_TRUE},
    {RC_STR_INIT("try"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("type"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("typedef"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("typeid"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("typename"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("typeof"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("union"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("unless"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("unorm"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("unsafe"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("unsized"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("use"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("using"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("var"), RC_WGSL_TOKEN_TYPE_VAR},
    {RC_STR_INIT("varying"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("virtual"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("volatile"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("wgsl"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("where"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("while"), RC_WGSL_TOKEN_TYPE_WHILE},
    {RC_STR_INIT("with"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("writeonly"), RC_WGSL_TOKEN_TYPE_NONE},
    {RC_STR_INIT("yield"), RC_WGSL_TOKEN_TYPE_NONE},
};

// The keyword or reserved word w, or {0} if it is an ordinary identifier.
static rc_wgsl_word rc_wgsl_find_word(rc_str w)
{
    rc_view_wgsl_word words = RC_VIEW(rc_wgsl_words);
    uint32_t i = rc_lower_bound_wgsl_word(words, w);

    if (i < words.num && rc_str_is_equal(rc_view_wgsl_word_get(words, i).text, w)) {
        return rc_view_wgsl_word_get(words, i);
    }

    return (rc_wgsl_word) {0};
}

// ---- characters ----

// The character at pos, or '\0' past the end, which no token continues with.
static char rc_wgsl_char_at(rc_str source, uint32_t pos)
{
    return pos < source.len ? source.data[pos] : '\0';
}


// True for first to last, inclusive.
static bool rc_wgsl_is_between(char c, char first, char last)
{
    return c >= first && c <= last;
}


// c in lower case if it is an ASCII letter. Nothing else turns into a letter.
static char rc_wgsl_lower(char c)
{
    return (char)(c | 0x20);
}


// True for 0 to 9.
static bool rc_wgsl_is_digit(char c)
{
    return rc_wgsl_is_between(c, '0', '9');
}


// True for 0 to 9 and a to f in either case.
static bool rc_wgsl_is_hex_digit(char c)
{
    return rc_wgsl_is_digit(c) || rc_wgsl_is_between(rc_wgsl_lower(c), 'a', 'f');
}


// True for a character that can start a word: an ASCII letter or an underscore.
static bool rc_wgsl_is_word_start(char c)
{
    return rc_wgsl_is_between(rc_wgsl_lower(c), 'a', 'z') || c == '_';
}


// True for a character that can carry a word on.
static bool rc_wgsl_is_word_char(char c)
{
    return rc_wgsl_is_word_start(c) || rc_wgsl_is_digit(c);
}


// The bytes in the run of digits from pos, hex or decimal.
static uint32_t rc_wgsl_count_digits(rc_str source, uint32_t pos, bool hex)
{
    uint32_t end = pos;

    while (hex ? rc_wgsl_is_hex_digit(rc_wgsl_char_at(source, end)) : rc_wgsl_is_digit(rc_wgsl_char_at(source, end))) {
        end++;
    }
    return end - pos;
}

// ---- blankspace and comments ----

// Where scanning goes on from, or why it cannot.
typedef struct rc_wgsl_skip {
    uint32_t pos;
    rc_str error;
} rc_wgsl_skip;

// True for an ASCII blank.
static bool rc_wgsl_is_ascii_blank(char c)
{
    switch (c) {
    case ' ':
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
        return true;
    default:
        return false;
    }
}


// The length of an ASCII blank at pos, or 0.
static uint32_t rc_wgsl_ascii_blank_len(rc_str source, uint32_t pos)
{
    return rc_wgsl_is_ascii_blank(rc_wgsl_char_at(source, pos)) ? 1 : 0;
}


// The length of a blank outside ASCII at pos in UTF-8, or 0: next line (U+0085), the left-to-right
// and right-to-left marks (U+200E, U+200F), and the line and paragraph separators (U+2028, U+2029).
static uint32_t rc_wgsl_unicode_blank_len(rc_str source, uint32_t pos)
{
    rc_str rest = rc_str_drop_first(source, pos);

    if (rc_str_starts_with(rest, RC_STR("\xc2\x85"))) {
        return 2;
    }

    if (!rc_str_starts_with(rest, RC_STR("\xe2\x80"))) {
        return 0;
    }

    switch ((uint8_t)rc_wgsl_char_at(source, pos + 2)) {
    case 0x8e:
    case 0x8f:
    case 0xa8:
    case 0xa9:
        return 3;
    default:
        return 0;
    }
}


// True for a character that ends a line.
static bool rc_wgsl_is_line_break(char c)
{
    return c == '\n' || c == '\r';
}


// The position just past the line comment at pos, at its line break.
static uint32_t rc_wgsl_skip_line_comment(rc_str source, uint32_t pos)
{
    while (pos < source.len && !rc_wgsl_is_line_break(source.data[pos])) {
        pos++;
    }
    return pos;
}


// The position just past the block comment at pos. Block comments nest.
static rc_wgsl_skip rc_wgsl_skip_block_comment(rc_str source, uint32_t pos)
{
    uint32_t depth = 0;
    uint32_t start = pos;

    while (pos < source.len) {
        rc_str rest = rc_str_drop_first(source, pos);

        if (rc_str_starts_with(rest, RC_STR("/*"))) {
            depth++;
            pos += 2;
        }
        else if (rc_str_starts_with(rest, RC_STR("*/"))) {
            pos += 2;
            if (--depth == 0) {
                return (rc_wgsl_skip) {.pos = pos};
            }
        }
        else {
            pos++;
        }
    }
    return (rc_wgsl_skip) {.pos = start, .error = RC_STR("this comment never ends")};
}


// The position of the next token at or after pos, past any blankspace and comments.
static rc_wgsl_skip rc_wgsl_skip_blank(rc_str source, uint32_t pos)
{
    while (pos < source.len) {
        rc_str rest = rc_str_drop_first(source, pos);
        uint32_t blank = rc_wgsl_ascii_blank_len(source, pos) + rc_wgsl_unicode_blank_len(source, pos);

        if (blank > 0) {
            pos += blank;
        }
        else if (rc_str_starts_with(rest, RC_STR("//"))) {
            pos = rc_wgsl_skip_line_comment(source, pos);
        }
        else if (rc_str_starts_with(rest, RC_STR("/*"))) {
            rc_wgsl_skip comment = rc_wgsl_skip_block_comment(source, pos);

            if (rc_str_is_valid(comment.error)) {
                return comment;
            }

            pos = comment.pos;
        }
        else {
            break;
        }
    }
    return (rc_wgsl_skip) {.pos = pos};
}

// ---- tokens ----

// One scanned token, or why there is none.
typedef struct rc_wgsl_scan {
    rc_wgsl_token token;
    rc_str error;
} rc_wgsl_scan;

// A token of type covering len bytes from pos.
static rc_wgsl_scan rc_wgsl_scanned(rc_wgsl_token_type type, uint32_t pos, uint32_t len)
{
    return (rc_wgsl_scan) {
        .token = {.offset = pos, .len = len, .type = (uint8_t)type}
    };
}


// An error at pos.
static rc_wgsl_scan rc_wgsl_scan_error(uint32_t pos, rc_str error)
{
    return (rc_wgsl_scan) {
        .token = {.offset = pos},
        .error = error
    };
}


// The bytes in the run of word characters from pos.
static uint32_t rc_wgsl_count_word_chars(rc_str source, uint32_t pos)
{
    uint32_t end = pos;

    while (rc_wgsl_is_word_char(rc_wgsl_char_at(source, end))) {
        end++;
    }
    return end - pos;
}


// The identifier, keyword or underscore at pos.
static rc_wgsl_scan rc_wgsl_scan_word(rc_str source, uint32_t pos, rc_arena *arena)
{
    rc_str text = rc_str_substr(source, pos, pos + rc_wgsl_count_word_chars(source, pos));
    rc_wgsl_word word = rc_wgsl_find_word(text);

    if (rc_str_is_equal(text, RC_STR("_"))) {
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_UNDERSCORE, pos, 1);
    }

    if (rc_str_starts_with(text, RC_STR("__"))) {
        return rc_wgsl_scan_error(pos, RC_STR("names can't start with two underscores"));
    }

    if (!rc_str_is_valid(word.text)) {
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_IDENT, pos, text.len);
    }

    if (word.type == RC_WGSL_TOKEN_TYPE_NONE) {
        rc_mstr message = rc_mstr_from_str(RC_STR("'"), 64, arena);

        rc_mstr_append(&message, text, arena);
        rc_mstr_append(&message, RC_STR("' is a reserved word"), arena);
        return rc_wgsl_scan_error(pos, message.view);
    }

    return rc_wgsl_scanned((rc_wgsl_token_type)word.type, pos, text.len);
}


// The length of the exponent at pos ('e' or 'p', a sign, digits), 0 if there is none, or
// RC_INDEX_NONE if it has no digits.
static uint32_t rc_wgsl_exponent_len(rc_str source, uint32_t pos, char letter)
{
    char sign = rc_wgsl_char_at(source, pos + 1);
    uint32_t start = pos + 1 + (sign == '+' || sign == '-' ? 1 : 0);
    uint32_t digits = rc_wgsl_count_digits(source, start, false);

    if (rc_wgsl_lower(rc_wgsl_char_at(source, pos)) != letter) {
        return 0;
    }

    return digits == 0 ? RC_INDEX_NONE : start + digits - pos;
}


// A literal's body, before its suffix: where it ends, and whether a point or an exponent makes it
// a float.
typedef struct rc_wgsl_number_body {
    uint32_t end;
    bool point;
    bool exponent;
    rc_str error;
} rc_wgsl_number_body;

// A body that ends at end, then has an exponent of the length rc_wgsl_exponent_len gave.
static rc_wgsl_number_body rc_wgsl_body_with_exponent(uint32_t end, bool point, uint32_t exponent)
{
    if (exponent == RC_INDEX_NONE) {
        return (rc_wgsl_number_body) {.end = end, .error = RC_STR("this exponent has no digits")};
    }

    return (rc_wgsl_number_body) {
        .end = end + exponent,
        .point = point,
        .exponent = exponent > 0
    };
}


// The body of the hex literal at pos, from its 0x: digits, a point and a binary exponent.
static rc_wgsl_number_body rc_wgsl_hex_body(rc_str source, uint32_t pos)
{
    uint32_t whole = rc_wgsl_count_digits(source, pos + 2, true);
    bool point = rc_wgsl_char_at(source, pos + 2 + whole) == '.';
    uint32_t fraction = point ? rc_wgsl_count_digits(source, pos + 3 + whole, true) : 0;
    uint32_t end = pos + 2 + whole + (point ? 1 + fraction : 0);

    if (whole + fraction == 0) {
        return (rc_wgsl_number_body) {.end = end, .error = RC_STR("this hex number has no digits")};
    }

    return rc_wgsl_body_with_exponent(end, point, rc_wgsl_exponent_len(source, end, 'p'));
}


// The body of the decimal literal at pos: digits, a point and an exponent.
static rc_wgsl_number_body rc_wgsl_decimal_body(rc_str source, uint32_t pos)
{
    uint32_t whole_end = pos + rc_wgsl_count_digits(source, pos, false);
    bool point = rc_wgsl_char_at(source, whole_end) == '.';
    uint32_t end = point ? whole_end + 1 + rc_wgsl_count_digits(source, whole_end + 1, false) : whole_end;

    return rc_wgsl_body_with_exponent(end, point, rc_wgsl_exponent_len(source, end, 'e'));
}


// True if a point or an exponent makes a number's body a float.
static bool rc_wgsl_body_is_float(rc_wgsl_number_body body)
{
    return body.point || body.exponent;
}


// True for the suffixes that make a number a float.
static bool rc_wgsl_is_float_suffix(char c)
{
    return c == 'f' || c == 'h';
}


// The suffix after a number's body that belongs to it: i or u on an integer, f or h on anything
// but a hex number with no exponent (where f would already be a digit). 0 if there is none.
static char rc_wgsl_number_suffix(rc_str source, rc_wgsl_number_body body, bool hex)
{
    char c = rc_wgsl_char_at(source, body.end);

    if (c == 'i' || c == 'u') {
        return rc_wgsl_body_is_float(body) ? 0 : c;
    }

    if (rc_wgsl_is_float_suffix(c)) {
        return !hex || body.exponent ? c : 0;
    }

    return 0;
}


// True for a decimal integer's body that starts with a 0 and goes on. 01f counts too.
static bool rc_wgsl_has_leading_zero(rc_str source, uint32_t pos, rc_wgsl_number_body body, bool hex)
{
    if (hex || rc_wgsl_body_is_float(body)) {
        return false;
    }

    return body.end - pos > 1 && rc_wgsl_char_at(source, pos) == '0';
}


// The number literal at pos, which starts with a digit, or with a point and then a digit.
static rc_wgsl_scan rc_wgsl_scan_number(rc_str source, uint32_t pos)
{
    bool hex = rc_wgsl_char_at(source, pos) == '0' && rc_wgsl_lower(rc_wgsl_char_at(source, pos + 1)) == 'x';
    rc_wgsl_number_body body = hex ? rc_wgsl_hex_body(source, pos) : rc_wgsl_decimal_body(source, pos);
    char suffix = rc_wgsl_number_suffix(source, body, hex);
    bool is_float = rc_wgsl_body_is_float(body) || rc_wgsl_is_float_suffix(suffix);
    uint32_t end = body.end + (suffix != 0 ? 1 : 0);
    char after = rc_wgsl_char_at(source, end);

    if (rc_str_is_valid(body.error)) {
        return rc_wgsl_scan_error(pos, body.error);
    }

    if (rc_wgsl_has_leading_zero(source, pos, body, hex)) {
        return rc_wgsl_scan_error(pos, RC_STR("numbers can't start with 0, apart from 0 itself"));
    }

    if (rc_wgsl_is_word_char(after) || after == '.') {
        return rc_wgsl_scan_error(pos, RC_STR("this number runs into what follows it"));
    }

    return rc_wgsl_scanned(is_float ? RC_WGSL_TOKEN_TYPE_FLOAT : RC_WGSL_TOKEN_TYPE_INT, pos, end - pos);
}


// The type of a one-character token that may be followed by '=', and its length.
static rc_wgsl_scan rc_wgsl_or_equal(rc_str source, uint32_t pos, rc_wgsl_token_type plain, rc_wgsl_token_type with_equal)
{
    if (rc_wgsl_char_at(source, pos + 1) == '=') {
        return rc_wgsl_scanned(with_equal, pos, 2);
    }

    return rc_wgsl_scanned(plain, pos, 1);
}


// The token at pos that starts with a character that can also be doubled: &&, ||, ++ and --.
static rc_wgsl_scan rc_wgsl_or_double(rc_str source, uint32_t pos, rc_wgsl_token_type plain,
    rc_wgsl_token_type doubled, rc_wgsl_token_type with_equal)
{
    if (rc_wgsl_char_at(source, pos + 1) == rc_wgsl_char_at(source, pos)) {
        return rc_wgsl_scanned(doubled, pos, 2);
    }

    return rc_wgsl_or_equal(source, pos, plain, with_equal);
}


// The token at pos that starts with '<'.
static rc_wgsl_scan rc_wgsl_scan_less(rc_str source, uint32_t pos)
{
    if (rc_wgsl_char_at(source, pos + 1) == '<') {
        if (rc_wgsl_char_at(source, pos + 2) == '=') {
            return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_SHIFT_LEFT_EQUAL, pos, 3);
        }

        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_SHIFT_LEFT, pos, 2);
    }

    return rc_wgsl_or_equal(source, pos, RC_WGSL_TOKEN_TYPE_LESS, RC_WGSL_TOKEN_TYPE_LESS_EQUAL);
}


// The token at pos that starts with '-'.
static rc_wgsl_scan rc_wgsl_scan_minus(rc_str source, uint32_t pos)
{
    if (rc_wgsl_char_at(source, pos + 1) == '>') {
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_ARROW, pos, 2);
    }

    return rc_wgsl_or_double(source, pos, RC_WGSL_TOKEN_TYPE_MINUS, RC_WGSL_TOKEN_TYPE_MINUS_MINUS, RC_WGSL_TOKEN_TYPE_MINUS_EQUAL);
}


// The punctuation at pos. A '>' is always alone for now: see lexer.h.
static rc_wgsl_scan rc_wgsl_scan_punctuation(rc_str source, uint32_t pos)
{
    switch (rc_wgsl_char_at(source, pos)) {
    case '&':
        return rc_wgsl_or_double(source, pos, RC_WGSL_TOKEN_TYPE_AND, RC_WGSL_TOKEN_TYPE_AND_AND, RC_WGSL_TOKEN_TYPE_AND_EQUAL);
    case '|':
        return rc_wgsl_or_double(source, pos, RC_WGSL_TOKEN_TYPE_OR, RC_WGSL_TOKEN_TYPE_OR_OR, RC_WGSL_TOKEN_TYPE_OR_EQUAL);
    case '+':
        return rc_wgsl_or_double(source, pos, RC_WGSL_TOKEN_TYPE_PLUS, RC_WGSL_TOKEN_TYPE_PLUS_PLUS, RC_WGSL_TOKEN_TYPE_PLUS_EQUAL);
    case '-':
        return rc_wgsl_scan_minus(source, pos);
    case '<':
        return rc_wgsl_scan_less(source, pos);
    case '!':
        return rc_wgsl_or_equal(source, pos, RC_WGSL_TOKEN_TYPE_BANG, RC_WGSL_TOKEN_TYPE_BANG_EQUAL);
    case '=':
        return rc_wgsl_or_equal(source, pos, RC_WGSL_TOKEN_TYPE_EQUAL, RC_WGSL_TOKEN_TYPE_EQUAL_EQUAL);
    case '%':
        return rc_wgsl_or_equal(source, pos, RC_WGSL_TOKEN_TYPE_PERCENT, RC_WGSL_TOKEN_TYPE_PERCENT_EQUAL);
    case '*':
        return rc_wgsl_or_equal(source, pos, RC_WGSL_TOKEN_TYPE_STAR, RC_WGSL_TOKEN_TYPE_STAR_EQUAL);
    case '/':
        return rc_wgsl_or_equal(source, pos, RC_WGSL_TOKEN_TYPE_SLASH, RC_WGSL_TOKEN_TYPE_SLASH_EQUAL);
    case '^':
        return rc_wgsl_or_equal(source, pos, RC_WGSL_TOKEN_TYPE_XOR, RC_WGSL_TOKEN_TYPE_XOR_EQUAL);
    case '>':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_GREATER, pos, 1);
    case '@':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_AT, pos, 1);
    case ':':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_COLON, pos, 1);
    case ',':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_COMMA, pos, 1);
    case '{':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_LEFT_BRACE, pos, 1);
    case '}':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_RIGHT_BRACE, pos, 1);
    case '[':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_LEFT_BRACKET, pos, 1);
    case ']':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_RIGHT_BRACKET, pos, 1);
    case '(':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_LEFT_PAREN, pos, 1);
    case ')':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_RIGHT_PAREN, pos, 1);
    case '.':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_PERIOD, pos, 1);
    case ';':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_SEMICOLON, pos, 1);
    case '~':
        return rc_wgsl_scanned(RC_WGSL_TOKEN_TYPE_TILDE, pos, 1);
    default:
        break;
    }

    if ((uint8_t)rc_wgsl_char_at(source, pos) >= 0x80) {
        return rc_wgsl_scan_error(pos, RC_STR("names must be ASCII"));
    }

    return rc_wgsl_scan_error(pos, RC_STR("this character has no place in WGSL"));
}


// True if a number starts at pos: a digit, or a point and then a digit.
static bool rc_wgsl_is_number_start(rc_str source, uint32_t pos)
{
    char c = rc_wgsl_char_at(source, pos);

    if (c == '.') {
        return rc_wgsl_is_digit(rc_wgsl_char_at(source, pos + 1));
    }

    return rc_wgsl_is_digit(c);
}


// The token at pos, which is not blankspace or a comment.
static rc_wgsl_scan rc_wgsl_scan_token(rc_str source, uint32_t pos, rc_arena *arena)
{
    if (rc_wgsl_is_word_start(rc_wgsl_char_at(source, pos))) {
        return rc_wgsl_scan_word(source, pos, arena);
    }

    if (rc_wgsl_is_number_start(source, pos)) {
        return rc_wgsl_scan_number(source, pos);
    }

    return rc_wgsl_scan_punctuation(source, pos);
}

// ---- template lists ----

// A '<' that may open a template list, and the nesting depth of brackets it was found at.
typedef struct rc_wgsl_candidate {
    uint32_t index;
    uint32_t depth;
} rc_wgsl_candidate;

#define RC_ARRAY_TYPE rc_wgsl_candidate
#define RC_ARRAY_NAME wgsl_candidate
#include "richc/array.template.h"

// The state of template list discovery: the candidates still open, innermost last, and the depth
// of brackets.
typedef struct rc_wgsl_discovery {
    rc_array_wgsl_candidate pending;
    uint32_t depth;
} rc_wgsl_discovery;

// The type of token i, or END past the last.
static rc_wgsl_token_type rc_wgsl_type_at(rc_span_wgsl_token tokens, uint32_t i)
{
    return i < tokens.num ? (rc_wgsl_token_type)rc_span_wgsl_token_get(tokens, i).type : RC_WGSL_TOKEN_TYPE_END;
}


// True if token i + 1 starts right where token i ends, with nothing between them.
static bool rc_wgsl_is_adjacent(rc_span_wgsl_token tokens, uint32_t i)
{
    rc_wgsl_token a = rc_span_wgsl_token_get(tokens, i);

    return i + 1 < tokens.num && a.offset + a.len == rc_span_wgsl_token_get(tokens, i + 1).offset;
}


// True if token i + 1 is of type and starts right where token i ends.
static bool rc_wgsl_is_adjacent_type(rc_span_wgsl_token tokens, uint32_t i, rc_wgsl_token_type type)
{
    return rc_wgsl_type_at(tokens, i + 1) == type && rc_wgsl_is_adjacent(tokens, i);
}


// Forget the candidates opened at the current depth or deeper.
static void rc_wgsl_discovery_drop_inner(rc_wgsl_discovery *d)
{
    while (d->pending.num > 0 && rc_array_wgsl_candidate_last_at(&d->pending)->depth >= d->depth) {
        rc_array_wgsl_candidate_pop(&d->pending);
    }
}


// Forget every candidate: the end of an expression.
static void rc_wgsl_discovery_reset(rc_wgsl_discovery *d)
{
    rc_array_wgsl_candidate_reset(&d->pending);
    d->depth = 0;
}


// Handle the '>' at i: it closes the innermost candidate if that is at this depth. Returns how many
// tokens it took, which is 2 for the '>=' operator.
static uint32_t rc_wgsl_discovery_greater(rc_wgsl_discovery *d, rc_span_wgsl_token tokens, uint32_t i)
{
    if (d->pending.num > 0 && rc_array_wgsl_candidate_last_at(&d->pending)->depth == d->depth) {
        rc_wgsl_candidate open = rc_array_wgsl_candidate_pop(&d->pending);

        rc_span_wgsl_token_at(tokens, open.index)->type = RC_WGSL_TOKEN_TYPE_TEMPLATE_START;
        rc_span_wgsl_token_at(tokens, i)->type = RC_WGSL_TOKEN_TYPE_TEMPLATE_END;
        return 1;
    }

    if (rc_wgsl_is_adjacent_type(tokens, i, RC_WGSL_TOKEN_TYPE_EQUAL)) {
        return 2;
    }

    return 1;
}


// True for a token that assigns, whose '=' ends any template lists still open.
static bool rc_wgsl_is_assignment(rc_wgsl_token_type type)
{
    switch (type) {
    case RC_WGSL_TOKEN_TYPE_EQUAL:
    case RC_WGSL_TOKEN_TYPE_AND_EQUAL:
    case RC_WGSL_TOKEN_TYPE_MINUS_EQUAL:
    case RC_WGSL_TOKEN_TYPE_OR_EQUAL:
    case RC_WGSL_TOKEN_TYPE_PERCENT_EQUAL:
    case RC_WGSL_TOKEN_TYPE_PLUS_EQUAL:
    case RC_WGSL_TOKEN_TYPE_SHIFT_LEFT_EQUAL:
    case RC_WGSL_TOKEN_TYPE_SLASH_EQUAL:
    case RC_WGSL_TOKEN_TYPE_STAR_EQUAL:
    case RC_WGSL_TOKEN_TYPE_XOR_EQUAL:
        return true;
    default:
        return false;
    }
}


// True for a word's token: an identifier or a keyword.
static bool rc_wgsl_is_word_token(rc_wgsl_token_type type)
{
    return type == RC_WGSL_TOKEN_TYPE_IDENT || rc_wgsl_token_is_keyword(type);
}


// Handle token i, returning how many tokens it took.
static uint32_t rc_wgsl_discovery_step(rc_wgsl_discovery *d, rc_span_wgsl_token tokens, uint32_t i, rc_arena *arena)
{
    rc_wgsl_token_type type = rc_wgsl_type_at(tokens, i);

    // A word followed by '<' may be a template
    if (rc_wgsl_is_word_token(type) && rc_wgsl_type_at(tokens, i + 1) == RC_WGSL_TOKEN_TYPE_LESS) {
        rc_array_wgsl_candidate_push(&d->pending, (rc_wgsl_candidate) {.index = i + 1, .depth = d->depth}, arena);
        return 2;
    }

    switch (type) {
    case RC_WGSL_TOKEN_TYPE_GREATER:
        return rc_wgsl_discovery_greater(d, tokens, i);
    case RC_WGSL_TOKEN_TYPE_LEFT_PAREN:
    case RC_WGSL_TOKEN_TYPE_LEFT_BRACKET:
        d->depth++;
        return 1;
    case RC_WGSL_TOKEN_TYPE_RIGHT_PAREN:
    case RC_WGSL_TOKEN_TYPE_RIGHT_BRACKET:
        rc_wgsl_discovery_drop_inner(d);
        d->depth = d->depth > 0 ? d->depth - 1 : 0;
        return 1;
    case RC_WGSL_TOKEN_TYPE_AND_AND:
    case RC_WGSL_TOKEN_TYPE_OR_OR:
        rc_wgsl_discovery_drop_inner(d);
        return 1;
    case RC_WGSL_TOKEN_TYPE_SEMICOLON:
    case RC_WGSL_TOKEN_TYPE_LEFT_BRACE:
    case RC_WGSL_TOKEN_TYPE_COLON:
        rc_wgsl_discovery_reset(d);
        return 1;
    default:
        if (rc_wgsl_is_assignment(type)) {
            rc_wgsl_discovery_reset(d);
        }

        return 1;
    }
}


// Mark the '<' and '>' tokens that open and close template lists, by the spec's algorithm.
static void rc_wgsl_discover_templates(rc_span_wgsl_token tokens, rc_arena *arena)
{
    rc_wgsl_discovery d = {0};

    for (uint32_t i = 0; i < tokens.num;) {
        i += rc_wgsl_discovery_step(&d, tokens, i, arena);
    }
    rc_array_wgsl_candidate_release(&d.pending, arena);
}

// ---- joining ----

// A token that a '>' makes with any adjacent '>' and '=' after it, and how many tokens it takes.
typedef struct rc_wgsl_joined {
    rc_wgsl_token token;
    uint32_t taken;
} rc_wgsl_joined;

// The type of a '>' joined with a second '>', an '=', both or neither.
static rc_wgsl_token_type rc_wgsl_joined_type(bool shift, bool equal)
{
    if (shift) {
        return equal ? RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT_EQUAL : RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT;
    }

    return equal ? RC_WGSL_TOKEN_TYPE_GREATER_EQUAL : RC_WGSL_TOKEN_TYPE_GREATER;
}


// The '>' at i, joined with any adjacent '>' and '=' after it.
static rc_wgsl_joined rc_wgsl_join_greater_at(rc_span_wgsl_token tokens, uint32_t i)
{
    bool shift = rc_wgsl_is_adjacent_type(tokens, i, RC_WGSL_TOKEN_TYPE_GREATER);
    uint32_t last = shift ? i + 1 : i;
    bool equal = rc_wgsl_is_adjacent_type(tokens, last, RC_WGSL_TOKEN_TYPE_EQUAL);
    uint32_t taken = last - i + (equal ? 2 : 1);

    return (rc_wgsl_joined) {
        .token = {
            .offset = rc_span_wgsl_token_get(tokens, i).offset,
            .len = taken,
            .type = (uint8_t)rc_wgsl_joined_type(shift, equal)
        },
        .taken = taken
    };
}


// Join every '>' still standing with what follows it, in place, returning the new count.
static uint32_t rc_wgsl_join_greaters(rc_span_wgsl_token tokens)
{
    uint32_t out = 0;

    for (uint32_t i = 0; i < tokens.num;) {
        rc_wgsl_joined joined = {.token = rc_span_wgsl_token_get(tokens, i), .taken = 1};

        if (joined.token.type == RC_WGSL_TOKEN_TYPE_GREATER) {
            joined = rc_wgsl_join_greater_at(tokens, i);
        }

        rc_span_wgsl_token_set(tokens, out++, joined.token);
        i += joined.taken;
    }
    return out;
}

// ---- lexing ----

// Scan source into tokens, ending with END, before template discovery. Returns the first error.
static rc_wgsl_error rc_wgsl_scan_all(rc_str source, rc_array_wgsl_token *tokens, rc_arena *arena)
{
    rc_wgsl_skip skip = rc_wgsl_skip_blank(source, 0);

    while (!rc_str_is_valid(skip.error) && skip.pos != source.len) {
        rc_wgsl_scan scan = rc_wgsl_scan_token(source, skip.pos, arena);

        if (rc_str_is_valid(scan.error)) {
            return rc_wgsl_error_at(source, scan.token.offset, scan.error);
        }

        rc_array_wgsl_token_push(tokens, scan.token, arena);
        skip = rc_wgsl_skip_blank(source, skip.pos + scan.token.len);
    }

    if (rc_str_is_valid(skip.error)) {
        return rc_wgsl_error_at(source, skip.pos, skip.error);
    }

    rc_array_wgsl_token_push(tokens, (rc_wgsl_token) {.offset = source.len, .type = RC_WGSL_TOKEN_TYPE_END}, arena);
    return (rc_wgsl_error) {0};
}


rc_wgsl_lex_result rc_wgsl_lex(rc_str source, rc_arena *arena)
{
    rc_array_wgsl_token tokens = rc_array_wgsl_token_make(source.len / 4 + 16, arena);
    rc_wgsl_error error = rc_wgsl_scan_all(source, &tokens, arena);

    if (rc_wgsl_error_is_set(error)) {
        return (rc_wgsl_lex_result) {.error = error};
    }

    rc_wgsl_discover_templates(tokens.span, arena);
    rc_array_wgsl_token_pop_n(&tokens, tokens.num - rc_wgsl_join_greaters(tokens.span));
    return (rc_wgsl_lex_result) {.tokens = tokens.view};
}


rc_wgsl_token_type rc_wgsl_compound_operator(rc_wgsl_token_type op)
{
    switch (op) {
    case RC_WGSL_TOKEN_TYPE_PLUS_EQUAL:
        return RC_WGSL_TOKEN_TYPE_PLUS;
    case RC_WGSL_TOKEN_TYPE_MINUS_EQUAL:
        return RC_WGSL_TOKEN_TYPE_MINUS;
    case RC_WGSL_TOKEN_TYPE_STAR_EQUAL:
        return RC_WGSL_TOKEN_TYPE_STAR;
    case RC_WGSL_TOKEN_TYPE_SLASH_EQUAL:
        return RC_WGSL_TOKEN_TYPE_SLASH;
    case RC_WGSL_TOKEN_TYPE_PERCENT_EQUAL:
        return RC_WGSL_TOKEN_TYPE_PERCENT;
    case RC_WGSL_TOKEN_TYPE_AND_EQUAL:
        return RC_WGSL_TOKEN_TYPE_AND;
    case RC_WGSL_TOKEN_TYPE_OR_EQUAL:
        return RC_WGSL_TOKEN_TYPE_OR;
    case RC_WGSL_TOKEN_TYPE_XOR_EQUAL:
        return RC_WGSL_TOKEN_TYPE_XOR;
    case RC_WGSL_TOKEN_TYPE_SHIFT_LEFT_EQUAL:
        return RC_WGSL_TOKEN_TYPE_SHIFT_LEFT;
    default:
        return RC_WGSL_TOKEN_TYPE_SHIFT_RIGHT;
    }
}
