// test_glsl.c
//
// Tests for the GLSL writer. Each writes one entry point of a small shader and looks for the
// lines that matter in the result, or reads back the error.

#include "glsl.h"
#include "parser.h"

#include <stdio.h>

#include "richc/file.h"
#include "richc/mstr.h"
#include "richc/test.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(glsl) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(glsl, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(glsl, fix)
{
    rc_arena_release(&fix->a);
}


// Save a program the tests wrote, for the build to check with a GLSL compiler if it has one.
// The file's extension gives its stage, and its name its target.
static void save(rc_str code, rc_shader_target target, uint8_t stage, rc_arena *arena)
{
    static uint32_t count;
    rc_mstr path = rc_mstr_from_str(RC_STR("glsl_out/glsl_"), 64, arena);

    rc_mstr_append_u32(&path, count++, arena);
    rc_mstr_append(&path, target == RC_SHADER_TARGET_GLSL_ES300 ? RC_STR(".es") : RC_STR(".gl"), arena);
    rc_mstr_append(&path, stage == RC_SHADER_STAGE_VERTEX ? RC_STR(".vert") : RC_STR(".frag"), arena);
    rc_file_save_text(path.view, code);
}


// The GLSL for the first entry point of source for target, or the error message.
static rc_str glsl(const char *source, rc_shader_target target, rc_arena *arena)
{
    rc_str text = rc_str_from_cstr(source);
    rc_wgsl_lex_result lexed = rc_wgsl_lex(text, arena);
    rc_wgsl_parse_result parsed = rc_wgsl_parse(text, lexed.tokens, arena);
    rc_wgsl_check_result checked;
    rc_writer_result written;
    rc_wgsl_entry entry;
    rc_mstr code;

    if (rc_wgsl_error_is_set(lexed.error) || rc_wgsl_error_is_set(parsed.error)) {
        return RC_STR("doesn't parse");
    }

    checked = rc_wgsl_check(parsed.ast, arena);
    if (rc_wgsl_error_is_set(checked.error)) {
        return checked.error.message;
    }

    entry = rc_view_wgsl_entry_get(checked.module.entries, 0);
    written = rc_glsl_write(checked.module, entry, target, arena);
    if (rc_wgsl_error_is_set(written.error)) {
        return written.error.message;
    }

    code = rc_mstr_from_str(written.declarations, 4096, arena);
    rc_mstr_append(&code, written.polyfills, arena);
    rc_mstr_append(&code, written.code, arena);
    save(code.view, target, entry.stage, arena);
    return code.view;
}


// The GLSL ES for a fragment shader returning expression, with statements before.
static rc_str fragment(const char *statements, const char *expression, rc_arena *arena)
{
    rc_mstr source = rc_mstr_from_str(RC_STR(
        "var<private> f: f32;\nvar<private> i: i32;\nvar<private> u: u32;\n"
        "var<private> v3: vec3f;\nvar<private> iv3: vec3i;\nvar<private> b: bool;\n"
        "@fragment fn fs() -> @location(0) vec4f {\n"), 512, arena);

    rc_mstr_append(&source, rc_str_from_cstr(statements), arena);
    rc_mstr_append(&source, RC_STR("\n    return vec4f("), arena);
    rc_mstr_append(&source, rc_str_from_cstr(expression), arena);
    rc_mstr_append(&source, RC_STR(");\n}\n"), arena);
    return glsl(source.data, RC_SHADER_TARGET_GLSL_ES300, arena);
}


// True if text holds part. If not, both are printed, to show what was written instead.
static bool holds(rc_str text, const char *part)
{
    bool found = rc_str_contains(text, rc_str_from_cstr(part));

    if (!found) {
        printf("    expected to find:\n%s\n    in:\n%.*s\n", part, (int)text.len, text.data);
    }

    return found;
}


static const char triangle[] = "struct Out { @builtin(position) pos: vec4f, @location(0) colour: vec3f }\n"
    "@vertex fn vs(@builtin(vertex_index) i: u32, @location(0) p: vec2f) -> Out {\n"
    "    return Out(vec4(p, 0.0, 1.0), vec3(f32(i)));\n"
    "}\n";

// ---- programs ----

RC_TEST_STEP(glsl, a_vertex_shader_in_es, fix)
{
    rc_str code = glsl(triangle, RC_SHADER_TARGET_GLSL_ES300, &fix->a);

    RC_CHECK_TRUE(rc_str_starts_with(code, RC_STR("#version 300 es\nprecision highp float;\n")));
    RC_CHECK_TRUE(holds(code, "struct Out {\n    vec4 pos;\n    vec3 colour;\n};"));
    RC_CHECK_TRUE(holds(code, "layout(location = 0) in vec2 rc_attr_0;"));
    RC_CHECK_TRUE(holds(code, "out vec3 rc_var_0;"));
    RC_CHECK_TRUE(holds(code, "Out vs(uint i, vec2 p) {"));
    RC_CHECK_TRUE(holds(code, "Out rc_result = vs(uint(gl_VertexID), rc_attr_0);"));
    RC_CHECK_TRUE(holds(code, "gl_Position = rc_result.pos;"));
    RC_CHECK_TRUE(holds(code, "rc_var_0 = rc_result.colour;"));
    RC_CHECK_TRUE(holds(code, "gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;"));
}


RC_TEST_STEP(glsl, a_vertex_shader_in_460, fix)
{
    rc_str code = glsl(triangle, RC_SHADER_TARGET_GLSL_460, &fix->a);

    // Desktop GL sets clip control instead of remapping depth
    RC_CHECK_TRUE(rc_str_starts_with(code, RC_STR("#version 460\n")));
    RC_CHECK_FALSE(rc_str_contains(code, RC_STR("gl_Position.z")));
    RC_CHECK_FALSE(rc_str_contains(code, RC_STR("precision")));
}


RC_TEST_STEP(glsl, a_fragment_shader, fix)
{
    rc_str code = glsl(
        "@fragment fn fs(@location(0) c: vec3f, @location(1) @interpolate(flat) id: u32,\n"
        "    @builtin(position) p: vec4f) -> @location(0) vec4f {\n"
        "    return vec4(c, f32(id) + p.x);\n"
        "}\n", RC_SHADER_TARGET_GLSL_ES300, &fix->a);

    RC_CHECK_TRUE(holds(code, "in vec3 rc_var_0;"));
    RC_CHECK_TRUE(holds(code, "flat in uint rc_var_1;"));
    RC_CHECK_TRUE(holds(code, "layout(location = 0) out vec4 rc_frag_0;"));
    RC_CHECK_TRUE(holds(code, "vec4 rc_result = fs(rc_var_0, rc_var_1, gl_FragCoord);"));
    RC_CHECK_TRUE(holds(code, "rc_frag_0 = rc_result;"));
    RC_CHECK_FALSE(rc_str_contains(code, RC_STR("gl_Position")));
}


RC_TEST_STEP(glsl, names_glsl_keeps, fix)
{
    rc_str code = fragment("let input = 1.0; let rc_x = 2.0; let texture = 3.0;", "input + rc_x + texture", &fix->a);

    RC_CHECK_TRUE(holds(code, "float rc_u_input = 1.0;"));
    RC_CHECK_TRUE(holds(code, "float rc_u_rc_x = 2.0;"));
    RC_CHECK_TRUE(holds(code, "float rc_u_texture = 3.0;"));
}


RC_TEST_STEP(glsl, globals_get_their_values_in_main, fix)
{
    rc_str code = glsl(
        "var<private> counter: i32;\nvar<private> scale = 2.5;\nconst LIMIT: u32 = 3u;\n"
        "@fragment fn fs() -> @location(0) vec4f {\n"
        "    counter++; return vec4(scale, f32(LIMIT), 0, 1);\n}\n",
        RC_SHADER_TARGET_GLSL_ES300, &fix->a);

    RC_CHECK_TRUE(holds(code, "int counter;\nfloat scale;\nuint LIMIT;"));
    RC_CHECK_TRUE(holds(code, "void main() {\n    counter = 0;\n    scale = 2.5;\n    LIMIT = 3u;"));
}


RC_TEST_STEP(glsl, functions_come_after_their_callees, fix)
{
    rc_str code = glsl(
        "@fragment fn fs() -> @location(0) vec4f { return vec4(a()); }\n"
        "fn a() -> f32 { return b() * 2.0; }\n"
        "fn b() -> f32 { return 1.0; }\n"
        "fn unused() -> f32 { return 3.0; }\n", RC_SHADER_TARGET_GLSL_ES300, &fix->a);
    uint32_t b = rc_str_find_first(code, RC_STR("float b()"));
    uint32_t a = rc_str_find_first(code, RC_STR("float a()"));
    uint32_t fs = rc_str_find_first(code, RC_STR("vec4 fs()"));

    RC_CHECK_TRUE(b < a && a < fs && fs != RC_INDEX_NONE);
    RC_CHECK_FALSE(rc_str_contains(code, RC_STR("unused")));
}

// ---- values ----

RC_TEST_STEP(glsl, literals, fix)
{
    rc_str code = fragment("let a = 1u; let c = -7; let d = 1.5f; let e = 0x1p3; let g = 1e3;", "f32(a) + f32(c) + d + e + g", &fix->a);

    RC_CHECK_TRUE(holds(code, "uint a = 1u;"));
    RC_CHECK_TRUE(holds(code, "int c = (-7);"));
    RC_CHECK_TRUE(holds(code, "float d = 1.5;"));
    RC_CHECK_TRUE(holds(code, "float e = 8.0;"));
    RC_CHECK_TRUE(holds(code, "float g = 1e3;"));
}


RC_TEST_STEP(glsl, hex_floats_are_exact, fix)
{
    // Rounded correctly, an f32 straight to f32: 0x1.000001p0f is a tie between 1 and the next
    // float, so goes to even, while 0x1.0000018p0f is past it. An abstract float keeps a double's
    // precision, every hex digit counting.
    rc_str code = fragment(
        "let a = 0x1.000001p0f; let b = 0x1.0000018p0f; let c = 0x1.8p1; let d = 0x.8; let e = 0x1.0000000000000801p0;",
        "a + b + c + d + e", &fix->a);

    RC_CHECK_TRUE(holds(code, "float a = 1.0;"));
    RC_CHECK_TRUE(holds(code, "float b = 1.00000012;"));
    RC_CHECK_TRUE(holds(code, "float c = 3.0;"));
    RC_CHECK_TRUE(holds(code, "float d = 0.5;"));
    RC_CHECK_TRUE(holds(code, "float e = 1.0000000000000002;"));
}


RC_TEST_STEP(glsl, abstract_consts_take_each_use_s_type, fix)
{
    rc_str code = glsl(
        "const K = 3;\nconst HALF = 0.5;\n"
        "@fragment fn fs() -> @location(0) vec4f {\n"
        "    let a: u32 = K; let c: f32 = f32(K) * HALF; let d = vec2(HALF, K);\n"
        "    return vec4(f32(a), c, d);\n"
        "}\n", RC_SHADER_TARGET_GLSL_ES300, &fix->a);

    RC_CHECK_TRUE(holds(code, "uint a = 3u;"));
    RC_CHECK_TRUE(holds(code, "float c = ((3.0) * (0.5));"));
    RC_CHECK_TRUE(holds(code, "vec2 d = vec2((0.5), (3.0));"));
    RC_CHECK_FALSE(rc_str_contains(code, RC_STR("HALF")));
}


RC_TEST_STEP(glsl, zero_values, fix)
{
    rc_str code = fragment("var a: vec3f; var c: array<i32, 2>; var d: mat2x2f; var e: bool;",
        "a.x + f32(c[0]) + d[0][0] + select(0.0, 1.0, e)", &fix->a);

    RC_CHECK_TRUE(holds(code, "vec3 a = vec3(0.0);"));
    RC_CHECK_TRUE(holds(code, "int[2] c = int[2](0, 0);"));
    RC_CHECK_TRUE(holds(code, "mat2 d = mat2(0.0);"));
    RC_CHECK_TRUE(holds(code, "bool e = false;"));
}

// ---- operators ----

RC_TEST_STEP(glsl, division_and_remainder, fix)
{
    rc_str code = fragment("let a = i / 2; let c = i / i; let d = i % 4; let e = u % 4u;"
        "let g = f % 2.0; let h = iv3 / i;",
        "f32(a + c + d) + f32(e) + g + f32(h.x)", &fix->a);

    RC_CHECK_TRUE(holds(code, "int a = (i / 2);"));
    RC_CHECK_TRUE(holds(code, "int c = rc_div(i, i);"));
    RC_CHECK_TRUE(holds(code, "int d = rc_mod(i, 4);"));
    RC_CHECK_TRUE(holds(code, "uint e = (u % 4u);"));
    RC_CHECK_TRUE(holds(code, "float g = rc_fmod(f, 2.0);"));
    RC_CHECK_TRUE(holds(code, "ivec3 h = rc_div(iv3, ivec3(i));"));
    RC_CHECK_TRUE(holds(code, "float rc_fmod(float a, float b) { return a - b * trunc(a / b); }"));
    RC_CHECK_TRUE(holds(code, "ivec3 rc_div(ivec3 a, ivec3 b) { return ivec3(rc_div(a.x, b.x), "
        "rc_div(a.y, b.y), rc_div(a.z, b.z)); }"));

    // The scalar polyfill comes first, for the vector one to call
    RC_CHECK_TRUE(rc_str_find_first(code, RC_STR("int rc_div(int a")) < rc_str_find_first(code, RC_STR("ivec3 rc_div(")));
}


RC_TEST_STEP(glsl, shifts_and_bits, fix)
{
    rc_str code = fragment("let a = i << u; let c = i << 3u; let d = b & (f > 0.0);", "f32(a + c) + select(0.0, 1.0, d)", &fix->a);

    RC_CHECK_TRUE(holds(code, "int a = (i << (u & 31u));"));
    RC_CHECK_TRUE(holds(code, "int c = (i << 3u);"));
    RC_CHECK_TRUE(holds(code, "bool d = rc_and(b, (f > 0.0));"));
}


RC_TEST_STEP(glsl, vector_comparisons, fix)
{
    rc_str code = fragment("let a = v3 < v3; let c = v3 == v3; let d = !a;", "select(0.0, 1.0, all(c) && any(d))", &fix->a);

    RC_CHECK_TRUE(holds(code, "bvec3 a = lessThan(v3, v3);"));
    RC_CHECK_TRUE(holds(code, "bvec3 c = equal(v3, v3);"));
    RC_CHECK_TRUE(holds(code, "bvec3 d = not(a);"));
}


RC_TEST_STEP(glsl, compound_assignments_are_written_out, fix)
{
    rc_str code = fragment("var x = 1.0; x += 2.0; x %= 3.0; var n = 4; n /= i; n++;", "x + f32(n)", &fix->a);

    RC_CHECK_TRUE(holds(code, "x = (x + 2.0);"));
    RC_CHECK_TRUE(holds(code, "x = rc_fmod(x, 3.0);"));
    RC_CHECK_TRUE(holds(code, "n = rc_div(n, i);"));
    RC_CHECK_TRUE(holds(code, "n++;"));
}

// ---- built-ins ----

RC_TEST_STEP(glsl, builtins, fix)
{
    rc_str code = fragment("let a = round(f); let c = inverseSqrt(f); let d = atan2(f, f);"
        "let e = saturate(v3); let g = select(1, 2, b); let h = select(v3, v3, v3 > v3);"
        "let k = dot(iv3, iv3); let m = abs(u); let n = bitcast<u32>(f); let p = i32(f);",
        "a + c + d + e.x + f32(g) + h.x + f32(k) + f32(m + n) + f32(p)", &fix->a);

    RC_CHECK_TRUE(holds(code, "float a = roundEven(f);"));
    RC_CHECK_TRUE(holds(code, "float c = inversesqrt(f);"));
    RC_CHECK_TRUE(holds(code, "float d = atan(f, f);"));
    RC_CHECK_TRUE(holds(code, "vec3 e = clamp(v3, 0.0, 1.0);"));
    RC_CHECK_TRUE(holds(code, "int g = rc_select(1, 2, b);"));
    RC_CHECK_TRUE(holds(code, "vec3 h = mix(v3, v3, greaterThan(v3, v3));"));
    RC_CHECK_TRUE(holds(code, "int k = rc_dot(iv3, iv3);"));
    RC_CHECK_TRUE(holds(code, "uint m = u;"));
    RC_CHECK_TRUE(holds(code, "uint n = floatBitsToUint(f);"));
    RC_CHECK_TRUE(holds(code, "int p = rc_f2i(f);"));
}


RC_TEST_STEP(glsl, textures, fix)
{
    rc_str code = glsl(
        "@group(0) @binding(0) var t: texture_2d<f32>;\n"
        "@group(0) @binding(1) var s: sampler;\n"
        "@group(1) @binding(0) var d: texture_depth_2d;\n"
        "@group(1) @binding(1) var cs: sampler_comparison;\n"
        "@group(1) @binding(2) var arr: texture_2d_array<f32>;\n"
        "@fragment fn fs(@location(0) uv: vec2f) -> @location(0) vec4f {\n"
        "    let a = textureSample(t, s, uv);\n"
        "    let c = textureSampleLevel(t, s, uv, 2.0, vec2i(1, 1));\n"
        "    let e = textureSampleCompare(d, cs, uv, 0.5);\n"
        "    let g = textureSampleCompareLevel(d, cs, uv, 0.5);\n"
        "    let h = textureLoad(t, vec2u(1u, 2u), 0);\n"
        "    let k = textureSample(arr, s, uv, 3);\n"
        "    let m = textureDimensions(arr);\n"
        "    let n = textureSampleBias(t, s, uv, 1.0, vec2i(0, 1));\n"
        "    return a + c + vec4(e + g) + h + k + vec4(f32(m.x)) + n;\n"
        "}\n", RC_SHADER_TARGET_GLSL_ES300, &fix->a);

    RC_CHECK_TRUE(holds(code, "uniform highp sampler2D rc_t0_0_s0_1;"));
    RC_CHECK_TRUE(holds(code, "uniform highp sampler2DShadow rc_t1_0_s1_1;"));
    RC_CHECK_TRUE(holds(code, "uniform highp sampler2D rc_t0_0;"));
    RC_CHECK_TRUE(holds(code, "uniform highp sampler2DArray rc_t1_2_s0_1;"));
    RC_CHECK_TRUE(holds(code, "vec4 a = texture(rc_t0_0_s0_1, uv);"));
    RC_CHECK_TRUE(holds(code, "vec4 c = textureLodOffset(rc_t0_0_s0_1, uv, 2.0, ivec2(1, 1));"));
    RC_CHECK_TRUE(holds(code, "float e = texture(rc_t1_0_s1_1, vec3(uv, 0.5));"));
    RC_CHECK_TRUE(holds(code, "float g = textureLod(rc_t1_0_s1_1, vec3(uv, 0.5), 0.0);"));
    RC_CHECK_TRUE(holds(code, "vec4 h = texelFetch(rc_t0_0, ivec2(uvec2(1u, 2u)), int(0));"));
    RC_CHECK_TRUE(holds(code, "vec4 k = texture(rc_t1_2_s0_1, vec3(uv, float(3)));"));
    RC_CHECK_TRUE(holds(code, "uvec2 m = uvec2(textureSize(rc_t1_2, 0).xy);"));
    RC_CHECK_TRUE(holds(code, "vec4 n = textureOffset(rc_t0_0_s0_1, uv, ivec2(0, 1), 1.0);"));
}

// ---- statements ----

RC_TEST_STEP(glsl, control_flow, fix)
{
    rc_str code = fragment(
        "var x = 0.0;\n"
        "if b { x = 1.0; } else if f > 0.0 { x = 2.0; } else { x = 3.0; }\n"
        "for (var j = 0; j < 4; j++) { if j == 2 { continue; } x += 1.0; }\n"
        "while x < 10.0 { x *= 2.0; }\n"
        "switch i { case 1, 2: { x = 0.0; } default: { break; } }\n",
        "x", &fix->a);

    RC_CHECK_TRUE(holds(code, "if (b) {\n        x = 1.0;\n    } else if ((f > 0.0)) {"));
    RC_CHECK_TRUE(holds(code, "} else {\n        x = 3.0;\n    }"));
    RC_CHECK_TRUE(holds(code, "for (int j = 0; (j < 4); j++) {"));
    RC_CHECK_TRUE(holds(code, "while ((x < 10.0)) {"));
    RC_CHECK_TRUE(holds(code, "switch (i) {\n    case 1:\n    case 2:\n    {\n        x = 0.0;\n"
        "        break;\n    }\n    default:\n    {\n        break;\n        break;\n    }\n    }"));
}


RC_TEST_STEP(glsl, loops_with_continuing, fix)
{
    rc_str end = fragment("var n = 0; loop { let k = n; if k > 3 { break; } "
        "continuing { n = k + 1; } }", "f32(n)", &fix->a);
    rc_str flag = fragment("var n = 0; loop { n++; if n < 2 { continue; } "
        "continuing { break if n > 3; } }", "f32(n)", &fix->a);

    // With no continue, the continuing block ends the body and sees its names
    RC_CHECK_TRUE(holds(end, "while (true) {\n        int k = n;"));
    RC_CHECK_TRUE(holds(end, "n = (k + 1);\n    }"));

    // With one, a flag runs it at the top of every pass but the first
    RC_CHECK_TRUE(holds(flag, "bool rc_loop_"));
    RC_CHECK_TRUE(holds(flag, "if ((n > 3)) {\n                break;\n            }"));
    RC_CHECK(fragment("var n = 0; loop { let k = n; if k < 2 { continue; } "
        "continuing { n = k; } }", "f32(n)", &fix->a), ==,
        RC_STR("a continuing block can only use names from its loop's body when the body doesn't "
            "use continue"));
}


RC_TEST_STEP(glsl, pointers, fix)
{
    rc_str code = glsl(
        "fn bump(p: ptr<function, f32>) { *p += 1.0; }\n"
        "@fragment fn fs() -> @location(0) vec4f {\n"
        "    var x = 0.0; var arr: array<f32, 2>;\n"
        "    bump(&x); let p = &arr[1]; *p = 2.0;\n"
        "    return vec4(x, arr[1], 0.0, 1.0);\n"
        "}\n", RC_SHADER_TARGET_GLSL_ES300, &fix->a);

    RC_CHECK_TRUE(holds(code, "void bump(inout float p) {\n    p = (p + 1.0);\n}"));
    RC_CHECK_TRUE(holds(code, "bump(x);"));
    RC_CHECK_TRUE(holds(code, "arr[1] = 2.0;"));
    RC_CHECK(fragment("var arr: array<f32, 2>; var j = 0; let p = &arr[j]; *p = 1.0;", "1.0",
        &fix->a), ==, RC_STR("this pointer's index can change: give the index a let of its own"));
}

// ---- resources ----

RC_TEST_STEP(glsl, uniform_buffers, fix)
{
    rc_str code = glsl(
        "struct U { m: mat4x4f, v: vec3f, s: f32 }\n"
        "@group(2) @binding(3) var<uniform> u: U;\n"
        "@fragment fn fs() -> @location(0) vec4f { return u.m[0] * u.s; }\n",
        RC_SHADER_TARGET_GLSL_ES300, &fix->a);

    RC_CHECK_TRUE(holds(code, "layout(std140) uniform rc_ubo_2_3 {\n    U u;\n};"));
    RC_CHECK_TRUE(holds(code, "return (u.m[0] * u.s);"));
}


RC_TEST_STEP(glsl, uniform_layouts_must_match_std140, fix)
{
    rc_str message = RC_STR("GLSL's std140 lays this out differently: in a uniform buffer, avoid "
        "matrices with two rows, and use @align and @size only where std140 agrees");

    RC_CHECK(glsl("struct U { m: mat2x2f }\n@group(0) @binding(0) var<uniform> u: U;\n"
        "@fragment fn fs() -> @location(0) vec4f { return vec4(u.m[0], 0, 0); }\n",
        RC_SHADER_TARGET_GLSL_ES300, &fix->a), ==, message);
    RC_CHECK(glsl("struct U { a: f32, @align(16) b: f32 }\n@group(0) @binding(0) var<uniform> u: U;\n"
        "@fragment fn fs() -> @location(0) vec4f { return vec4(u.a, u.b, 0, 0); }\n",
        RC_SHADER_TARGET_GLSL_ES300, &fix->a), ==, message);
    RC_CHECK_TRUE(holds(glsl("struct U { a: f32, @align(16) b: vec4f, @size(16) c: f32 }\n"
        "@group(0) @binding(0) var<uniform> u: U;\n"
        "@fragment fn fs() -> @location(0) vec4f { return u.b * u.a * u.c; }\n",
        RC_SHADER_TARGET_GLSL_ES300, &fix->a), "uniform rc_ubo_0_0"));
}


RC_TEST_STEP(glsl, storage_buffers, fix)
{
    static const char source[] = "struct Particles { count: u32, items: array<vec4f> }\n"
        "@group(0) @binding(0) var<storage> particles: Particles;\n"
        "@group(0) @binding(1) var<storage, read_write> out_data: array<f32>;\n"
        "@fragment fn fs() -> @location(0) vec4f {\n"
        "    out_data[0] = f32(arrayLength(&out_data));\n"
        "    return particles.items[particles.count - 1u];\n"
        "}\n";
    rc_str code = glsl(source, RC_SHADER_TARGET_GLSL_460, &fix->a);

    RC_CHECK_TRUE(holds(code, "layout(std430) readonly buffer rc_ssbo_0_0 {\n    uint count;\n"
        "    vec4 items[];\n} particles;"));
    RC_CHECK_TRUE(holds(code, "layout(std430) buffer rc_ssbo_0_1 {\n    float out_data[];\n};"));
    RC_CHECK_TRUE(holds(code, "out_data[clamp(0, 0, int(out_data.length()) - 1)] = float(uint(out_data.length()));"));
    RC_CHECK_TRUE(holds(code, "return particles.items[min((particles.count - 1u), uint(particles.items.length()) - 1u)];"));
    RC_CHECK(glsl(source, RC_SHADER_TARGET_GLSL_ES300, &fix->a), ==, RC_STR("storage buffers can't be written in GLSL ES 3.00"));
}


RC_TEST_STEP(glsl, what_es_can_t_say, fix)
{
    static const char linear[] = "@fragment fn fs(@location(0) @interpolate(linear) c: f32) -> @location(0) vec4f {\n"
        "    return vec4(c);\n}\n";
    static const char instance[] = "@vertex fn vs(@builtin(instance_index) i: u32) -> @builtin(position) vec4f {\n"
        "    return vec4(f32(i));\n}\n";

    RC_CHECK(glsl(linear, RC_SHADER_TARGET_GLSL_ES300, &fix->a), ==, RC_STR("@interpolate(linear) can't be written in GLSL ES 3.00"));
    RC_CHECK_TRUE(holds(glsl(linear, RC_SHADER_TARGET_GLSL_460, &fix->a), "noperspective in float rc_var_0;"));
    RC_CHECK_TRUE(holds(glsl(instance, RC_SHADER_TARGET_GLSL_ES300, &fix->a), "uint(gl_InstanceID)"));
    RC_CHECK_TRUE(holds(glsl(instance, RC_SHADER_TARGET_GLSL_460, &fix->a), "uint(gl_InstanceID + gl_BaseInstance)"));
    RC_CHECK(glsl("@group(0) @binding(0) var t: texture_1d<f32>;\n"
        "@fragment fn fs() -> @location(0) vec4f { return textureLoad(t, 0, 0); }\n",
        RC_SHADER_TARGET_GLSL_ES300, &fix->a), ==,
        RC_STR("this texture type can't be written in GLSL ES 3.00"));
    RC_CHECK(glsl("var<private> a: array<array<f32, 2>, 2>;\n"
        "@fragment fn fs() -> @location(0) vec4f { return vec4(a[0][0]); }\n",
        RC_SHADER_TARGET_GLSL_ES300, &fix->a), ==,
        RC_STR("arrays of arrays can't be written in GLSL ES 3.00"));
}


RC_TEST_STEP(glsl, indices_stay_in_bounds, fix)
{
    static const char source[] = "var<private> arr: array<f32, 4>;\n"
        "var<private> v: vec3f;\n"
        "var<private> m: mat2x2f;\n"
        "@fragment fn fs(@location(0) @interpolate(flat) i: i32, @location(1) @interpolate(flat) "
        "u: u32) -> @location(0) vec4f {\n"
        "    return vec4(arr[i] + arr[2] + v[u] + m[u].y);\n"
        "}\n";
    rc_str code = glsl(source, RC_SHADER_TARGET_GLSL_ES300, &fix->a);

    RC_CHECK_TRUE(holds(code, "arr[clamp(i, 0, 3)]"));
    RC_CHECK_TRUE(holds(code, "arr[2]"));
    RC_CHECK_TRUE(holds(code, "v[min(u, 2u)]"));
    RC_CHECK_TRUE(holds(code, "m[min(u, 1u)].y"));
}


RC_TEST_STEP(glsl, textures_stay_where_they_are_declared, fix)
{
    static const char source[] = "@group(0) @binding(0) var tex: texture_2d<f32>;\n"
        "@group(0) @binding(1) var smp: sampler;\n"
        "fn take(t: texture_2d<f32>, s: sampler) -> vec4f { return textureSample(t, s, vec2(0.5)); }\n"
        "@fragment fn fs() -> @location(0) vec4f { return take(tex, smp); }\n";

    RC_CHECK(glsl(source, RC_SHADER_TARGET_GLSL_460, &fix->a), ==, RC_STR("a sampler or texture parameter can't be written in GLSL 4.60"));
}
