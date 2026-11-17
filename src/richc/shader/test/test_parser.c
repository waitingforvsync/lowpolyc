// test_parser.c
//
// Tests for the WGSL parser. Each tree is written out as an S-expression, so a test reads as the
// source next to the shape it should parse to.

#include "parser.h"

#include "richc/mstr.h"
#include "richc/test.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(parser) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(parser, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(parser, fix)
{
    rc_arena_release(&fix->a);
}


// The text being written, with the tree and arena it comes from.
typedef struct dumper {
    rc_wgsl_ast ast;
    rc_mstr out;
    rc_arena *arena;
} dumper;

static void dump(dumper *d, uint32_t node);

// Append text.
static void put(dumper *d, rc_str text)
{
    rc_mstr_append(&d->out, text, d->arena);
}


// Each node of list, each after sep.
static void dump_list(dumper *d, uint32_t list, rc_str sep)
{
    rc_view_wgsl_index items = rc_wgsl_ast_list(d->ast, list);

    for (uint32_t i = 0; i < items.num; i++) {
        put(d, sep);
        dump(d, rc_view_wgsl_index_get(items, i));
    }
}


// "(" head, then each part after a space, then ")". A part of 0 shows as "_".
static void dump_form(dumper *d, rc_str head, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t parts[3] = {a, b, c};

    put(d, RC_STR("("));
    put(d, head);
    for (uint32_t i = 0; i < 3 && parts[i] != RC_INDEX_NONE; i++) {
        put(d, RC_STR(" "));
        dump(d, parts[i]);
    }
    put(d, RC_STR(")"));
}


// A name and its template list, as array<f32, 4>.
static void dump_ident(dumper *d, uint32_t node)
{
    rc_view_wgsl_index args = rc_wgsl_ast_list(d->ast, rc_wgsl_ast_node(d->ast, node).lhs);

    put(d, rc_wgsl_ast_text(d->ast, node));
    if (args.num > 0) {
        put(d, RC_STR("<"));
        for (uint32_t i = 0; i < args.num; i++) {
            put(d, i > 0 ? RC_STR(", ") : RC_STR(""));
            dump(d, rc_view_wgsl_index_get(args, i));
        }
        put(d, RC_STR(">"));
    }
}


// An attribute, as @location(0).
static void dump_attribute(dumper *d, uint32_t node)
{
    rc_view_wgsl_index args = rc_wgsl_ast_list(d->ast, rc_wgsl_ast_node(d->ast, node).lhs);

    put(d, RC_STR("@"));
    put(d, rc_wgsl_ast_text(d->ast, node));
    if (args.num > 0) {
        put(d, RC_STR("("));
        for (uint32_t i = 0; i < args.num; i++) {
            put(d, i > 0 ? RC_STR(", ") : RC_STR(""));
            dump(d, rc_view_wgsl_index_get(args, i));
        }
        put(d, RC_STR(")"));
    }
}


// A declaration's head: its keyword, attributes and name, as "(var @group(0) u".
static void dump_head(dumper *d, rc_str keyword, uint32_t attributes, uint32_t node)
{
    put(d, RC_STR("("));
    put(d, keyword);
    dump_list(d, attributes, RC_STR(" "));
    put(d, RC_STR(" "));
    put(d, rc_wgsl_ast_text(d->ast, node));
}


// A function, as (fn @vertex f ((param x f32)) -> @location(0) T {...}).
static void dump_fn(dumper *d, uint32_t node)
{
    rc_wgsl_fn_parts fn = rc_wgsl_ast_fn_parts(d->ast, node);

    dump_head(d, RC_STR("fn"), fn.attributes, node);
    put(d, RC_STR(" ("));
    dump_list(d, fn.params, RC_STR(""));
    put(d, RC_STR(")"));
    if (fn.return_type != 0) {
        put(d, RC_STR(" ->"));
        dump_list(d, fn.return_attributes, RC_STR(" "));
        put(d, RC_STR(" "));
        dump(d, fn.return_type);
    }

    put(d, RC_STR(" "));
    dump(d, fn.body);
    put(d, RC_STR(")"));
}


// A var, as (var<uniform> @group(0) u T init).
static void dump_var(dumper *d, uint32_t node)
{
    rc_wgsl_var_parts var = rc_wgsl_ast_var_parts(d->ast, node);
    rc_view_wgsl_index templates = rc_wgsl_ast_list(d->ast, var.templates);
    rc_mstr keyword = rc_mstr_from_str(RC_STR("var"), 32, d->arena);

    for (uint32_t i = 0; i < templates.num; i++) {
        rc_mstr_append(&keyword, i == 0 ? RC_STR("<") : RC_STR(", "), d->arena);
        rc_mstr_append(&keyword, rc_wgsl_ast_text(d->ast, rc_view_wgsl_index_get(templates, i)), d->arena);
    }
    if (templates.num > 0) {
        rc_mstr_append(&keyword, RC_STR(">"), d->arena);
    }

    dump_head(d, keyword.view, var.attributes, node);
    put(d, RC_STR(" "));
    dump(d, var.type);
    put(d, RC_STR(" "));
    dump(d, var.initialiser);
    put(d, RC_STR(")"));
}


// A named node with two parts, as (let x T init).
static void dump_named(dumper *d, rc_str keyword, uint32_t attributes, uint32_t node, uint32_t a, uint32_t b)
{
    dump_head(d, keyword, attributes, node);
    put(d, RC_STR(" "));
    dump(d, a);
    if (b != RC_INDEX_NONE) {
        put(d, RC_STR(" "));
        dump(d, b);
    }

    put(d, RC_STR(")"));
}


// A block, as {s1 s2}.
static void dump_block(dumper *d, uint32_t node)
{
    rc_view_wgsl_index statements = rc_wgsl_ast_list(d->ast, rc_wgsl_ast_node(d->ast, node).lhs);

    put(d, RC_STR("{"));
    for (uint32_t i = 0; i < statements.num; i++) {
        put(d, i > 0 ? RC_STR(" ") : RC_STR(""));
        dump(d, rc_view_wgsl_index_get(statements, i));
    }
    put(d, RC_STR("}"));
}


// A list in parentheses, as (a b).
static void dump_paren_list(dumper *d, uint32_t list)
{
    rc_view_wgsl_index items = rc_wgsl_ast_list(d->ast, list);

    put(d, RC_STR("("));
    for (uint32_t i = 0; i < items.num; i++) {
        put(d, i > 0 ? RC_STR(" ") : RC_STR(""));
        dump(d, rc_view_wgsl_index_get(items, i));
    }
    put(d, RC_STR(")"));
}


static void dump(dumper *d, uint32_t node)
{
    rc_wgsl_node n = rc_wgsl_ast_node(d->ast, node);
    rc_str text = rc_wgsl_ast_text(d->ast, node);

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_NONE:
        put(d, RC_STR("_"));
        break;
    case RC_WGSL_NODE_TYPE_IDENT:
        dump_ident(d, node);
        break;
    case RC_WGSL_NODE_TYPE_INT:
    case RC_WGSL_NODE_TYPE_FLOAT:
    case RC_WGSL_NODE_TYPE_BOOL:
    case RC_WGSL_NODE_TYPE_BREAK:
    case RC_WGSL_NODE_TYPE_CONTINUE:
    case RC_WGSL_NODE_TYPE_DISCARD:
    case RC_WGSL_NODE_TYPE_PHONY:
    case RC_WGSL_NODE_TYPE_DEFAULT:
        put(d, text);
        break;
    case RC_WGSL_NODE_TYPE_CALL:
        put(d, RC_STR("(call "));
        dump(d, n.lhs);
        dump_list(d, n.rhs, RC_STR(" "));
        put(d, RC_STR(")"));
        break;
    case RC_WGSL_NODE_TYPE_INDEX:
        dump_form(d, RC_STR("index"), n.lhs, n.rhs, RC_INDEX_NONE);
        break;
    case RC_WGSL_NODE_TYPE_MEMBER:
        put(d, RC_STR("(. "));
        dump(d, n.lhs);
        put(d, RC_STR(" "));
        put(d, text);
        put(d, RC_STR(")"));
        break;
    case RC_WGSL_NODE_TYPE_UNARY:
    case RC_WGSL_NODE_TYPE_INCREMENT:
    case RC_WGSL_NODE_TYPE_DECREMENT:
        dump_form(d, text, n.lhs, RC_INDEX_NONE, RC_INDEX_NONE);
        break;
    case RC_WGSL_NODE_TYPE_BINARY:
    case RC_WGSL_NODE_TYPE_ASSIGN:
        dump_form(d, text, n.lhs, n.rhs, RC_INDEX_NONE);
        break;
    case RC_WGSL_NODE_TYPE_ATTRIBUTE:
        dump_attribute(d, node);
        break;
    case RC_WGSL_NODE_TYPE_STRUCT:
        put(d, RC_STR("(struct "));
        put(d, text);
        dump_list(d, n.lhs, RC_STR(" "));
        put(d, RC_STR(")"));
        break;
    case RC_WGSL_NODE_TYPE_FIELD:
        dump_named(d, RC_STR("field"), n.lhs, node, n.rhs, RC_INDEX_NONE);
        break;
    case RC_WGSL_NODE_TYPE_PARAM:
        dump_named(d, RC_STR("param"), n.lhs, node, n.rhs, RC_INDEX_NONE);
        break;
    case RC_WGSL_NODE_TYPE_FN:
        dump_fn(d, node);
        break;
    case RC_WGSL_NODE_TYPE_VAR:
        dump_var(d, node);
        break;
    case RC_WGSL_NODE_TYPE_LET:
        dump_named(d, RC_STR("let"), 0, node, n.lhs, n.rhs);
        break;
    case RC_WGSL_NODE_TYPE_CONST:
        dump_named(d, RC_STR("const"), 0, node, n.lhs, n.rhs);
        break;
    case RC_WGSL_NODE_TYPE_ALIAS:
        dump_named(d, RC_STR("alias"), 0, node, n.lhs, RC_INDEX_NONE);
        break;
    case RC_WGSL_NODE_TYPE_CONST_ASSERT:
        dump_form(d, RC_STR("const_assert"), n.lhs, RC_INDEX_NONE, RC_INDEX_NONE);
        break;
    case RC_WGSL_NODE_TYPE_BLOCK:
        dump_block(d, node);
        break;
    case RC_WGSL_NODE_TYPE_RETURN:
        dump_form(d, RC_STR("return"), n.lhs, RC_INDEX_NONE, RC_INDEX_NONE);
        break;
    case RC_WGSL_NODE_TYPE_IF: {
        rc_wgsl_if_parts parts = rc_wgsl_ast_if_parts(d->ast, node);

        dump_form(d, RC_STR("if"), n.lhs, parts.then, parts.other);
        break;
    }
    case RC_WGSL_NODE_TYPE_SWITCH:
        put(d, RC_STR("(switch "));
        dump(d, n.lhs);
        dump_list(d, n.rhs, RC_STR(" "));
        put(d, RC_STR(")"));
        break;
    case RC_WGSL_NODE_TYPE_CASE:
        put(d, RC_STR("(case "));
        dump_paren_list(d, n.lhs);
        put(d, RC_STR(" "));
        dump(d, n.rhs);
        put(d, RC_STR(")"));
        break;
    case RC_WGSL_NODE_TYPE_LOOP:
        dump_form(d, RC_STR("loop"), n.lhs, n.rhs, RC_INDEX_NONE);
        break;
    case RC_WGSL_NODE_TYPE_CONTINUING:
        dump_form(d, RC_STR("continuing"), n.lhs, n.rhs, RC_INDEX_NONE);
        break;
    case RC_WGSL_NODE_TYPE_FOR: {
        rc_wgsl_for_parts parts = rc_wgsl_ast_for_parts(d->ast, node);

        put(d, RC_STR("(for "));
        dump_form(d, RC_STR(""), parts.init, parts.condition, parts.update);
        put(d, RC_STR(" "));
        dump(d, n.rhs);
        put(d, RC_STR(")"));
        break;
    }
    case RC_WGSL_NODE_TYPE_WHILE:
        dump_form(d, RC_STR("while"), n.lhs, n.rhs, RC_INDEX_NONE);
        break;
    }
}


// The declarations of source written out one after another, separated by spaces, or the error
// message parsing it gives.
static rc_str parsed(rc_str source, rc_arena *arena)
{
    rc_wgsl_lex_result lexed = rc_wgsl_lex(source, arena);
    rc_wgsl_parse_result result;
    dumper d = {.arena = arena};

    if (rc_wgsl_error_is_set(lexed.error)) {
        return lexed.error.message;
    }

    result = rc_wgsl_parse(source, lexed.tokens, arena);
    if (rc_wgsl_error_is_set(result.error)) {
        return result.error.message;
    }

    d.ast = result.ast;
    d.out = rc_mstr_make(256, arena);
    dump_list(&d, result.ast.decls, RC_STR(" "));
    return rc_str_drop_first(d.out.view, 1);
}


// The body of a function holding statements, written out.
static rc_str statements(const char *statements, rc_arena *arena)
{
    rc_mstr source = rc_mstr_from_str(RC_STR("fn f() {"), 256, arena);
    rc_str tree;

    rc_mstr_append(&source, rc_str_from_cstr(statements), arena);
    rc_mstr_append(&source, RC_STR("}"), arena);
    tree = parsed(source.view, arena);

    // Strip "(fn f () {" and "})" when it parsed
    if (rc_str_starts_with(tree, RC_STR("(fn f () {"))) {
        return rc_str_drop_last(rc_str_drop_first(tree, 10), 2);
    }

    return tree;
}


// The expression in a let, written out.
static rc_str expression(const char *expression, rc_arena *arena)
{
    rc_mstr source = rc_mstr_from_str(RC_STR("let x = "), 256, arena);
    rc_str tree;

    rc_mstr_append(&source, rc_str_from_cstr(expression), arena);
    rc_mstr_append(&source, RC_STR(";"), arena);
    tree = statements(source.data, arena);

    // Strip "(let x _ " and ")" when it parsed
    if (rc_str_starts_with(tree, RC_STR("(let x _ "))) {
        return rc_str_drop_last(rc_str_drop_first(tree, 9), 1);
    }

    return tree;
}

// ---- expressions ----

RC_TEST_STEP(parser, primaries, fix)
{
    RC_CHECK(expression("a", &fix->a), ==, RC_STR("a"));
    RC_CHECK(expression("1u", &fix->a), ==, RC_STR("1u"));
    RC_CHECK(expression("2.5", &fix->a), ==, RC_STR("2.5"));
    RC_CHECK(expression("true", &fix->a), ==, RC_STR("true"));
    RC_CHECK(expression("((a))", &fix->a), ==, RC_STR("a"));
}


RC_TEST_STEP(parser, calls_and_constructors, fix)
{
    RC_CHECK(expression("f()", &fix->a), ==, RC_STR("(call f)"));
    RC_CHECK(expression("f(a, b,)", &fix->a), ==, RC_STR("(call f a b)"));
    RC_CHECK(expression("vec3<f32>(1.0)", &fix->a), ==, RC_STR("(call vec3<f32> 1.0)"));
    RC_CHECK(expression("array<vec2f, 2>(a, b)", &fix->a), ==, RC_STR("(call array<vec2f, 2> a b)"));
    RC_CHECK(expression("bitcast<u32>(x)", &fix->a), ==, RC_STR("(call bitcast<u32> x)"));
}


RC_TEST_STEP(parser, postfix, fix)
{
    RC_CHECK(expression("a.b.c", &fix->a), ==, RC_STR("(. (. a b) c)"));
    RC_CHECK(expression("a[i][j]", &fix->a), ==, RC_STR("(index (index a i) j)"));
    RC_CHECK(expression("f(x).xyz[0]", &fix->a), ==, RC_STR("(index (. (call f x) xyz) 0)"));
}


RC_TEST_STEP(parser, unary, fix)
{
    RC_CHECK(expression("-a", &fix->a), ==, RC_STR("(- a)"));
    RC_CHECK(expression("!~a", &fix->a), ==, RC_STR("(! (~ a))"));
    RC_CHECK(expression("*&a", &fix->a), ==, RC_STR("(* (& a))"));
    RC_CHECK(expression("-a.b", &fix->a), ==, RC_STR("(- (. a b))"));
    RC_CHECK(expression("- -a", &fix->a), ==, RC_STR("(- (- a))"));
}


RC_TEST_STEP(parser, precedence, fix)
{
    RC_CHECK(expression("a + b * c", &fix->a), ==, RC_STR("(+ a (* b c))"));
    RC_CHECK(expression("a * b + c", &fix->a), ==, RC_STR("(+ (* a b) c)"));
    RC_CHECK(expression("a - b - c", &fix->a), ==, RC_STR("(- (- a b) c)"));
    RC_CHECK(expression("a / b % c", &fix->a), ==, RC_STR("(% (/ a b) c)"));
    RC_CHECK(expression("-a * b", &fix->a), ==, RC_STR("(* (- a) b)"));
    RC_CHECK(expression("a + b < c * d", &fix->a), ==, RC_STR("(< (+ a b) (* c d))"));
    RC_CHECK(expression("a << b == c", &fix->a), ==, RC_STR("(== (<< a b) c)"));
    RC_CHECK(expression("a < b && c >= d && e", &fix->a), ==, RC_STR("(&& (&& (< a b) (>= c d)) e)"));
    RC_CHECK(expression("a || b || c", &fix->a), ==, RC_STR("(|| (|| a b) c)"));
    RC_CHECK(expression("a & b & c", &fix->a), ==, RC_STR("(& (& a b) c)"));
    RC_CHECK(expression("a ^ -b", &fix->a), ==, RC_STR("(^ a (- b))"));
    RC_CHECK(expression("(a + b) * c", &fix->a), ==, RC_STR("(* (+ a b) c)"));
}


RC_TEST_STEP(parser, mixes_need_parentheses, fix)
{
    rc_str mix = RC_STR("WGSL needs parentheses to mix these operators");

    RC_CHECK(expression("a & b | c", &fix->a), ==, mix);
    RC_CHECK(expression("a & b + c", &fix->a), ==, mix);
    RC_CHECK(expression("a + b & c", &fix->a), ==, mix);
    RC_CHECK(expression("a + b << c", &fix->a), ==, mix);
    RC_CHECK(expression("a << b << c", &fix->a), ==, mix);
    RC_CHECK(expression("a < b < c", &fix->a), ==, mix);
    RC_CHECK(expression("a && b || c", &fix->a), ==, mix);
    RC_CHECK(expression("a || b && c", &fix->a), ==, mix);
    RC_CHECK(expression("(a & b) | c", &fix->a), ==, RC_STR("(| (& a b) c)"));
}


RC_TEST_STEP(parser, bad_expressions, fix)
{
    RC_CHECK(expression("", &fix->a), ==, RC_STR("expected an expression"));
    RC_CHECK(expression("(a", &fix->a), ==, RC_STR("expected ')'"));
    RC_CHECK(expression("f(a b)", &fix->a), ==, RC_STR("expected ',' or ')'"));
    RC_CHECK(expression("a[0", &fix->a), ==, RC_STR("expected ']'"));
    RC_CHECK(expression("a.(b)", &fix->a), ==, RC_STR("expected a member name after '.'"));

    // As the spec lexes it, .0 is a number, so this is a missing ';'
    RC_CHECK(expression("a.0", &fix->a), ==, RC_STR("expected ';'"));
    RC_CHECK(expression("vec3<f32 1>(1)", &fix->a), ==, RC_STR("expected ',' or '>'"));
}


// The error for nesting or chains too deep to check.
#define TOO_DEEP "this is nested too deeply, or too long a chain"

RC_TEST_STEP(parser, deep_nesting_is_an_error, fix)
{
    rc_mstr deep = rc_mstr_make(512, &fix->a);

    rc_mstr_append_n(&deep, '(', 200, &fix->a);
    rc_mstr_append_char(&deep, 'a', &fix->a);
    rc_mstr_append_n(&deep, ')', 200, &fix->a);
    RC_CHECK(expression(deep.data, &fix->a), ==, RC_STR(TOO_DEEP));

    rc_mstr_reset(&deep);
    rc_mstr_append_n(&deep, '!', 200, &fix->a);
    rc_mstr_append_char(&deep, 'a', &fix->a);
    RC_CHECK(expression(deep.data, &fix->a), ==, RC_STR(TOO_DEEP));
}


RC_TEST_STEP(parser, long_chains_are_an_error, fix)
{
    rc_mstr chain = rc_mstr_from_str(RC_STR("a"), 1024, &fix->a);
    rc_mstr members = rc_mstr_from_str(RC_STR("a"), 1024, &fix->a);

    for (uint32_t i = 0; i < 200; i++) {
        rc_mstr_append(&chain, RC_STR(" + a"), &fix->a);
        rc_mstr_append(&members, i % 2 ? RC_STR(".b") : RC_STR("[0]"), &fix->a);
    }
    RC_CHECK(expression(chain.data, &fix->a), ==, RC_STR(TOO_DEEP));
    RC_CHECK(expression(members.data, &fix->a), ==, RC_STR(TOO_DEEP));
    rc_mstr_append(&members, RC_STR(" = 1;"), &fix->a);
    RC_CHECK(statements(members.data, &fix->a), ==, RC_STR(TOO_DEEP));

    // Short chains, side by side, are fine however many there are
    RC_CHECK(expression("(a + a + a) + (a + a + a) + (a.b.c + a[0][1])", &fix->a), ==,
        RC_STR("(+ (+ (+ (+ a a) a) (+ (+ a a) a)) (+ (. (. a b) c) (index (index a 0) 1)))"));
}

// ---- statements ----

RC_TEST_STEP(parser, declarations_in_functions, fix)
{
    RC_CHECK(statements("var a: f32;", &fix->a), ==, RC_STR("(var a f32 _)"));
    RC_CHECK(statements("var a = 1;", &fix->a), ==, RC_STR("(var a _ 1)"));
    RC_CHECK(statements("var<function> a: i32 = 2;", &fix->a), ==, RC_STR("(var<function> a i32 2)"));
    RC_CHECK(statements("let a: vec2<f32> = b;", &fix->a), ==, RC_STR("(let a vec2<f32> b)"));
    RC_CHECK(statements("const a = 3;", &fix->a), ==, RC_STR("(const a _ 3)"));
    RC_CHECK(statements("let a;", &fix->a), ==, RC_STR("expected '=' and a value"));
}


RC_TEST_STEP(parser, assignments, fix)
{
    RC_CHECK(statements("a = b;", &fix->a), ==, RC_STR("(= a b)"));
    RC_CHECK(statements("a.x += 1;", &fix->a), ==, RC_STR("(+= (. a x) 1)"));
    RC_CHECK(statements("a[i] >>= 2u;", &fix->a), ==, RC_STR("(>>= (index a i) 2u)"));
    RC_CHECK(statements("*p = 0;", &fix->a), ==, RC_STR("(= (* p) 0)"));
    RC_CHECK(statements("(*p).x = 0;", &fix->a), ==, RC_STR("(= (. (* p) x) 0)"));
    RC_CHECK(statements("_ = f();", &fix->a), ==, RC_STR("(= _ (call f))"));
    RC_CHECK(statements("i++; j--;", &fix->a), ==, RC_STR("(++ i) (-- j)"));
    RC_CHECK(statements("a;", &fix->a), ==, RC_STR("expected an assignment, or a call"));
    RC_CHECK(statements("_ += 1;", &fix->a), ==, RC_STR("expected '=' after '_'"));
}


RC_TEST_STEP(parser, calls_as_statements, fix)
{
    RC_CHECK(statements("f();", &fix->a), ==, RC_STR("(call f)"));
    RC_CHECK(statements("f(1, 2);", &fix->a), ==, RC_STR("(call f 1 2)"));
    RC_CHECK(statements("f()", &fix->a), ==, RC_STR("expected ';'"));
}


RC_TEST_STEP(parser, simple_statements, fix)
{
    RC_CHECK(statements("return;", &fix->a), ==, RC_STR("(return _)"));
    RC_CHECK(statements("return a + 1;", &fix->a), ==, RC_STR("(return (+ a 1))"));
    RC_CHECK(statements("discard;", &fix->a), ==, RC_STR("discard"));
    RC_CHECK(statements(";;", &fix->a), ==, RC_STR(""));
    RC_CHECK(statements("{ a = 1; {} }", &fix->a), ==, RC_STR("{(= a 1) {}}"));
    RC_CHECK(statements("const_assert 1 < 2;", &fix->a), ==, RC_STR("(const_assert (< 1 2))"));
}


RC_TEST_STEP(parser, ifs, fix)
{
    RC_CHECK(statements("if a { b = 1; }", &fix->a), ==, RC_STR("(if a {(= b 1)} _)"));
    RC_CHECK(statements("if (a) {} else {}", &fix->a), ==, RC_STR("(if a {} {})"));
    RC_CHECK(statements("if a {} else if b {} else {}", &fix->a), ==, RC_STR("(if a {} (if b {} {}))"));
    RC_CHECK(statements("if a b = 1;", &fix->a), ==, RC_STR("expected '{'"));
}


RC_TEST_STEP(parser, switches, fix)
{
    RC_CHECK(statements("switch x { case 1, 2: {} default: { break; } }", &fix->a), ==,
        RC_STR("(switch x (case (1 2) {}) (case (default) {break}))"));
    RC_CHECK(statements("switch x { case 1, default, { } }", &fix->a), ==, RC_STR("(switch x (case (1 default) {}))"));
    RC_CHECK(statements("switch (x) { default {} }", &fix->a), ==, RC_STR("(switch x (case (default) {}))"));
    RC_CHECK(statements("switch x { a }", &fix->a), ==, RC_STR("expected 'case', 'default' or '}'"));
}


RC_TEST_STEP(parser, loops, fix)
{
    RC_CHECK(statements("loop { break; }", &fix->a), ==, RC_STR("(loop {break} _)"));
    RC_CHECK(statements("loop { a++; continuing { b++; } }", &fix->a), ==, RC_STR("(loop {(++ a)} (continuing {(++ b)} _))"));
    RC_CHECK(statements("loop { continuing { break if a > 3; } }", &fix->a), ==, RC_STR("(loop {} (continuing {} (> a 3)))"));
    RC_CHECK(statements("while a < 3 { a++; }", &fix->a), ==, RC_STR("(while (< a 3) {(++ a)})"));
    RC_CHECK(statements("for (var i = 0; i < 4; i++) {}", &fix->a), ==, RC_STR("(for ( (var i _ 0) (< i 4) (++ i)) {})"));
    RC_CHECK(statements("for (;;) {}", &fix->a), ==, RC_STR("(for ( _ _ _) {})"));
    RC_CHECK(statements("for (i = 0; ; f()) {}", &fix->a), ==, RC_STR("(for ( (= i 0) _ (call f)) {})"));
}


RC_TEST_STEP(parser, loop_mistakes, fix)
{
    RC_CHECK(statements("loop { continuing {} a++; }", &fix->a), ==, RC_STR("the continuing block must come last in a loop"));
    RC_CHECK(statements("continuing {}", &fix->a), ==, RC_STR("a continuing block must come last in a loop"));
    RC_CHECK(statements("loop { continuing { break if a; b++; } }", &fix->a), ==,
        RC_STR("'break if' must be the last statement in a continuing block"));
    RC_CHECK(statements("loop { continuing { break; } }", &fix->a), ==, RC_STR("a continuing block can't break, except by 'break if'"));
    RC_CHECK(statements("loop { break if a; }", &fix->a), ==, RC_STR("'break if' can only end a continuing block"));
    RC_CHECK(statements("for (var i = 0; i < 4; var j = 0) {}", &fix->a), ==, RC_STR("expected an expression"));
}


RC_TEST_STEP(parser, diagnostics_are_dropped, fix)
{
    RC_CHECK(statements("@diagnostic(off, derivative_uniformity) if a {}", &fix->a), ==, RC_STR("(if a {} _)"));
    RC_CHECK(statements("if a @diagnostic(off, x.y) {}", &fix->a), ==, RC_STR("(if a {} _)"));
    RC_CHECK(statements("@diagnostic(off, x) a = 1;", &fix->a), ==, RC_STR("attributes can't go on this statement"));
    RC_CHECK(statements("@must_use {}", &fix->a), ==, RC_STR("only @diagnostic can go on a statement"));
}

// ---- declarations ----

RC_TEST_STEP(parser, structs, fix)
{
    RC_CHECK(parsed(RC_STR("struct S { a: f32, @align(16) b: vec3<f32>, }"), &fix->a), ==,
        RC_STR("(struct S (field a f32) (field @align(16) b vec3<f32>))"));
    RC_CHECK(parsed(RC_STR("struct S { a: f32 }"), &fix->a), ==, RC_STR("(struct S (field a f32))"));
    RC_CHECK(parsed(RC_STR("struct S {}"), &fix->a), ==, RC_STR("a struct needs at least one member"));
    RC_CHECK(parsed(RC_STR("struct S { a: f32; }"), &fix->a), ==, RC_STR("expected ',' or '}'"));
    RC_CHECK(parsed(RC_STR("@align(4) struct S { a: f32 }"), &fix->a), ==, RC_STR("attributes can't go on this declaration"));
}


RC_TEST_STEP(parser, globals, fix)
{
    RC_CHECK(parsed(RC_STR("@group(0) @binding(1) var<uniform> u: U;"), &fix->a), ==, RC_STR("(var<uniform> @group(0) @binding(1) u U _)"));
    RC_CHECK(parsed(RC_STR("var<storage, read_write> s: array<f32>;"), &fix->a), ==, RC_STR("(var<storage, read_write> s array<f32> _)"));
    RC_CHECK(parsed(RC_STR("var<private> p = 1.0;"), &fix->a), ==, RC_STR("(var<private> p _ 1.0)"));
    RC_CHECK(parsed(RC_STR("const PI = 3.14;"), &fix->a), ==, RC_STR("(const PI _ 3.14)"));
    RC_CHECK(parsed(RC_STR("alias v3 = vec3<f32>;"), &fix->a), ==, RC_STR("(alias v3 vec3<f32>)"));
    RC_CHECK(parsed(RC_STR("const_assert N > 0;"), &fix->a), ==, RC_STR("(const_assert (> N 0))"));
    RC_CHECK(parsed(RC_STR(";;const a = 1;;"), &fix->a), ==, RC_STR("(const a _ 1)"));
}


RC_TEST_STEP(parser, functions, fix)
{
    RC_CHECK(parsed(RC_STR("fn f() {}"), &fix->a), ==, RC_STR("(fn f () {})"));
    RC_CHECK(parsed(RC_STR("fn f(a: f32, b: i32,) -> f32 { return a; }"), &fix->a), ==,
        RC_STR("(fn f ((param a f32)(param b i32)) -> f32 {(return a)})"));
    RC_CHECK(parsed(RC_STR("@vertex fn vs(@builtin(vertex_index) i: u32) -> "
        "@builtin(position) vec4f { return p; }"), &fix->a), ==,
        RC_STR("(fn @vertex vs ((param @builtin(vertex_index) i u32)) -> @builtin(position) vec4f "
        "{(return p)})"));
    RC_CHECK(parsed(RC_STR("@compute @workgroup_size(8, 8, 1) fn cs() {}"), &fix->a), ==,
        RC_STR("(fn @compute @workgroup_size(8, 8, 1) cs () {})"));
    RC_CHECK(parsed(RC_STR("fn f(a) {}"), &fix->a), ==, RC_STR("expected ':' and the parameter's type"));
}


RC_TEST_STEP(parser, refused_declarations, fix)
{
    RC_CHECK(parsed(RC_STR("enable f16;"), &fix->a), ==, RC_STR("enable directives aren't supported yet"));
    RC_CHECK(parsed(RC_STR("requires readonly_and_readwrite_storage_textures;"), &fix->a), ==,
        RC_STR("requires directives aren't supported yet"));
    RC_CHECK(parsed(RC_STR("override n: u32;"), &fix->a), ==, RC_STR("override declarations aren't supported yet"));
    RC_CHECK(parsed(RC_STR("let a = 1;"), &fix->a), ==, RC_STR("'let' only goes in functions: use 'const' here"));
    RC_CHECK(parsed(RC_STR("a = 1;"), &fix->a), ==, RC_STR("expected a declaration"));
}


RC_TEST_STEP(parser, diagnostic_directives_are_dropped, fix)
{
    RC_CHECK(parsed(RC_STR("diagnostic(off, derivative_uniformity); const a = 1;"), &fix->a), ==, RC_STR("(const a _ 1)"));
    RC_CHECK(parsed(RC_STR("diagnostic(off, x) const a = 1;"), &fix->a), ==, RC_STR("expected ';'"));
}


RC_TEST_STEP(parser, error_positions, fix)
{
    rc_str source = RC_STR("fn f() {\n    let a = ;\n}");
    rc_wgsl_lex_result lexed = rc_wgsl_lex(source, &fix->a);
    rc_wgsl_parse_result result = rc_wgsl_parse(source, lexed.tokens, &fix->a);

    RC_CHECK(result.error.message, ==, RC_STR("expected an expression"));
    RC_CHECK(result.error.line, ==, 2u);
    RC_CHECK(result.error.column, ==, 13u);
    RC_CHECK(result.ast.nodes.num, ==, 0u);
}


RC_TEST_STEP(parser, a_whole_shader, fix)
{
    rc_str source = RC_STR(
        "struct Out {\n"
        "    @builtin(position) position: vec4f,\n"
        "    @location(0) uv: vec2f,\n"
        "}\n"
        "@group(0) @binding(0) var<uniform> mvp: mat4x4f;\n"
        "@vertex fn vs(@location(0) p: vec3f, @location(1) uv: vec2f) -> Out {\n"
        "    return Out(mvp * vec4f(p, 1.0), uv);\n"
        "}\n"
        "@group(0) @binding(1) var t: texture_2d<f32>;\n"
        "@group(0) @binding(2) var s: sampler;\n"
        "@fragment fn fs(in: Out) -> @location(0) vec4f {\n"
        "    return textureSample(t, s, in.uv);\n"
        "}\n");

    RC_CHECK(parsed(source, &fix->a), ==, RC_STR(
        "(struct Out (field @builtin(position) position vec4f) (field @location(0) uv vec2f)) "
        "(var<uniform> @group(0) @binding(0) mvp mat4x4f _) "
        "(fn @vertex vs ((param @location(0) p vec3f)(param @location(1) uv vec2f)) -> Out "
        "{(return (call Out (* mvp (call vec4f p 1.0)) uv))}) "
        "(var @group(0) @binding(1) t texture_2d<f32> _) "
        "(var @group(0) @binding(2) s sampler _) "
        "(fn @fragment fs ((param in Out)) -> @location(0) vec4f "
        "{(return (call textureSample t s (. in uv)))})"));
}
