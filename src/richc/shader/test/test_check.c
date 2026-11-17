// test_check.c
//
// Tests for the WGSL checker. Most tests check one expression or a few statements against a
// prelude of declarations, and read back either a type's name or the error.

#include "check.h"
#include "parser.h"

#include "richc/mstr.h"
#include "richc/test.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(check) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(check, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(check, fix)
{
    rc_arena_release(&fix->a);
}


// Declarations every test can use.
static const char prelude[] = "struct S { a: f32, b: i32 }\n"
    "var<private> f: f32;\n"
    "var<private> i: i32;\n"
    "var<private> u: u32;\n"
    "var<private> b: bool;\n"
    "var<private> v2: vec2f;\n"
    "var<private> v3: vec3f;\n"
    "var<private> v4: vec4f;\n"
    "var<private> iv3: vec3i;\n"
    "var<private> m: mat4x4f;\n"
    "var<private> arr: array<f32, 4>;\n"
    "@group(0) @binding(0) var t: texture_2d<f32>;\n"
    "@group(0) @binding(1) var s: sampler;\n"
    "@group(0) @binding(2) var d: texture_depth_2d;\n"
    "@group(0) @binding(3) var cs: sampler_comparison;\n"
    "@group(0) @binding(4) var<uniform> uni: S;\n";

// The result of checking the prelude followed by text.
static rc_wgsl_check_result check(const char *text, rc_arena *arena)
{
    rc_mstr source = rc_mstr_from_str(rc_str_from_cstr(prelude), 1024, arena);
    rc_wgsl_lex_result lexed;
    rc_wgsl_parse_result parsed;

    rc_mstr_append(&source, rc_str_from_cstr(text), arena);
    lexed = rc_wgsl_lex(source.view, arena);
    if (rc_wgsl_error_is_set(lexed.error)) {
        return (rc_wgsl_check_result) {.error = lexed.error};
    }

    parsed = rc_wgsl_parse(source.view, lexed.tokens, arena);
    if (rc_wgsl_error_is_set(parsed.error)) {
        return (rc_wgsl_check_result) {.error = parsed.error};
    }

    return rc_wgsl_check(parsed.ast, arena);
}


// The error checking the prelude and text gives, or "" if it is fine.
static rc_str errors(const char *text, rc_arena *arena)
{
    rc_wgsl_check_result result = check(text, arena);

    return rc_wgsl_error_is_set(result.error) ? result.error.message : RC_STR("");
}


// The error checking statements in a function body gives, or "" if they are fine.
static rc_str body_errors(const char *statements, rc_arena *arena)
{
    rc_mstr text = rc_mstr_from_str(RC_STR("fn test_body() {\n"), 256, arena);

    rc_mstr_append(&text, rc_str_from_cstr(statements), arena);
    rc_mstr_append(&text, RC_STR("\n}\n"), arena);
    return errors(text.data, arena);
}


// The last declaration of a checked module.
static uint32_t last_decl(rc_wgsl_module module)
{
    rc_view_wgsl_index decls = rc_wgsl_ast_list(module.ast, module.ast.decls);

    return rc_view_wgsl_index_get(decls, decls.num - 1);
}


// The name of the type of the declaration decl, or of the error.
static rc_str type_name(rc_wgsl_check_result result, uint32_t decl, rc_arena *arena)
{
    rc_mstr name = {0};

    if (rc_wgsl_error_is_set(result.error)) {
        return result.error.message;
    }

    rc_wgsl_type_append_name(&name, result.module.types, rc_wgsl_module_sema(result.module, decl).type, result.module.ast, arena);
    return name.view;
}


// The type of x in fn test_let() { let x = expression; }.
static rc_str let_type(const char *expression, rc_arena *arena)
{
    rc_mstr text = rc_mstr_from_str(RC_STR("fn test_let() { let x = "), 256, arena);
    rc_wgsl_check_result result;
    uint32_t body;

    rc_mstr_append(&text, rc_str_from_cstr(expression), arena);
    rc_mstr_append(&text, RC_STR("; }\n"), arena);
    result = check(text.data, arena);
    if (rc_wgsl_error_is_set(result.error)) {
        return result.error.message;
    }

    body = rc_wgsl_ast_fn_parts(result.module.ast, last_decl(result.module)).body;
    return type_name(result, rc_view_wgsl_index_get(rc_wgsl_ast_list(result.module.ast,
        rc_wgsl_ast_node(result.module.ast, body).lhs), 0), arena);
}


// The type of x in const x = expression; at module scope.
static rc_str const_type(const char *expression, rc_arena *arena)
{
    rc_mstr text = rc_mstr_from_str(RC_STR("const x = "), 256, arena);
    rc_wgsl_check_result result;

    rc_mstr_append(&text, rc_str_from_cstr(expression), arena);
    rc_mstr_append(&text, RC_STR(";\n"), arena);
    result = check(text.data, arena);
    return type_name(result, rc_wgsl_error_is_set(result.error) ? 0 : last_decl(result.module), arena);
}


// The value of the integer constant expression, or -999 if it has none.
static int64_t const_value(const char *expression, rc_arena *arena)
{
    rc_mstr text = rc_mstr_from_str(RC_STR("const x = "), 256, arena);
    rc_wgsl_check_result result;
    rc_wgsl_sema sema;

    rc_mstr_append(&text, rc_str_from_cstr(expression), arena);
    rc_mstr_append(&text, RC_STR(";\n"), arena);
    result = check(text.data, arena);
    if (rc_wgsl_error_is_set(result.error)) {
        return -999;
    }

    sema = rc_wgsl_module_sema(result.module, last_decl(result.module));
    return (sema.flags & RC_WGSL_SEMA_VALUE) ? sema.value : -999;
}

// ---- literals and names ----

RC_TEST_STEP(check, the_prelude_is_fine, fix)
{
    RC_CHECK(errors("", &fix->a), ==, RC_STR(""));
}


RC_TEST_STEP(check, literals_settle_on_defaults, fix)
{
    RC_CHECK(let_type("1", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("1u", &fix->a), ==, RC_STR("u32"));
    RC_CHECK(let_type("1i", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("1.0", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("1f", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("0x1p4", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("true", &fix->a), ==, RC_STR("bool"));
}


RC_TEST_STEP(check, consts_stay_abstract, fix)
{
    RC_CHECK(const_type("1", &fix->a), ==, RC_STR("an integer"));
    RC_CHECK(const_type("1.0", &fix->a), ==, RC_STR("a number"));
    RC_CHECK(const_type("1 + 2.0", &fix->a), ==, RC_STR("a number"));
    RC_CHECK(const_type("vec3(1, 2, 3)", &fix->a), ==, RC_STR("vec3<an integer>"));
    RC_CHECK(const_type("1u", &fix->a), ==, RC_STR("u32"));
}


RC_TEST_STEP(check, literal_ranges, fix)
{
    rc_str too_big = RC_STR("this integer is too big for its type");

    RC_CHECK(let_type("2147483647i", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("2147483648i", &fix->a), ==, too_big);
    RC_CHECK(let_type("4294967295u", &fix->a), ==, RC_STR("u32"));
    RC_CHECK(let_type("4294967296u", &fix->a), ==, too_big);
    RC_CHECK(let_type("99999999999999999999", &fix->a), ==, too_big);
    RC_CHECK(let_type("1h", &fix->a), ==, RC_STR("f16 isn't supported yet"));

    // An abstract integer can hold more, until it settles
    RC_CHECK(const_type("4294967296", &fix->a), ==, RC_STR("an integer"));
    RC_CHECK(let_type("4294967296", &fix->a), ==, RC_STR("this is out of range for i32"));
}


RC_TEST_STEP(check, names, fix)
{
    RC_CHECK(let_type("q", &fix->a), ==, RC_STR("there is nothing called 'q'"));
    RC_CHECK(let_type("f32", &fix->a), ==, RC_STR("'f32' is a type, not a value"));
    RC_CHECK(let_type("sin", &fix->a), ==, RC_STR("'sin' is a function: call it with ( and )"));
    RC_CHECK(let_type("uniform", &fix->a), ==, RC_STR("'uniform' only goes in a template list"));
    RC_CHECK(let_type("S", &fix->a), ==, RC_STR("'S' is a type, not a value"));
    RC_CHECK(let_type("vec3<f32>", &fix->a), ==, RC_STR("a type can't be used as a value"));
    RC_CHECK(let_type("f", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("uni", &fix->a), ==, RC_STR("S"));
}


RC_TEST_STEP(check, declarations_in_any_order, fix)
{
    RC_CHECK(errors("fn g() -> T { return T(N); }\nstruct T { x: i32 }\nconst N = 3;\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("const A = B;\nconst B = A;\n", &fix->a), ==, RC_STR("'A' depends on itself"));
    RC_CHECK(errors("struct R { r: R }\n", &fix->a), ==, RC_STR("'R' depends on itself"));
    RC_CHECK(errors("const f = 1;\n", &fix->a), ==, RC_STR("'f' is already declared"));
}


RC_TEST_STEP(check, scopes, fix)
{
    RC_CHECK(body_errors("let a = 1; { let a = 2.0; }", &fix->a), ==, RC_STR(""));
    RC_CHECK(body_errors("let a = 1; let a = 2;", &fix->a), ==, RC_STR("'a' is already declared here"));
    RC_CHECK(body_errors("{ let a = 1; } let b = a;", &fix->a), ==, RC_STR("there is nothing called 'a'"));
    RC_CHECK(body_errors("let f = 1; let g: i32 = f;", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("fn h(p: i32) { let p = 2; }\n", &fix->a), ==, RC_STR("'p' is already declared here"));
    RC_CHECK(errors("fn h(p: i32, p: f32) {}\n", &fix->a), ==, RC_STR("'p' is already a parameter"));
}

// ---- operators ----

RC_TEST_STEP(check, arithmetic, fix)
{
    RC_CHECK(let_type("f + 1", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("1 + f", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("i * 2", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("u % 3", &fix->a), ==, RC_STR("u32"));
    RC_CHECK(let_type("f % 2.5", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("v3 * 2.0", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("2 * v3", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("v3 - v3", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("-iv3", &fix->a), ==, RC_STR("vec3<i32>"));
    RC_CHECK(let_type("i + u", &fix->a), ==, RC_STR("can't use '+' on i32 and u32"));
    RC_CHECK(let_type("v3 + v2", &fix->a), ==, RC_STR("can't use '+' on vec3<f32> and vec2<f32>"));
    RC_CHECK(let_type("b + b", &fix->a), ==, RC_STR("can't use '+' on bool and bool"));
    RC_CHECK(let_type("-u", &fix->a), ==, RC_STR("can't negate u32"));
}


RC_TEST_STEP(check, matrices, fix)
{
    RC_CHECK(let_type("m * v4", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("v4 * m", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("m * m", &fix->a), ==, RC_STR("mat4x4<f32>"));
    RC_CHECK(let_type("m * 2", &fix->a), ==, RC_STR("mat4x4<f32>"));
    RC_CHECK(let_type("m + m", &fix->a), ==, RC_STR("mat4x4<f32>"));
    RC_CHECK(let_type("mat2x3f() * mat4x2f()", &fix->a), ==, RC_STR("mat4x3<f32>"));
    RC_CHECK(let_type("m * v3", &fix->a), ==, RC_STR("can't use '*' on mat4x4<f32> and vec3<f32>"));
    RC_CHECK(let_type("m / m", &fix->a), ==, RC_STR("can't use '/' on mat4x4<f32> and mat4x4<f32>"));
}


RC_TEST_STEP(check, comparisons_and_logic, fix)
{
    RC_CHECK(let_type("f < 1", &fix->a), ==, RC_STR("bool"));
    RC_CHECK(let_type("1 < 2", &fix->a), ==, RC_STR("bool"));
    RC_CHECK(let_type("v3 == v3", &fix->a), ==, RC_STR("vec3<bool>"));
    RC_CHECK(let_type("b == true", &fix->a), ==, RC_STR("bool"));
    RC_CHECK(let_type("b && f > 0", &fix->a), ==, RC_STR("bool"));
    RC_CHECK(let_type("!b", &fix->a), ==, RC_STR("bool"));
    RC_CHECK(let_type("b < b", &fix->a), ==, RC_STR("can't use '<' on bool and bool"));
    RC_CHECK(let_type("i < u", &fix->a), ==, RC_STR("can't use '<' on i32 and u32"));
    RC_CHECK(let_type("i && b", &fix->a), ==, RC_STR("can't use '&&' on i32 and bool"));
}


RC_TEST_STEP(check, bits_and_shifts, fix)
{
    RC_CHECK(let_type("i & 3", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("~u", &fix->a), ==, RC_STR("u32"));
    RC_CHECK(let_type("b | b", &fix->a), ==, RC_STR("bool"));
    RC_CHECK(let_type("i << 2u", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("i << 2", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("1 << 3", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("iv3 >> vec3u(1)", &fix->a), ==, RC_STR("vec3<i32>"));
    RC_CHECK(let_type("i << i", &fix->a), ==, RC_STR("can't use '<<' on i32 and i32"));
    RC_CHECK(let_type("b ^ b", &fix->a), ==, RC_STR("can't use '^' on bool and bool"));
    RC_CHECK(let_type("f & 1", &fix->a), ==, RC_STR("can't use '&' on f32 and an integer"));
}


RC_TEST_STEP(check, constant_values, fix)
{
    RC_CHECK(const_value("1 + 2 * 3", &fix->a), ==, 7);
    RC_CHECK(const_value("-7 / 2", &fix->a), ==, -3);
    RC_CHECK(const_value("-7 % 2", &fix->a), ==, -1);
    RC_CHECK(const_value("1 << 40", &fix->a), ==, 1ll << 40);
    RC_CHECK(const_value("0xffffffffu & ~1u", &fix->a), ==, 0xfffffffe);
    RC_CHECK(const_value("~0u", &fix->a), ==, 0xffffffff);
    RC_CHECK(const_value("i32(4294967295u)", &fix->a), ==, -1);
    RC_CHECK(const_value("u32(-1i)", &fix->a), ==, 0xffffffff);
    RC_CHECK(const_value("3 > 2", &fix->a), ==, 1);
    RC_CHECK(const_value("1.5", &fix->a), ==, -999);
}


RC_TEST_STEP(check, constant_errors, fix)
{
    RC_CHECK(const_type("2147483647i + 1i", &fix->a), ==, RC_STR("this overflows"));
    RC_CHECK(const_type("0u - 1u", &fix->a), ==, RC_STR("this overflows"));
    RC_CHECK(const_type("1 / 0", &fix->a), ==, RC_STR("this divides by zero"));
    RC_CHECK(const_type("1i << 32u", &fix->a), ==, RC_STR("this overflows"));
    RC_CHECK(const_type("0x7fffffffffffffff + 1", &fix->a), ==, RC_STR("this overflows"));
    RC_CHECK(const_type("-(-2147483647i - 1i)", &fix->a), ==, RC_STR("this overflows"));
    RC_CHECK(const_type("2147483647 + 1", &fix->a), ==, RC_STR("an integer"));

    // Worked out whole, then converted, so the parts needn't fit an i32
    RC_CHECK(let_type("4000000000 / 4", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("4000000000 / 1", &fix->a), ==, RC_STR("this is out of range for i32"));
    RC_CHECK(const_type("f", &fix->a), ==, RC_STR("a const needs a value known before the shader runs"));
}

// ---- indexing and members ----

RC_TEST_STEP(check, indexing, fix)
{
    RC_CHECK(let_type("v3[0]", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("m[1]", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("arr[i]", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("arr[u]", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("arr[4]", &fix->a), ==, RC_STR("this index is out of bounds"));
    RC_CHECK(let_type("arr[-1]", &fix->a), ==, RC_STR("this index is out of bounds"));
    RC_CHECK(let_type("arr[f]", &fix->a), ==, RC_STR("an index must be an integer, not f32"));
    RC_CHECK(let_type("f[0]", &fix->a), ==, RC_STR("can't index f32"));
}


RC_TEST_STEP(check, swizzles_and_members, fix)
{
    RC_CHECK(let_type("v3.x", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("v3.zyx", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("v4.rgba", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("v2.xyxy", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("v3.w", &fix->a), ==, RC_STR("this isn't a swizzle of vec3<f32>"));
    RC_CHECK(let_type("v3.xg", &fix->a), ==, RC_STR("this isn't a swizzle of vec3<f32>"));
    RC_CHECK(let_type("v4.xyzwx", &fix->a), ==, RC_STR("a swizzle has at most four letters"));
    RC_CHECK(let_type("uni.a", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("uni.c", &fix->a), ==, RC_STR("S has no member called 'c'"));
    RC_CHECK(let_type("f.x", &fix->a), ==, RC_STR("f32 has no members"));
}

// ---- constructors ----

RC_TEST_STEP(check, vector_constructors, fix)
{
    RC_CHECK(let_type("vec3f(1, 2, 3)", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("vec3(1, 2, 3)", &fix->a), ==, RC_STR("vec3<i32>"));
    RC_CHECK(let_type("vec3(1.0, 2, 3)", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("vec2(f, 1)", &fix->a), ==, RC_STR("vec2<f32>"));
    RC_CHECK(let_type("vec4(v3, 1)", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("vec4(v2, v2)", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("vec3<f32>(0.5)", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("vec3f(iv3)", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("vec3f()", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("vec3f(1, 2)", &fix->a), ==, RC_STR("these don't add up to the vector's size"));
    RC_CHECK(let_type("vec2(i, u)", &fix->a), ==, RC_STR("the arguments to 'vec2' have no type in common"));
    RC_CHECK(let_type("vec3()", &fix->a), ==, RC_STR("'vec3' needs its element type when it has no arguments"));
}


RC_TEST_STEP(check, other_constructors, fix)
{
    RC_CHECK(let_type("array(1, 2, 3)", &fix->a), ==, RC_STR("array<i32, 3>"));
    RC_CHECK(let_type("array(1, 2.5)", &fix->a), ==, RC_STR("array<f32, 2>"));
    RC_CHECK(let_type("array<f32, 2>(1, 2)", &fix->a), ==, RC_STR("array<f32, 2>"));
    RC_CHECK(let_type("array(vec2(0.0, 0.5), vec2(1.0, 1.0))", &fix->a), ==, RC_STR("array<vec2<f32>, 2>"));
    RC_CHECK(let_type("mat2x2(1, 2, 3, 4)", &fix->a), ==, RC_STR("mat2x2<f32>"));
    RC_CHECK(let_type("mat2x2f(v2, v2)", &fix->a), ==, RC_STR("mat2x2<f32>"));
    RC_CHECK(let_type("S(1.0, 2)", &fix->a), ==, RC_STR("S"));
    RC_CHECK(let_type("S()", &fix->a), ==, RC_STR("S"));
    RC_CHECK(let_type("f32(i)", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("i32(1.5)", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("bool(1)", &fix->a), ==, RC_STR("bool"));
    RC_CHECK(let_type("u32(-1)", &fix->a), ==, RC_STR("this is out of range for u32"));
    RC_CHECK(let_type("array<f32, 2>(1)", &fix->a), ==, RC_STR("'array' takes 2 arguments, not 1"));
    RC_CHECK(let_type("S(1.0)", &fix->a), ==, RC_STR("'S' takes 2 arguments, not 1"));
    RC_CHECK(let_type("S(1.0, 2.5)", &fix->a), ==, RC_STR("expected i32, got a number"));
    RC_CHECK(let_type("mat2x2f(1, 2, 3)", &fix->a), ==, RC_STR("mat2x2<f32> takes its columns, or all its numbers"));
}

// ---- built-in functions ----

RC_TEST_STEP(check, builtin_types, fix)
{
    RC_CHECK(let_type("max(f, 1)", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("max(1, 2)", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("abs(i)", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("sin(1)", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("clamp(v3, vec3f(0), vec3f(1))", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("dot(v3, v3)", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("dot(iv3, iv3)", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("length(v3)", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("distance(f, 2.0)", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("cross(v3, v3)", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("normalize(v2)", &fix->a), ==, RC_STR("vec2<f32>"));
    RC_CHECK(let_type("mix(v3, v3, 0.5)", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("mix(f, 1, 0.5)", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("refract(v3, v3, 1)", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("select(1, 2, b)", &fix->a), ==, RC_STR("i32"));
    RC_CHECK(let_type("select(v3, v3, v3 > v3)", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("transpose(mat2x3f())", &fix->a), ==, RC_STR("mat3x2<f32>"));
    RC_CHECK(let_type("determinant(m)", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("all(v3 > v3)", &fix->a), ==, RC_STR("bool"));
    RC_CHECK(let_type("bitcast<u32>(f)", &fix->a), ==, RC_STR("u32"));
    RC_CHECK(let_type("bitcast<vec3f>(iv3)", &fix->a), ==, RC_STR("vec3<f32>"));
    RC_CHECK(let_type("pack2x16float(v2)", &fix->a), ==, RC_STR("u32"));
    RC_CHECK(let_type("unpack2x16unorm(u)", &fix->a), ==, RC_STR("vec2<f32>"));
}


RC_TEST_STEP(check, builtin_mismatches, fix)
{
    RC_CHECK(let_type("sin(b)", &fix->a), ==, RC_STR("'sin' can't take (bool)"));
    RC_CHECK(let_type("sin(i)", &fix->a), ==, RC_STR("'sin' can't take (i32)"));
    RC_CHECK(let_type("max(i, u)", &fix->a), ==, RC_STR("'max' can't take (i32, u32)"));
    RC_CHECK(let_type("dot(f, f)", &fix->a), ==, RC_STR("'dot' can't take (f32, f32)"));
    RC_CHECK(let_type("cross(v2, v2)", &fix->a), ==, RC_STR("'cross' can't take (vec2<f32>, vec2<f32>)"));
    RC_CHECK(let_type("sign(u)", &fix->a), ==, RC_STR("'sign' can't take (u32)"));
    RC_CHECK(let_type("max(f)", &fix->a), ==, RC_STR("'max' can't take (f32)"));
    RC_CHECK(let_type("bitcast<u32>(v2)", &fix->a), ==, RC_STR("'bitcast' can't take (vec2<f32>)"));
    RC_CHECK(let_type("bitcast(f)", &fix->a), ==, RC_STR("bitcast needs its type, as bitcast<u32>(x)"));
    RC_CHECK(let_type("atomicAdd(i, 1)", &fix->a), ==, RC_STR("'atomicAdd' isn't supported yet"));
    RC_CHECK(let_type("nope(1)", &fix->a), ==, RC_STR("there is no function called 'nope'"));
}


RC_TEST_STEP(check, textures, fix)
{
    RC_CHECK(let_type("textureSample(t, s, vec2(0.5, 0.5))", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("textureSample(t, s, v2, vec2i(1, 1))", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("textureSampleLevel(t, s, v2, 0.0)", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("textureSampleGrad(t, s, v2, v2, v2)", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("textureSample(d, s, v2)", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("textureSampleCompare(d, cs, v2, 0.5)", &fix->a), ==, RC_STR("f32"));
    RC_CHECK(let_type("textureLoad(t, vec2i(0), 0)", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("textureLoad(t, vec2u(0), 0u)", &fix->a), ==, RC_STR("vec4<f32>"));
    RC_CHECK(let_type("textureDimensions(t)", &fix->a), ==, RC_STR("vec2<u32>"));
    RC_CHECK(let_type("textureDimensions(t, 1)", &fix->a), ==, RC_STR("vec2<u32>"));
    RC_CHECK(let_type("textureNumLevels(t)", &fix->a), ==, RC_STR("u32"));
}


RC_TEST_STEP(check, texture_mistakes, fix)
{
    RC_CHECK(let_type("textureSample(t, cs, v2)", &fix->a), ==, RC_STR("expected sampler, got sampler_comparison"));
    RC_CHECK(let_type("textureSample(s, s, v2)", &fix->a), ==, RC_STR("this can't take sampler"));
    RC_CHECK(let_type("textureSample(t, s, v3)", &fix->a), ==, RC_STR("expected vec2<f32>, got vec3<f32>"));
    RC_CHECK(let_type("textureSample(t, s)", &fix->a), ==, RC_STR("this needs more arguments"));
    RC_CHECK(let_type("textureSample(t, s, v2, vec2i(1), 3)", &fix->a), ==, RC_STR("this is one argument too many"));
    RC_CHECK(let_type("textureSample(t, s, v2, vec2(i, i))", &fix->a), ==, RC_STR("an offset must be known before the shader runs"));
    RC_CHECK(let_type("textureSampleCompare(t, cs, v2, 0.5)", &fix->a), ==, RC_STR("this can't take texture_2d<f32>"));
    RC_CHECK(let_type("textureLoad(t, v2, 0)", &fix->a), ==, RC_STR("expected i32 or u32 here, not vec2<f32>"));
    RC_CHECK(let_type("textureNumLayers(t)", &fix->a), ==, RC_STR("only an arrayed texture has layers"));
}

// ---- statements ----

RC_TEST_STEP(check, assignments, fix)
{
    RC_CHECK(body_errors("var x = 1.0; x = 2; x += 1; x *= f;", &fix->a), ==, RC_STR(""));
    RC_CHECK(body_errors("f = 1; v3.x = 2; v3 = vec3(1, 2, 3); arr[1] = f;", &fix->a), ==, RC_STR(""));
    RC_CHECK(body_errors("var x = 1; x <<= 2; x++; x--;", &fix->a), ==, RC_STR(""));
    RC_CHECK(body_errors("let x = 1; x = 2;", &fix->a), ==, RC_STR("'x' can't change: make it a var"));
    RC_CHECK(body_errors("uni.a = 1.0;", &fix->a), ==, RC_STR("this is read-only"));
    RC_CHECK(body_errors("v3.xy = v2;", &fix->a), ==, RC_STR("can only change a var, or part of one"));
    RC_CHECK(body_errors("f = i;", &fix->a), ==, RC_STR("expected f32, got i32"));
    RC_CHECK(body_errors("i += 1u;", &fix->a), ==, RC_STR("can't apply this to i32 with u32"));
    RC_CHECK(body_errors("f++;", &fix->a), ==, RC_STR("only an i32 or u32 counts up or down, not f32"));
    RC_CHECK(body_errors("_ = f; _ = sin(1.0);", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("fn h(p: i32) { p = 2; }\n", &fix->a), ==, RC_STR("'p' is a parameter, which can't change"));
}


RC_TEST_STEP(check, calls_as_statements, fix)
{
    RC_CHECK(errors("fn g() -> i32 { return 1; }\nfn h() { g(); }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(body_errors("sin(1.0);", &fix->a), ==, RC_STR("the result of 'sin' must be used"));
    RC_CHECK(body_errors("vec3f(1.0);", &fix->a), ==, RC_STR("a value made here must be used"));
    RC_CHECK(errors("fn g() {}\nfn h() { let x = g(); }\n", &fix->a), ==, RC_STR("this gives no value"));
}


RC_TEST_STEP(check, returns, fix)
{
    RC_CHECK(errors("fn g() -> f32 { return 1; }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("fn g() -> f32 { if b { return 1.0; } else { return 2.0; } }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("fn g() -> f32 { loop { } }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("fn g() -> f32 { if b { return 1.0; } }\n", &fix->a), ==, RC_STR("this function can end without returning a value"));
    RC_CHECK(errors("fn g() -> f32 { loop { break; } }\n", &fix->a), ==, RC_STR("this function can end without returning a value"));
    RC_CHECK(errors("fn g() -> f32 { for (;;) { } }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("fn g() -> f32 { while true { return 1.0; } }\n", &fix->a), ==,
        RC_STR("this function can end without returning a value"));
    RC_CHECK(errors("fn g() -> f32 { switch i { default: { return 1.0; } } }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("fn g() -> f32 { return; }\n", &fix->a), ==, RC_STR("this function must return f32"));
    RC_CHECK(errors("fn g() { return 1; }\n", &fix->a), ==, RC_STR("this function returns nothing"));
    RC_CHECK(errors("fn g() -> f32 { return b; }\n", &fix->a), ==, RC_STR("expected f32, got bool"));
}


RC_TEST_STEP(check, control_flow, fix)
{
    RC_CHECK(body_errors("if f > 0 { } else if b { } else { }", &fix->a), ==, RC_STR(""));
    RC_CHECK(body_errors("for (var j = 0; j < 4; j++) { if j == 2 { continue; } break; }", &fix->a), ==, RC_STR(""));
    RC_CHECK(body_errors("loop { let k = 1; continuing { let l = k; break if l > 0; } }", &fix->a), ==, RC_STR(""));
    RC_CHECK(body_errors("if i { }", &fix->a), ==, RC_STR("expected bool, got i32"));
    RC_CHECK(body_errors("while 1 { }", &fix->a), ==, RC_STR("expected bool, got an integer"));
    RC_CHECK(body_errors("break;", &fix->a), ==, RC_STR("a break must be in a loop or switch"));
    RC_CHECK(body_errors("continue;", &fix->a), ==, RC_STR("a continue must be in a loop"));
    RC_CHECK(body_errors("loop { continuing { if b { break; } } }", &fix->a), ==,
        RC_STR("a continuing block leaves its loop by 'break if'"));
    RC_CHECK(body_errors("loop { continuing { continue; } }", &fix->a), ==, RC_STR("a continuing block can't continue"));
    RC_CHECK(body_errors("loop { continuing { return; } }", &fix->a), ==, RC_STR("a continuing block can't return"));
    RC_CHECK(body_errors("loop { continuing { loop { break; } break if b; } }", &fix->a), ==, RC_STR(""));
}


RC_TEST_STEP(check, switches, fix)
{
    RC_CHECK(body_errors("switch i { case 1, 2: { } case 3: { break; } default: { } }", &fix->a), ==, RC_STR(""));
    RC_CHECK(body_errors("switch u { case 1u: { } default: { } }", &fix->a), ==, RC_STR(""));
    RC_CHECK(body_errors("switch 2 { case 1u: { } default: { } }", &fix->a), ==, RC_STR(""));
    RC_CHECK(body_errors("switch i { case 1, 1: { } default: { } }", &fix->a), ==, RC_STR("an earlier case has this value"));
    RC_CHECK(body_errors("switch i { case 1: { } }", &fix->a), ==, RC_STR("a switch needs a default case"));
    RC_CHECK(body_errors("switch i { default: { } default: { } }", &fix->a), ==, RC_STR("a switch has only one default case"));
    RC_CHECK(body_errors("switch f { default: { } }", &fix->a), ==, RC_STR("a switch needs an integer, not f32"));
    RC_CHECK(body_errors("switch i { case i: { } default: { } }", &fix->a), ==, RC_STR("a case must be known before the shader runs"));
    RC_CHECK(body_errors("switch i { case 1u: { } default: { } }", &fix->a), ==, RC_STR("expected i32, got u32"));
}


RC_TEST_STEP(check, pointers, fix)
{
    RC_CHECK(errors("fn g(p: ptr<function, f32>) { *p = 1.0; }\n"
        "fn h() { var x = 0.0; g(&x); let p = &x; *p = 2.0; }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("fn g(p: ptr<private, vec3f>) { (*p).x = 1.0; p.y = 2.0; }\n"
        "fn h() { g(&v3); }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(body_errors("let p = &1;", &fix->a), ==, RC_STR("only memory has an address: a var, or part of one"));
    RC_CHECK(body_errors("let p = &v3.x;", &fix->a), ==, RC_STR("can't take the address of this"));
    RC_CHECK(body_errors("let x = *f;", &fix->a), ==, RC_STR("'*' needs a pointer, not f32"));
    RC_CHECK(errors("fn g(p: ptr<uniform, S>) {}\n", &fix->a), ==,
        RC_STR("only pointers to function and private memory are supported yet"));
}


RC_TEST_STEP(check, const_asserts, fix)
{
    RC_CHECK(errors("const_assert 1 < 2;\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("const_assert 2 < 1;\n", &fix->a), ==, RC_STR("this const_assert fails"));
    RC_CHECK(errors("const_assert 1.0 < 2.0;\n", &fix->a), ==, RC_STR("this condition must be made of integers and bools we can work out"));
    RC_CHECK(body_errors("const n = 4; const_assert n == 4;", &fix->a), ==, RC_STR(""));
}

// ---- declarations ----

RC_TEST_STEP(check, module_vars, fix)
{
    RC_CHECK(errors("var<private> p: f32 = 1.0;\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("var<private> p = vec2(1, 2);\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("@group(1) @binding(0) var<storage, read_write> sb: array<f32>;\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("var p: f32;\n", &fix->a), ==, RC_STR("this var needs an address space, such as var<private>"));
    RC_CHECK(errors("var<uniform> p: f32;\n", &fix->a), ==, RC_STR("this var needs @group and @binding"));
    RC_CHECK(errors("@group(0) @binding(9) var<uniform, read> p: f32;\n", &fix->a), ==, RC_STR("only storage vars take an access mode"));
    RC_CHECK(errors("@group(0) @binding(9) var<uniform> p: bool;\n", &fix->a), ==, RC_STR("a buffer can't hold bool"));
    RC_CHECK(errors("@group(0) @binding(9) var<uniform> p: array<f32>;\n", &fix->a), ==, RC_STR("a uniform buffer can't hold array<f32>"));
    RC_CHECK(errors("@group(0) @binding(9) var<private> p: f32;\n", &fix->a), ==,
        RC_STR("only uniform, storage and handle vars take attributes"));
    RC_CHECK(errors("var<private> p: f32 = f;\n", &fix->a), ==, RC_STR("a module-scope var needs a value known before the shader runs"));
    RC_CHECK(errors("var<function> p: f32;\n", &fix->a), ==, RC_STR("function vars only go in functions"));
    RC_CHECK(errors("var<workgroup> p: f32;\n", &fix->a), ==, RC_STR("workgroup vars aren't supported yet"));
    RC_CHECK(errors("var<private> p: sampler;\n", &fix->a), ==, RC_STR("samplers and textures take no address space"));
    RC_CHECK(errors("var<bogus> p: f32;\n", &fix->a), ==, RC_STR("expected an address space, such as 'private' or 'uniform'"));
}


RC_TEST_STEP(check, types_in_declarations, fix)
{
    RC_CHECK(errors("var<private> p: array<f32, 0>;\n", &fix->a), ==, RC_STR("this must be from 1 to 4294967295"));
    RC_CHECK(errors("const N = 3u;\nvar<private> p: array<vec2<f32>, N * 2>;\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("var<private> p: array<f32, f>;\n", &fix->a), ==, RC_STR("expected an integer, got f32"));
    RC_CHECK(errors("var<private> p: vec3;\n", &fix->a), ==, RC_STR("'vec3' takes 1 template argument"));
    RC_CHECK(errors("var<private> p: vec3<S>;\n", &fix->a), ==, RC_STR("expected a scalar type, got S"));
    RC_CHECK(errors("var<private> p: mat2x2<i32>;\n", &fix->a), ==, RC_STR("expected f32, got i32"));
    RC_CHECK(errors("var<private> p: f32<i32>;\n", &fix->a), ==, RC_STR("'f32' takes no template list"));
    RC_CHECK(errors("var<private> p: nope;\n", &fix->a), ==, RC_STR("there is no type called 'nope'"));
    RC_CHECK(errors("var<private> p: f16;\n", &fix->a), ==, RC_STR("'f16' isn't supported yet"));
    RC_CHECK(errors("var<private> p: atomic<i32>;\n", &fix->a), ==, RC_STR("atomics aren't supported yet"));
    RC_CHECK(errors("alias V = vec3f;\nvar<private> p: V;\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("struct Q { a: f32, a: i32 }\n", &fix->a), ==, RC_STR("'a' is already a member"));
    RC_CHECK(errors("struct Q { a: array<f32>, b: f32 }\n", &fix->a), ==, RC_STR("only the last member can be sized at run time"));
    RC_CHECK(errors("struct Q { a: sampler }\n", &fix->a), ==, RC_STR("a struct can't hold sampler"));
}


RC_TEST_STEP(check, attributes, fix)
{
    RC_CHECK(errors("struct Q { @align(16) @size(32) a: f32 }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("struct Q { @nope a: f32 }\n", &fix->a), ==, RC_STR("there is no attribute @nope"));
    RC_CHECK(errors("struct Q { @group(0) a: f32 }\n", &fix->a), ==, RC_STR("@group can't go here"));
    RC_CHECK(errors("struct Q { @size(0) a: f32 }\n", &fix->a), ==, RC_STR("this must be from 1 to 4294967295"));
    RC_CHECK(errors("struct Q { @align(4) @align(8) a: f32 }\n", &fix->a), ==, RC_STR("@align is here already"));
    RC_CHECK(errors("struct Q { @size a: f32 }\n", &fix->a), ==, RC_STR("@size has the wrong number of arguments"));
    RC_CHECK(errors("@vertex(1) fn g() -> @builtin(position) vec4f { return v4; }\n", &fix->a), ==, RC_STR("@vertex takes no arguments"));
    RC_CHECK(errors("struct Q { @builtin(nope) a: f32 }\n", &fix->a), ==, RC_STR("expected a built-in value, such as 'position'"));
    RC_CHECK(errors("struct Q { @interpolate(flat, nope) a: f32 }\n", &fix->a), ==, RC_STR("expected a sampling, such as 'centroid'"));
}

// ---- functions and entry points ----

RC_TEST_STEP(check, function_calls, fix)
{
    RC_CHECK(errors("fn g(a: f32, b: i32) -> f32 { return a; }\nfn h() { _ = g(1, 2); }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("fn g(a: f32) {}\nfn h() { g(); }\n", &fix->a), ==, RC_STR("'g' takes 1 argument, not 0"));
    RC_CHECK(errors("fn g(a: f32) {}\nfn h() { g(i); }\n", &fix->a), ==, RC_STR("expected f32, got i32"));
    RC_CHECK(errors("fn g() { h(); }\nfn h() { g(); }\n", &fix->a), ==, RC_STR("functions can't call themselves, even through others"));
    RC_CHECK(errors("fn g() { g(); }\n", &fix->a), ==, RC_STR("functions can't call themselves, even through others"));
    RC_CHECK(body_errors("f();", &fix->a), ==, RC_STR("'f' can't be called"));
    RC_CHECK(errors("fn takes(x: sampler, y: texture_2d<f32>) -> vec4f "
        "{ return textureSample(y, x, v2); }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("@fragment fn fs(x: sampler) {}\n", &fix->a), ==, RC_STR("an entry point can't take a sampler or texture"));
}


RC_TEST_STEP(check, entry_points, fix)
{
    RC_CHECK(errors(
        "struct VOut { @builtin(position) p: vec4f, @location(0) uv: vec2f, "
        "@location(1) @interpolate(flat) id: u32 }\n"
        "@vertex fn vs(@builtin(vertex_index) vi: u32, @location(0) pos: vec3f) -> VOut {\n"
        "    return VOut(vec4(pos, 1.0), vec2(0.0), vi);\n"
        "}\n"
        "@fragment fn fs(in: VOut, @builtin(front_facing) ff: bool) -> @location(0) vec4f {\n"
        "    return textureSample(t, s, in.uv);\n"
        "}\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("@vertex fn vs() -> @location(0) vec4f { return v4; }\n", &fix->a), ==,
        RC_STR("a vertex shader must return @builtin(position)"));
    RC_CHECK(errors("@fragment fn fs(@location(0) x: u32) {}\n", &fix->a), ==,
        RC_STR("an integer passed between stages needs @interpolate(flat)"));
    RC_CHECK(errors("@fragment fn fs(@location(0) x: f32, @location(0) y: f32) {}\n", &fix->a),
        ==, RC_STR("this location is taken already"));
    RC_CHECK(errors("@fragment fn fs(x: f32) {}\n", &fix->a), ==, RC_STR("this needs @location or @builtin"));
    RC_CHECK(errors("@fragment fn fs(@builtin(position) x: vec3f) {}\n", &fix->a), ==, RC_STR("this built-in is vec4<f32>, not vec3<f32>"));
    RC_CHECK(errors("@fragment fn fs(@builtin(vertex_index) x: u32) {}\n", &fix->a), ==, RC_STR("this built-in can't be an input here"));
    RC_CHECK(errors("@fragment fn fs(@location(0) x: bool) {}\n", &fix->a), ==, RC_STR("a location can't hold bool"));
    RC_CHECK(errors("@compute @workgroup_size(8) fn main_cs() {}\n", &fix->a), ==, RC_STR("compute shaders aren't supported yet"));
    RC_CHECK(errors("fn g(@location(0) x: f32) {}\n", &fix->a), ==, RC_STR("@location can't go here"));
}


RC_TEST_STEP(check, fragment_only, fix)
{
    RC_CHECK(errors("fn sample() -> vec4f { return textureSample(t, s, v2); }\n"
        "@vertex fn vs() -> @builtin(position) vec4f { return sample(); }\n", &fix->a), ==,
        RC_STR("only a fragment shader can do this"));
    RC_CHECK(errors("@vertex fn vs() -> @builtin(position) vec4f { "
        "return textureSampleLevel(t, s, v2, 0.0); }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("@vertex fn vs() -> @builtin(position) vec4f { discard; }\n", &fix->a), ==,
        RC_STR("only a fragment shader can do this"));
    RC_CHECK(errors("@vertex fn vs() -> @builtin(position) vec4f { return vec4(dpdx(f)); }\n",
        &fix->a), ==, RC_STR("only a fragment shader can do this"));
}


RC_TEST_STEP(check, abstract_values_settle_through_expressions, fix)
{
    rc_wgsl_check_result result = check("fn g() -> f32 { return 1 + 2 * 3.0; }\n", &fix->a);
    rc_wgsl_module module = result.module;
    uint32_t body = rc_wgsl_ast_fn_parts(module.ast, last_decl(module)).body;
    uint32_t ret = rc_view_wgsl_index_get(rc_wgsl_ast_list(module.ast, rc_wgsl_ast_node(module.ast, body).lhs), 0);
    uint32_t sum = rc_wgsl_ast_node(module.ast, ret).lhs;
    uint32_t product = rc_wgsl_ast_node(module.ast, sum).rhs;

    // Every part of the expression is f32 now, down to the literals
    RC_CHECK_FALSE(rc_wgsl_error_is_set(result.error));
    RC_CHECK(rc_wgsl_module_sema(module, sum).type, ==, (uint32_t)RC_WGSL_TYPE_ID_F32);
    RC_CHECK(rc_wgsl_module_sema(module, product).type, ==, (uint32_t)RC_WGSL_TYPE_ID_F32);
    RC_CHECK(rc_wgsl_module_sema(module, rc_wgsl_ast_node(module.ast, product).lhs).type, ==, (uint32_t)RC_WGSL_TYPE_ID_F32);
    RC_CHECK(rc_wgsl_module_sema(module, rc_wgsl_ast_node(module.ast, sum).lhs).type, ==, (uint32_t)RC_WGSL_TYPE_ID_F32);
}


RC_TEST_STEP(check, attribute_limits, fix)
{
    RC_CHECK(errors("@group(4) @binding(0) var tex: texture_2d<f32>;\n", &fix->a), ==, RC_STR("this must be from 0 to 3"));
    RC_CHECK(errors("@group(0) @binding(1000) var tex: texture_2d<f32>;\n", &fix->a), ==, RC_STR("this must be from 0 to 999"));
    RC_CHECK(errors("@fragment fn fs(@location(16) x: f32) {}\n", &fix->a), ==, RC_STR("this must be from 0 to 15"));
    RC_CHECK(errors("struct T { @align(3) a: f32 }\n", &fix->a), ==, RC_STR("@align must be a power of 2"));
    RC_CHECK(errors("struct T { @align(8) @size(16) a: f32 }\n", &fix->a), ==, RC_STR(""));
    RC_CHECK(errors("struct T { @size(4) @size(8) a: f32 }\n", &fix->a), ==, RC_STR("@size is here already"));
    RC_CHECK(errors("@group(0) @binding(0) var<storage> big: array<vec4f, 268435457>;\n", &fix->a), ==, RC_STR("this buffer is too large"));
}


RC_TEST_STEP(check, types_nest_only_so_deep, fix)
{
    rc_mstr structs = rc_mstr_make(2048, &fix->a);
    rc_mstr arrays = rc_mstr_make(512, &fix->a);

    rc_mstr_append(&structs, RC_STR("struct S0 { a: f32 }\n"), &fix->a);
    for (uint32_t i = 1; i < 40; i++) {
        rc_mstr_append(&structs, RC_STR("struct S"), &fix->a);
        rc_mstr_append_u32(&structs, i, &fix->a);
        rc_mstr_append(&structs, RC_STR(" { a: S"), &fix->a);
        rc_mstr_append_u32(&structs, i - 1, &fix->a);
        rc_mstr_append(&structs, RC_STR(" }\n"), &fix->a);
    }
    RC_CHECK(errors(structs.data, &fix->a), ==, RC_STR("this struct nests other types too deeply"));
    rc_mstr_append(&arrays, RC_STR("alias A = "), &fix->a);
    for (uint32_t i = 0; i < 40; i++) {
        rc_mstr_append(&arrays, RC_STR("array<"), &fix->a);
    }
    rc_mstr_append(&arrays, RC_STR("f32"), &fix->a);
    for (uint32_t i = 0; i < 40; i++) {
        rc_mstr_append(&arrays, RC_STR(", 1>"), &fix->a);
    }
    rc_mstr_append(&arrays, RC_STR(";\n"), &fix->a);
    RC_CHECK(errors(arrays.data, &fix->a), ==, RC_STR("this type nests other types too deeply"));
}


RC_TEST_STEP(check, bindings_are_used_once_by_each_entry_point, fix)
{
    static const char shared[] = "@group(1) @binding(0) var<uniform> ua: vec4f;\n"
        "@group(1) @binding(0) var<uniform> ub: vec4f;\n";
    rc_mstr both = rc_mstr_from_str(rc_str_from_cstr(shared), 512, &fix->a);
    rc_mstr apart = rc_mstr_from_str(rc_str_from_cstr(shared), 512, &fix->a);

    rc_mstr_append(&both, RC_STR(
        "fn fetch() -> vec4f { return ub; }\n"
        "@fragment fn fs() -> @location(0) vec4f { return ua + fetch(); }\n"), &fix->a);
    RC_CHECK(errors(both.data, &fix->a), ==, RC_STR("'ub' has the @group and @binding of another resource this entry point uses"));
    rc_mstr_append(&apart, RC_STR(
        "@fragment fn fs() -> @location(0) vec4f { return ua; }\n"
        "@fragment fn fs2() -> @location(0) vec4f { return ub; }\n"), &fix->a);
    RC_CHECK(errors(apart.data, &fix->a), ==, RC_STR(""));
}


RC_TEST_STEP(check, pointers_never_alias, fix)
{
    static const char swap[] = "fn swap(a: ptr<function, f32>, b: ptr<function, f32>) { let t = *a; *a = *b; *b = t; }\n";
    static const char bump[] = "var<private> gv: f32;\n"
        "fn bump(p: ptr<private, f32>) { *p += 1.0; }\n"
        "fn peek(p: ptr<private, f32>) -> f32 { *p += 1.0; return gv; }\n"
        "fn deeper(p: ptr<private, f32>) -> f32 { return peek(p); }\n";
    rc_mstr same = rc_mstr_from_str(rc_str_from_cstr(swap), 512, &fix->a);
    rc_mstr apart = rc_mstr_from_str(rc_str_from_cstr(swap), 512, &fix->a);
    rc_mstr global = rc_mstr_from_str(rc_str_from_cstr(bump), 512, &fix->a);
    rc_mstr through = rc_mstr_from_str(rc_str_from_cstr(bump), 512, &fix->a);
    rc_mstr fine = rc_mstr_from_str(rc_str_from_cstr(bump), 512, &fix->a);

    rc_mstr_append(&same, RC_STR("fn caller() { var x = 1.0; let p = &x; swap(&x, p); }\n"), &fix->a);
    RC_CHECK(errors(same.data, &fix->a), ==, RC_STR("this points at the same memory as another argument, which WGSL forbids"));
    rc_mstr_append(&apart, RC_STR("fn caller() { var x = 1.0; var y = 2.0; swap(&x, &y); }\n"), &fix->a);
    RC_CHECK(errors(apart.data, &fix->a), ==, RC_STR(""));
    rc_mstr_append(&global, RC_STR("fn caller() -> f32 { return peek(&gv); }\n"), &fix->a);
    RC_CHECK(errors(global.data, &fix->a), ==, RC_STR("the function called uses 'gv' itself, which WGSL forbids with a pointer to it"));
    rc_mstr_append(&through, RC_STR("fn caller() -> f32 { return deeper(&gv); }\n"), &fix->a);
    RC_CHECK(errors(through.data, &fix->a), ==, RC_STR("the function called uses 'gv' itself, which WGSL forbids with a pointer to it"));
    rc_mstr_append(&fine, RC_STR("fn caller() { bump(&gv); }\n"), &fix->a);
    RC_CHECK(errors(fine.data, &fix->a), ==, RC_STR(""));
}


RC_TEST_STEP(check, every_builtin_is_found_by_its_name, fix)
{
    // The predeclared names are found by a binary search, so this catches them out of order too
    for (uint32_t i = RC_WGSL_BUILTIN_NONE + 1; i <= RC_WGSL_BUILTIN_UNPACK2X16UNORM; i++) {
        rc_wgsl_predeclared found = rc_wgsl_predeclared_find(rc_wgsl_builtin_name((rc_wgsl_builtin)i));

        RC_CHECK(found.type, ==, RC_WGSL_PREDECLARED_TYPE_FUNCTION);
        RC_CHECK(found.builtin, ==, i);
    }
    (void)fix;
}
