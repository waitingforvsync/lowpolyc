// glsl_builtin.c
//
// Built-in functions in GLSL, the texture functions on their combined samplers, and the polyfills.
//
// Most built-ins have a GLSL function of the same meaning, if not always the same name. The rest
// are written out in place (saturate as clamp) or by a polyfill.

#include "glsl_writer.h"

// ---- polyfills ----

static void rc_glsl_ensure_polyfill(rc_glsl_writer *w, rc_glsl_polyfill polyfill,
    uint32_t operand_type, uint32_t node, rc_arena *arena);

// Append "T name(T a, T b" for a polyfill on operand_type, ready for its body.
static void rc_glsl_polyfill_head(rc_glsl_writer *w, uint32_t result, rc_str name, uint32_t a,
    uint32_t b, uint32_t node, rc_arena *arena)
{
    rc_mstr *out = &w->polyfills;

    rc_glsl_write_type(w, out, result, node, arena);
    rc_mstr_append(out, RC_STR(" "), arena);
    rc_mstr_append(out, name, arena);
    rc_mstr_append(out, RC_STR("("), arena);
    rc_glsl_write_type(w, out, a, node, arena);
    rc_mstr_append(out, RC_STR(" a"), arena);
    if (b != 0) {
        rc_mstr_append(out, RC_STR(", "), arena);
        rc_glsl_write_type(w, out, b, node, arena);
        rc_mstr_append(out, RC_STR(" b"), arena);
    }
}


// Append a vector polyfill's body: the scalar polyfill name applied to each component, as
// "{ return ivec3(rc_div(a.x, b.x), ...); }". joiner, if not empty, replaces the call: a.x && b.x.
static void rc_glsl_polyfill_by_component(rc_glsl_writer *w, uint32_t operand_type, rc_str name,
    rc_str joiner, uint32_t node, rc_arena *arena)
{
    rc_mstr *out = &w->polyfills;
    uint32_t width = rc_glsl_typeinfo(w, operand_type).width;
    static const char letters[] = "xyzw";

    rc_mstr_append(out, RC_STR(") { return "), arena);
    rc_glsl_write_type(w, out, operand_type, node, arena);
    rc_mstr_append(out, RC_STR("("), arena);
    for (uint32_t i = 0; i < width; i++) {
        rc_str letter = rc_str_from_ptr(letters + i, 1);

        rc_mstr_append(out, i > 0 ? RC_STR(", ") : RC_STR(""), arena);
        if (rc_str_is_empty(joiner)) {
            rc_mstr_append(out, name, arena);
            rc_mstr_append(out, RC_STR("(a."), arena);
            rc_mstr_append(out, letter, arena);
            rc_mstr_append(out, RC_STR(", b."), arena);
            rc_mstr_append(out, letter, arena);
            rc_mstr_append(out, RC_STR(")"), arena);
        }
        else {
            rc_mstr_append(out, RC_STR("a."), arena);
            rc_mstr_append(out, letter, arena);
            rc_mstr_append(out, joiner, arena);
            rc_mstr_append(out, RC_STR("b."), arena);
            rc_mstr_append(out, letter, arena);
        }
    }
    rc_mstr_append(out, RC_STR("); }\n"), arena);
}


// The name of a polyfill.
static rc_str rc_glsl_polyfill_name(rc_glsl_polyfill polyfill)
{
    static const rc_str names[] = {
        [RC_GLSL_POLYFILL_NONE] = RC_STR_INIT(""),
        [RC_GLSL_POLYFILL_FMOD] = RC_STR_INIT("rc_fmod"),
        [RC_GLSL_POLYFILL_DIV] = RC_STR_INIT("rc_div"),
        [RC_GLSL_POLYFILL_MOD] = RC_STR_INIT("rc_mod"),
        [RC_GLSL_POLYFILL_F2I] = RC_STR_INIT("rc_f2i"),
        [RC_GLSL_POLYFILL_F2U] = RC_STR_INIT("rc_f2u"),
        [RC_GLSL_POLYFILL_AND] = RC_STR_INIT("rc_and"),
        [RC_GLSL_POLYFILL_OR] = RC_STR_INIT("rc_or"),
        [RC_GLSL_POLYFILL_SELECT] = RC_STR_INIT("rc_select"),
        [RC_GLSL_POLYFILL_SELECT_VECTOR] = RC_STR_INIT("rc_select"),
        [RC_GLSL_POLYFILL_DOT] = RC_STR_INIT("rc_dot")
    };

    RC_ASSERT(polyfill < RC_COUNT(names));
    return names[polyfill];
}


// Write polyfill for a scalar operand_type.
static void rc_glsl_write_scalar_polyfill(rc_glsl_writer *w, rc_glsl_polyfill polyfill,
    uint32_t operand_type, uint32_t node, rc_arena *arena)
{
    rc_mstr *out = &w->polyfills;
    bool is_unsigned = operand_type == RC_WGSL_TYPE_ID_U32;
    rc_str name = rc_glsl_polyfill_name(polyfill);

    switch (polyfill) {
    case RC_GLSL_POLYFILL_DIV:
    case RC_GLSL_POLYFILL_MOD:
        rc_glsl_polyfill_head(w, operand_type, name, operand_type, operand_type, node, arena);
        rc_mstr_append(out, is_unsigned ? RC_STR(") { return b == 0u ? ")
            : RC_STR(") { return b == 0 || (a == (-2147483647 - 1) && b == -1) ? "), arena);
        if (polyfill == RC_GLSL_POLYFILL_DIV) {
            rc_mstr_append(out, RC_STR("a : a / b; }\n"), arena);
        }
        else {
            rc_mstr_append(out, is_unsigned ? RC_STR("0u : a % b; }\n")
                                            : RC_STR("0 : a - b * (a / b); }\n"), arena);
        }
        break;
    case RC_GLSL_POLYFILL_F2I:
        rc_glsl_polyfill_head(w, RC_WGSL_TYPE_ID_I32, name, operand_type, 0, node, arena);
        rc_mstr_append(out, RC_STR(") { return int(clamp(a, -2147483648.0, 2147483520.0)); }\n"),
            arena);
        break;
    case RC_GLSL_POLYFILL_F2U:
        rc_glsl_polyfill_head(w, RC_WGSL_TYPE_ID_U32, name, operand_type, 0, node, arena);
        rc_mstr_append(out, RC_STR(") { return uint(clamp(a, 0.0, 4294967040.0)); }\n"), arena);
        break;
    case RC_GLSL_POLYFILL_AND:
    case RC_GLSL_POLYFILL_OR:
        rc_glsl_polyfill_head(w, operand_type, name, operand_type, operand_type, node, arena);
        rc_mstr_append(out, polyfill == RC_GLSL_POLYFILL_AND ? RC_STR(") { return a && b; }\n")
                                                       : RC_STR(") { return a || b; }\n"), arena);
        break;
    default:
        RC_UNREACHABLE();
    }
}


// Write polyfill for a vector operand_type, after any scalar version it builds on.
static void rc_glsl_write_vector_polyfill(rc_glsl_writer *w, rc_glsl_polyfill polyfill,
    uint32_t operand_type, uint32_t node, rc_arena *arena)
{
    rc_wgsl_typeinfo t = rc_glsl_typeinfo(w, operand_type);
    rc_mstr *out = &w->polyfills;
    rc_str name = rc_glsl_polyfill_name(polyfill);
    uint32_t to;

    switch (polyfill) {
    case RC_GLSL_POLYFILL_DIV:
    case RC_GLSL_POLYFILL_MOD:
        rc_glsl_ensure_polyfill(w, polyfill, t.element, node, arena);
        rc_glsl_polyfill_head(w, operand_type, name, operand_type, operand_type, node, arena);
        rc_glsl_polyfill_by_component(w, operand_type, name, RC_STR(""), node, arena);
        break;
    case RC_GLSL_POLYFILL_F2I:
    case RC_GLSL_POLYFILL_F2U:
        to = rc_wgsl_type_with_scalar(&w->types, operand_type, polyfill == RC_GLSL_POLYFILL_F2I
            ? RC_WGSL_TYPE_ID_I32 : RC_WGSL_TYPE_ID_U32, arena);
        rc_glsl_polyfill_head(w, to, name, operand_type, 0, node, arena);
        rc_mstr_append(out, RC_STR(") { return "), arena);
        rc_glsl_write_type(w, out, to, node, arena);
        rc_mstr_append(out, polyfill == RC_GLSL_POLYFILL_F2I
            ? RC_STR("(clamp(a, -2147483648.0, 2147483520.0)); }\n")
            : RC_STR("(clamp(a, 0.0, 4294967040.0)); }\n"), arena);
        break;
    case RC_GLSL_POLYFILL_AND:
    case RC_GLSL_POLYFILL_OR:
        rc_glsl_polyfill_head(w, operand_type, name, operand_type, operand_type, node, arena);
        rc_glsl_polyfill_by_component(w, operand_type, name,
            polyfill == RC_GLSL_POLYFILL_AND ? RC_STR(" && ") : RC_STR(" || "), node, arena);
        break;
    default:
        RC_UNREACHABLE();
    }
}


// Write the select polyfills: select(f, t, c) with a bool, or a bool vector, choosing.
static void rc_glsl_write_select_polyfill(rc_glsl_writer *w, rc_glsl_polyfill polyfill,
    uint32_t operand_type, uint32_t node, rc_arena *arena)
{
    rc_mstr *out = &w->polyfills;
    uint32_t width = rc_glsl_typeinfo(w, operand_type).width;
    uint32_t condition = polyfill == RC_GLSL_POLYFILL_SELECT ? RC_WGSL_TYPE_ID_BOOL
        : rc_wgsl_type_vector(&w->types, RC_WGSL_TYPE_ID_BOOL, width, arena);
    static const char letters[] = "xyzw";

    rc_glsl_polyfill_head(w, operand_type, RC_STR("rc_select"), operand_type, operand_type, node,
        arena);
    rc_mstr_append(out, RC_STR(", "), arena);
    rc_glsl_write_type(w, out, condition, node, arena);
    if (polyfill == RC_GLSL_POLYFILL_SELECT) {
        rc_mstr_append(out, RC_STR(" c) { return c ? b : a; }\n"), arena);
        return;
    }
    rc_mstr_append(out, RC_STR(" c) { return "), arena);
    rc_glsl_write_type(w, out, operand_type, node, arena);
    rc_mstr_append(out, RC_STR("("), arena);
    for (uint32_t i = 0; i < width; i++) {
        rc_str letter = rc_str_from_ptr(letters + i, 1);

        rc_mstr_append(out, i > 0 ? RC_STR(", c.") : RC_STR("c."), arena);
        rc_mstr_append(out, letter, arena);
        rc_mstr_append(out, RC_STR(" ? b."), arena);
        rc_mstr_append(out, letter, arena);
        rc_mstr_append(out, RC_STR(" : a."), arena);
        rc_mstr_append(out, letter, arena);
    }
    rc_mstr_append(out, RC_STR("); }\n"), arena);
}


// Write the integer dot product polyfill for a vector operand_type.
static void rc_glsl_write_dot_polyfill(rc_glsl_writer *w, uint32_t operand_type, uint32_t node,
    rc_arena *arena)
{
    rc_mstr *out = &w->polyfills;
    rc_wgsl_typeinfo t = rc_glsl_typeinfo(w, operand_type);
    static const char letters[] = "xyzw";

    rc_glsl_polyfill_head(w, t.element, RC_STR("rc_dot"), operand_type, operand_type, node, arena);
    rc_mstr_append(out, RC_STR(") { return "), arena);
    for (uint32_t i = 0; i < t.width; i++) {
        rc_str letter = rc_str_from_ptr(letters + i, 1);

        rc_mstr_append(out, i > 0 ? RC_STR(" + a.") : RC_STR("a."), arena);
        rc_mstr_append(out, letter, arena);
        rc_mstr_append(out, RC_STR(" * b."), arena);
        rc_mstr_append(out, letter, arena);
    }
    rc_mstr_append(out, RC_STR("; }\n"), arena);
}


// True if polyfill for operand_type is written already, noting it as written if not.
static bool rc_glsl_polyfill_is_written(rc_glsl_writer *w, rc_glsl_polyfill polyfill,
    uint32_t operand_type, rc_arena *arena)
{
    rc_glsl_polyfill_written wanted = {.operand_type = operand_type, .polyfill = (uint8_t)polyfill};

    for (uint32_t i = 0; i < w->polyfills_written.num; i++) {
        rc_glsl_polyfill_written written = rc_array_glsl_polyfill_written_get(
            &w->polyfills_written, i);

        if (written.polyfill == wanted.polyfill && written.operand_type == wanted.operand_type) {
            return true;
        }
    }
    rc_array_glsl_polyfill_written_push(&w->polyfills_written, wanted, arena);
    return false;
}


// Write polyfill for operand_type, if it isn't written already.
static void rc_glsl_ensure_polyfill(rc_glsl_writer *w, rc_glsl_polyfill polyfill,
    uint32_t operand_type, uint32_t node, rc_arena *arena)
{
    bool vector = rc_glsl_typeinfo(w, operand_type).type == RC_WGSL_TYPEINFO_TYPE_VECTOR;

    if (rc_glsl_polyfill_is_written(w, polyfill, operand_type, arena)) {
        return;
    }
    if (polyfill == RC_GLSL_POLYFILL_FMOD) {
        rc_glsl_polyfill_head(w, operand_type, RC_STR("rc_fmod"), operand_type, operand_type,
            node, arena);
        rc_mstr_append(&w->polyfills, RC_STR(") { return a - b * trunc(a / b); }\n"), arena);
    }
    else if (polyfill == RC_GLSL_POLYFILL_SELECT || polyfill == RC_GLSL_POLYFILL_SELECT_VECTOR) {
        rc_glsl_write_select_polyfill(w, polyfill, operand_type, node, arena);
    }
    else if (polyfill == RC_GLSL_POLYFILL_DOT) {
        rc_glsl_write_dot_polyfill(w, operand_type, node, arena);
    }
    else if (vector) {
        rc_glsl_write_vector_polyfill(w, polyfill, operand_type, node, arena);
    }
    else {
        rc_glsl_write_scalar_polyfill(w, polyfill, operand_type, node, arena);
    }
}


void rc_glsl_write_polyfill_call(rc_glsl_writer *w, rc_glsl_polyfill polyfill,
    uint32_t operand_type, uint32_t node, rc_arena *arena)
{
    rc_glsl_ensure_polyfill(w, polyfill, operand_type, node, arena);
    rc_glsl_put(w, rc_glsl_polyfill_name(polyfill), arena);
    rc_glsl_put(w, RC_STR("("), arena);
}

// ---- the ordinary built-ins ----

// The GLSL function for builtin, where it maps directly; "" for those written specially.
static rc_str rc_glsl_builtin_function(rc_wgsl_builtin builtin, rc_glsl_dialect dialect)
{
    bool desktop = dialect == RC_GLSL_DIALECT_460;

    switch (builtin) {
    case RC_WGSL_BUILTIN_ATAN2:
        return RC_STR("atan");
    case RC_WGSL_BUILTIN_DPDX:
        return RC_STR("dFdx");
    case RC_WGSL_BUILTIN_DPDX_COARSE:
        return desktop ? RC_STR("dFdxCoarse") : RC_STR("dFdx");
    case RC_WGSL_BUILTIN_DPDX_FINE:
        return desktop ? RC_STR("dFdxFine") : RC_STR("dFdx");
    case RC_WGSL_BUILTIN_DPDY:
        return RC_STR("dFdy");
    case RC_WGSL_BUILTIN_DPDY_COARSE:
        return desktop ? RC_STR("dFdyCoarse") : RC_STR("dFdy");
    case RC_WGSL_BUILTIN_DPDY_FINE:
        return desktop ? RC_STR("dFdyFine") : RC_STR("dFdy");
    case RC_WGSL_BUILTIN_FWIDTH_COARSE:
        return desktop ? RC_STR("fwidthCoarse") : RC_STR("fwidth");
    case RC_WGSL_BUILTIN_FWIDTH_FINE:
        return desktop ? RC_STR("fwidthFine") : RC_STR("fwidth");
    case RC_WGSL_BUILTIN_FACE_FORWARD:
        return RC_STR("faceforward");
    case RC_WGSL_BUILTIN_INVERSE_SQRT:
        return RC_STR("inversesqrt");
    case RC_WGSL_BUILTIN_PACK2X16FLOAT:
        return RC_STR("packHalf2x16");
    case RC_WGSL_BUILTIN_PACK2X16SNORM:
        return RC_STR("packSnorm2x16");
    case RC_WGSL_BUILTIN_PACK2X16UNORM:
        return RC_STR("packUnorm2x16");
    case RC_WGSL_BUILTIN_UNPACK2X16FLOAT:
        return RC_STR("unpackHalf2x16");
    case RC_WGSL_BUILTIN_UNPACK2X16SNORM:
        return RC_STR("unpackSnorm2x16");
    case RC_WGSL_BUILTIN_UNPACK2X16UNORM:
        return RC_STR("unpackUnorm2x16");
    case RC_WGSL_BUILTIN_ROUND:
        return RC_STR("roundEven");
    case RC_WGSL_BUILTIN_FMA:
        return desktop ? RC_STR("fma") : RC_STR("");
    case RC_WGSL_BUILTIN_ABS:
    case RC_WGSL_BUILTIN_ALL:
    case RC_WGSL_BUILTIN_ANY:
    case RC_WGSL_BUILTIN_DOT:
    case RC_WGSL_BUILTIN_SATURATE:
    case RC_WGSL_BUILTIN_SELECT:
    case RC_WGSL_BUILTIN_ARRAY_LENGTH:
        return RC_STR("");
    default:
        return rc_wgsl_builtin_name(builtin);
    }
}


// Write argument i of a call.
static void rc_glsl_write_arg(rc_glsl_writer *w, rc_view_wgsl_index args, uint32_t i,
    uint32_t scalar, rc_arena *arena)
{
    rc_glsl_write_expr(w, rc_view_wgsl_index_get(args, i), scalar, arena);
}


// Write "function(arguments...)".
static void rc_glsl_write_function_call(rc_glsl_writer *w, rc_str function,
    rc_view_wgsl_index args, uint32_t scalar, rc_arena *arena)
{
    rc_glsl_put(w, function, arena);
    rc_glsl_put(w, RC_STR("("), arena);
    for (uint32_t i = 0; i < args.num; i++) {
        rc_glsl_put(w, i > 0 ? RC_STR(", ") : RC_STR(""), arena);
        rc_glsl_write_arg(w, args, i, scalar, arena);
    }
    rc_glsl_put(w, RC_STR(")"), arena);
}


// Write select(f, t, c): a ternary for each component, through a polyfill, so both choices are
// worked out as WGSL has it.
static void rc_glsl_write_select(rc_glsl_writer *w, uint32_t node, rc_view_wgsl_index args,
    uint32_t scalar, rc_arena *arena)
{
    uint32_t type = rc_glsl_expr_type(w, node, scalar, arena);
    uint32_t condition = rc_glsl_sema(w, rc_view_wgsl_index_get(args, 2)).type;
    bool vector_condition = rc_glsl_typeinfo(w, condition).type == RC_WGSL_TYPEINFO_TYPE_VECTOR;

    if (vector_condition && rc_wgsl_type_scalar_of(w->types.view, type) == RC_WGSL_TYPE_ID_F32) {
        rc_glsl_write_function_call(w, RC_STR("mix"), args, scalar, arena);
        return;
    }
    rc_glsl_write_polyfill_call(w, vector_condition ? RC_GLSL_POLYFILL_SELECT_VECTOR
                                                  : RC_GLSL_POLYFILL_SELECT, type, node, arena);
    rc_glsl_write_arg(w, args, 0, scalar, arena);
    rc_glsl_put(w, RC_STR(", "), arena);
    rc_glsl_write_arg(w, args, 1, scalar, arena);
    rc_glsl_put(w, RC_STR(", "), arena);
    rc_glsl_write_arg(w, args, 2, 0, arena);
    rc_glsl_put(w, RC_STR(")"), arena);
}


// Write a built-in that has no GLSL function of its own meaning.
static void rc_glsl_write_special(rc_glsl_writer *w, uint32_t node, rc_wgsl_builtin builtin,
    rc_view_wgsl_index args, uint32_t scalar, rc_arena *arena)
{
    uint32_t first = rc_glsl_expr_type(w, rc_view_wgsl_index_get(args, 0), scalar, arena);
    bool scalar_arg = rc_wgsl_type_is_scalar(w->types.view, first);
    bool integer = rc_wgsl_type_scalar_of(w->types.view, first) != RC_WGSL_TYPE_ID_F32;

    switch (builtin) {
    case RC_WGSL_BUILTIN_ABS:
        // An unsigned integer is its own magnitude, and GLSL has no abs for it
        if (rc_wgsl_type_scalar_of(w->types.view, first) == RC_WGSL_TYPE_ID_U32) {
            rc_glsl_write_arg(w, args, 0, scalar, arena);
        }
        else {
            rc_glsl_write_function_call(w, RC_STR("abs"), args, scalar, arena);
        }
        break;
    case RC_WGSL_BUILTIN_ALL:
    case RC_WGSL_BUILTIN_ANY:
        if (scalar_arg) {
            rc_glsl_write_arg(w, args, 0, scalar, arena);
        }
        else {
            rc_glsl_write_function_call(w, rc_wgsl_builtin_name(builtin), args, scalar, arena);
        }
        break;
    case RC_WGSL_BUILTIN_DOT:
        if (integer) {
            rc_glsl_write_polyfill_call(w, RC_GLSL_POLYFILL_DOT, first, node, arena);
            rc_glsl_write_arg(w, args, 0, scalar, arena);
            rc_glsl_put(w, RC_STR(", "), arena);
            rc_glsl_write_arg(w, args, 1, scalar, arena);
            rc_glsl_put(w, RC_STR(")"), arena);
        }
        else {
            rc_glsl_write_function_call(w, RC_STR("dot"), args, scalar, arena);
        }
        break;
    case RC_WGSL_BUILTIN_SATURATE:
        rc_glsl_put(w, RC_STR("clamp("), arena);
        rc_glsl_write_arg(w, args, 0, scalar, arena);
        rc_glsl_put(w, RC_STR(", 0.0, 1.0)"), arena);
        break;
    case RC_WGSL_BUILTIN_FMA:
        rc_glsl_put(w, RC_STR("(("), arena);
        rc_glsl_write_arg(w, args, 0, scalar, arena);
        rc_glsl_put(w, RC_STR(" * "), arena);
        rc_glsl_write_arg(w, args, 1, scalar, arena);
        rc_glsl_put(w, RC_STR(") + "), arena);
        rc_glsl_write_arg(w, args, 2, scalar, arena);
        rc_glsl_put(w, RC_STR(")"), arena);
        break;
    case RC_WGSL_BUILTIN_SELECT:
        rc_glsl_write_select(w, node, args, scalar, arena);
        break;
    case RC_WGSL_BUILTIN_ARRAY_LENGTH:
        rc_glsl_put(w, RC_STR("uint("), arena);
        rc_glsl_write_arg(w, args, 0, 0, arena);
        rc_glsl_put(w, RC_STR(".length())"), arena);
        break;
    default:
        RC_UNREACHABLE();
    }
}

// ---- textures ----

// The texture functions' arguments, read in turn.
typedef struct rc_glsl_texture_call {
    rc_view_wgsl_index args;
    rc_wgsl_typeinfo texture;
    uint32_t next;
    uint32_t node;
} rc_glsl_texture_call;

// The next argument.
static uint32_t rc_glsl_texture_next(rc_glsl_texture_call *t)
{
    return rc_view_wgsl_index_get(t->args, t->next++);
}


// True if there is an argument left, which is the offset.
static bool rc_glsl_texture_has_more(const rc_glsl_texture_call *t)
{
    return t->next < t->args.num;
}


// Write the combined sampler for the texture argument and, if with_sampler, the sampler after it.
static void rc_glsl_write_combined(rc_glsl_writer *w, rc_glsl_texture_call *t, bool with_sampler,
    rc_arena *arena)
{
    uint32_t texture = rc_glsl_texture_next(t);
    rc_glsl_pair pair = {.texture = rc_glsl_sema(w, texture).target};

    if (with_sampler) {
        pair.sampler = rc_glsl_sema(w, rc_glsl_texture_next(t)).target;
    }
    rc_glsl_append_sampler_name(&w->code, w->module, pair, arena);
}


// Write the next argument converted to int, as "int(x)", or ivecN for width above 1.
static void rc_glsl_write_int(rc_glsl_writer *w, rc_glsl_texture_call *t, uint32_t width,
    rc_arena *arena)
{
    if (width == 1) {
        rc_glsl_put(w, RC_STR("int("), arena);
    }
    else {
        rc_glsl_put(w, RC_STR("ivec"), arena);
        rc_mstr_append_u32(&w->code, width, arena);
        rc_glsl_put(w, RC_STR("("), arena);
    }
    rc_glsl_write_expr(w, rc_glsl_texture_next(t), 0, arena);
    rc_glsl_put(w, RC_STR(")"), arena);
}


// The number of coordinates a texture's dim takes.
static uint32_t rc_glsl_texture_coords(rc_wgsl_dim dim)
{
    switch (dim) {
    case RC_WGSL_DIM_1D:
        return 1;
    case RC_WGSL_DIM_2D:
    case RC_WGSL_DIM_2D_ARRAY:
        return 2;
    default:
        return 3;
    }
}


// True for the dims with layers.
static bool rc_glsl_texture_is_array(rc_wgsl_dim dim)
{
    return dim == RC_WGSL_DIM_2D_ARRAY || dim == RC_WGSL_DIM_CUBE_ARRAY;
}


// Write sampling coordinates: the coordinates, with the layer and then depth_ref (if not 0)
// joined on as GLSL wants them.
static void rc_glsl_write_place(rc_glsl_writer *w, rc_glsl_texture_call *t, bool with_reference,
    rc_arena *arena)
{
    rc_wgsl_dim dim = (rc_wgsl_dim)t->texture.dim;
    uint32_t extra = (rc_glsl_texture_is_array(dim) ? 1u : 0u) + (with_reference ? 1u : 0u);
    uint32_t width = rc_glsl_texture_coords(dim) + extra;

    // A cube array's depth reference has no room in a vec4, so it goes on its own after
    if (dim == RC_WGSL_DIM_CUBE_ARRAY && with_reference) {
        width = 4;
    }
    if (extra == 0) {
        rc_glsl_write_expr(w, rc_glsl_texture_next(t), 0, arena);
        return;
    }
    rc_glsl_put(w, RC_STR("vec"), arena);
    rc_mstr_append_u32(&w->code, width, arena);
    rc_glsl_put(w, RC_STR("("), arena);
    rc_glsl_write_expr(w, rc_glsl_texture_next(t), 0, arena);
    if (rc_glsl_texture_is_array(dim)) {
        rc_glsl_put(w, RC_STR(", float("), arena);
        rc_glsl_write_expr(w, rc_glsl_texture_next(t), 0, arena);
        rc_glsl_put(w, RC_STR(")"), arena);
    }
    if (with_reference && dim != RC_WGSL_DIM_CUBE_ARRAY) {
        rc_glsl_put(w, RC_STR(", "), arena);
        rc_glsl_write_expr(w, rc_glsl_texture_next(t), 0, arena);
    }
    rc_glsl_put(w, RC_STR(")"), arena);
}


// Write ", next argument", or ", float(next argument)" if as_float.
static void rc_glsl_write_extra(rc_glsl_writer *w, rc_glsl_texture_call *t, bool as_float,
    rc_arena *arena)
{
    rc_glsl_put(w, as_float ? RC_STR(", float(") : RC_STR(", "), arena);
    rc_glsl_write_expr(w, rc_glsl_texture_next(t), 0, arena);
    rc_glsl_put(w, as_float ? RC_STR(")") : RC_STR(""), arena);
}


// Write textureSample, textureSampleBias, textureSampleLevel or textureSampleGrad.
static void rc_glsl_write_sample(rc_glsl_writer *w, rc_wgsl_builtin builtin,
    rc_glsl_texture_call *t, rc_arena *arena)
{
    bool depth = t->texture.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH;
    uint32_t layer = rc_glsl_texture_is_array((rc_wgsl_dim)t->texture.dim) ? 1 : 0;
    uint32_t extras = builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE ? 0
                    : builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_GRAD ? 2 : 1;

    // The texture, sampler and coordinates, the layer and the extras come before any offset
    bool offset = t->args.num > 3 + layer + extras;

    switch (builtin) {
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL:
        rc_glsl_put(w, offset ? RC_STR("textureLodOffset(") : RC_STR("textureLod("), arena);
        break;
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_GRAD:
        rc_glsl_put(w, offset ? RC_STR("textureGradOffset(") : RC_STR("textureGrad("), arena);
        break;
    default:
        rc_glsl_put(w, offset ? RC_STR("textureOffset(") : RC_STR("texture("), arena);
        break;
    }
    rc_glsl_write_combined(w, t, true, arena);
    rc_glsl_put(w, RC_STR(", "), arena);
    rc_glsl_write_place(w, t, false, arena);
    if (builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL) {
        rc_glsl_write_extra(w, t, depth, arena);
    }
    else if (builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_GRAD) {
        rc_glsl_write_extra(w, t, false, arena);
        rc_glsl_write_extra(w, t, false, arena);
    }

    // A bias goes after the offset in GLSL, but before it in WGSL
    if (builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_BIAS) {
        uint32_t bias = rc_glsl_texture_next(t);

        if (rc_glsl_texture_has_more(t)) {
            rc_glsl_write_extra(w, t, false, arena);
        }
        rc_glsl_put(w, RC_STR(", "), arena);
        rc_glsl_write_expr(w, bias, 0, arena);
    }
    else if (rc_glsl_texture_has_more(t)) {
        rc_glsl_write_extra(w, t, false, arena);
    }
    rc_glsl_put(w, depth ? RC_STR(").x") : RC_STR(")"), arena);
}


// Write textureSampleCompare or textureSampleCompareLevel, on a shadow sampler.
static void rc_glsl_write_compare(rc_glsl_writer *w, rc_wgsl_builtin builtin,
    rc_glsl_texture_call *t, rc_arena *arena)
{
    rc_wgsl_dim dim = (rc_wgsl_dim)t->texture.dim;
    bool level = builtin == RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE_LEVEL;
    bool offset = t->args.num > (rc_glsl_texture_is_array(dim) ? 5u : 4u);

    // Only 2D has a textureLod for shadows; a zero gradient does the same for the rest
    bool grad = level && dim != RC_WGSL_DIM_2D;

    if (level && dim == RC_WGSL_DIM_CUBE_ARRAY) {
        rc_glsl_fail(w, t->node,
            RC_STR("textureSampleCompareLevel on a cube array isn't supported"));
        return;
    }
    rc_glsl_put(w, grad ? (offset ? RC_STR("textureGradOffset(") : RC_STR("textureGrad("))
                 : level ? (offset ? RC_STR("textureLodOffset(") : RC_STR("textureLod("))
                 : (offset ? RC_STR("textureOffset(") : RC_STR("texture(")), arena);
    rc_glsl_write_combined(w, t, true, arena);
    rc_glsl_put(w, RC_STR(", "), arena);
    rc_glsl_write_place(w, t, true, arena);
    if (dim == RC_WGSL_DIM_CUBE_ARRAY) {
        rc_glsl_write_extra(w, t, false, arena);
    }
    if (grad) {
        rc_glsl_put(w, dim == RC_WGSL_DIM_CUBE ? RC_STR(", vec3(0.0), vec3(0.0)")
                                               : RC_STR(", vec2(0.0), vec2(0.0)"), arena);
    }
    else if (level) {
        rc_glsl_put(w, RC_STR(", 0.0"), arena);
    }
    if (rc_glsl_texture_has_more(t)) {
        rc_glsl_write_extra(w, t, false, arena);
    }
    rc_glsl_put(w, RC_STR(")"), arena);
}


// Write textureLoad, as texelFetch.
static void rc_glsl_write_load(rc_glsl_writer *w, rc_glsl_texture_call *t, rc_arena *arena)
{
    rc_wgsl_dim dim = (rc_wgsl_dim)t->texture.dim;
    bool depth = t->texture.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH
        || t->texture.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH_MULTISAMPLED;
    uint32_t coords = rc_glsl_texture_coords(dim);

    rc_glsl_put(w, RC_STR("texelFetch("), arena);
    rc_glsl_write_combined(w, t, false, arena);
    rc_glsl_put(w, RC_STR(", "), arena);
    if (rc_glsl_texture_is_array(dim)) {
        rc_glsl_put(w, RC_STR("ivec3("), arena);
        rc_glsl_write_expr(w, rc_glsl_texture_next(t), 0, arena);
        rc_glsl_put(w, RC_STR(", int("), arena);
        rc_glsl_write_expr(w, rc_glsl_texture_next(t), 0, arena);
        rc_glsl_put(w, RC_STR("))"), arena);
    }
    else {
        rc_glsl_write_int(w, t, coords, arena);
    }
    rc_glsl_put(w, RC_STR(", "), arena);
    rc_glsl_write_int(w, t, 1, arena);
    rc_glsl_put(w, depth ? RC_STR(").x") : RC_STR(")"), arena);
}


// Write textureDimensions, as textureSize.
static void rc_glsl_write_dimensions(rc_glsl_writer *w, rc_glsl_texture_call *t, rc_arena *arena)
{
    rc_wgsl_dim dim = (rc_wgsl_dim)t->texture.dim;
    bool multisampled = t->texture.texture_class == RC_WGSL_TEXTURE_CLASS_MULTISAMPLED
        || t->texture.texture_class == RC_WGSL_TEXTURE_CLASS_DEPTH_MULTISAMPLED;

    rc_glsl_put(w, dim == RC_WGSL_DIM_1D ? RC_STR("uint(textureSize(")
                 : dim == RC_WGSL_DIM_3D ? RC_STR("uvec3(textureSize(")
                                         : RC_STR("uvec2(textureSize("), arena);
    rc_glsl_write_combined(w, t, false, arena);
    if (rc_glsl_texture_has_more(t)) {
        rc_glsl_put(w, RC_STR(", "), arena);
        rc_glsl_write_int(w, t, 1, arena);
    }
    else if (!multisampled) {
        rc_glsl_put(w, RC_STR(", 0"), arena);
    }
    rc_glsl_put(w, rc_glsl_texture_is_array(dim) ? RC_STR(").xy)") : RC_STR("))"), arena);
}


// Write a call to a texture function.
static void rc_glsl_write_texture(rc_glsl_writer *w, uint32_t node, rc_wgsl_builtin builtin,
    rc_view_wgsl_index args, rc_arena *arena)
{
    rc_glsl_texture_call t = {
        .args = args,
        .texture = rc_glsl_typeinfo(w, rc_glsl_sema(w, rc_view_wgsl_index_get(args, 0)).type),
        .node = node
    };

    switch (builtin) {
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE:
    case RC_WGSL_BUILTIN_TEXTURE_SAMPLE_COMPARE_LEVEL:
        rc_glsl_write_compare(w, builtin, &t, arena);
        break;
    case RC_WGSL_BUILTIN_TEXTURE_LOAD:
        rc_glsl_write_load(w, &t, arena);
        break;
    case RC_WGSL_BUILTIN_TEXTURE_DIMENSIONS:
        rc_glsl_write_dimensions(w, &t, arena);
        break;
    case RC_WGSL_BUILTIN_TEXTURE_NUM_LAYERS:
        rc_glsl_put(w, RC_STR("uint(textureSize("), arena);
        rc_glsl_write_combined(w, &t, false, arena);
        rc_glsl_put(w, RC_STR(", 0).z)"), arena);
        break;
    case RC_WGSL_BUILTIN_TEXTURE_NUM_LEVELS:
    case RC_WGSL_BUILTIN_TEXTURE_NUM_SAMPLES:
        if (w->dialect == RC_GLSL_DIALECT_ES300) {
            rc_glsl_fail_dialect(w, node, rc_wgsl_builtin_name(builtin), arena);
            return;
        }
        rc_glsl_put(w, builtin == RC_WGSL_BUILTIN_TEXTURE_NUM_LEVELS
            ? RC_STR("uint(textureQueryLevels(") : RC_STR("uint(textureSamples("), arena);
        rc_glsl_write_combined(w, &t, false, arena);
        rc_glsl_put(w, RC_STR("))"), arena);
        break;
    default:
        rc_glsl_write_sample(w, builtin, &t, arena);
        break;
    }
}


// True for the texture built-ins.
static bool rc_glsl_is_texture_builtin(rc_wgsl_builtin builtin)
{
    return builtin >= RC_WGSL_BUILTIN_TEXTURE_DIMENSIONS
        && builtin <= RC_WGSL_BUILTIN_TEXTURE_SAMPLE_LEVEL;
}


void rc_glsl_write_builtin(rc_glsl_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    rc_wgsl_builtin builtin = (rc_wgsl_builtin)rc_glsl_sema(w, node).builtin;
    rc_view_wgsl_index args = rc_wgsl_ast_list(w->module.ast,
        rc_wgsl_ast_node(w->module.ast, node).rhs);
    rc_str function = rc_glsl_builtin_function(builtin, w->dialect);

    if (rc_glsl_is_texture_builtin(builtin)) {
        rc_glsl_write_texture(w, node, builtin, args, arena);
    }
    else if (rc_str_is_empty(function)) {
        rc_glsl_write_special(w, node, builtin, args, scalar, arena);
    }
    else {
        rc_glsl_write_function_call(w, function, args, scalar, arena);
    }
}
