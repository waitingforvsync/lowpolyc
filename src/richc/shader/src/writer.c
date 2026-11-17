// writer.c
//
// What every writer shares: its state and errors, the polyfills written, names, and literals.

#include "writer.h"

#define RC_LOWER_BOUND_TYPE rc_str
#define RC_LOWER_BOUND_NAME writer_word
#define RC_LOWER_BOUND_CMP(a, b) (rc_str_compare((a), (b)) < 0)
#include "richc/lower_bound.template.h"

// ---- state ----

rc_writer rc_writer_make(rc_wgsl_module module, rc_wgsl_entry entry, rc_shader_target target, rc_arena *arena)
{
    return (rc_writer) {
        .module = module,
        .entry = entry,
        .types = rc_wgsl_types_from_view(module.types, arena),
        .polyfills = rc_mstr_make(256, arena),
        .code = rc_mstr_make(4096, arena),
        .polyfills_written = rc_array_writer_polyfill_make(4, arena),
        .slots = rc_array_writer_slot_make(4, arena),
        .target = (uint8_t)target
    };
}


rc_writer_result rc_writer_finish(const rc_writer *w, rc_str declarations)
{
    if (rc_writer_failed(w)) {
        return (rc_writer_result) {.error = w->error};
    }

    return (rc_writer_result) {
        .declarations = declarations,
        .polyfills = w->polyfills.view,
        .code = w->code.view,
        .slots = w->slots.view
    };
}


bool rc_writer_failed(const rc_writer *w)
{
    return rc_wgsl_error_is_set(w->error);
}


void rc_writer_fail(rc_writer *w, uint32_t node, rc_str message)
{
    if (!rc_writer_failed(w)) {
        w->error = rc_wgsl_ast_error(w->module.ast, node, message);
    }
}


// The message for what, which target can't write.
static rc_str rc_writer_target_message(rc_str what, uint8_t target, rc_arena *arena)
{
    static const rc_str targets[] = {
        [RC_SHADER_TARGET_NONE] = RC_STR_INIT(""),
        [RC_SHADER_TARGET_GLSL_ES300] = RC_STR_INIT("GLSL ES 3.00"),
        [RC_SHADER_TARGET_GLSL_460] = RC_STR_INIT("GLSL 4.60")
    };
    rc_mstr message = rc_mstr_from_str(what, 64, arena);

    RC_ASSERT(target < RC_COUNT(targets));
    rc_mstr_append(&message, RC_STR(" can't be written in "), arena);
    rc_mstr_append(&message, targets[target], arena);
    return message.view;
}


void rc_writer_fail_target(rc_writer *w, uint32_t node, rc_str what, rc_arena *arena)
{
    if (!rc_writer_failed(w)) {
        rc_writer_fail(w, node, rc_writer_target_message(what, w->target, arena));
    }
}


rc_wgsl_sema rc_writer_sema(const rc_writer *w, uint32_t node)
{
    return rc_view_wgsl_sema_get(w->module.sema, node);
}


rc_wgsl_typeinfo rc_writer_typeinfo(const rc_writer *w, uint32_t id)
{
    return rc_array_wgsl_typeinfo_get(&w->types.infos, id);
}


void rc_writer_put(rc_writer *w, rc_str text, rc_arena *arena)
{
    rc_mstr_append(&w->code, text, arena);
}


void rc_writer_line(rc_writer *w, rc_arena *arena)
{
    rc_mstr_append_char(&w->code, '\n', arena);
    rc_mstr_append_n(&w->code, ' ', 4 * w->indent, arena);
}


uint32_t rc_writer_expr_type(rc_writer *w, uint32_t node, uint32_t scalar, rc_arena *arena)
{
    uint32_t type = rc_writer_sema(w, node).type;

    if (!rc_wgsl_type_is_abstract(w->types.infos.view, type)) {
        return type;
    }

    if (scalar != 0) {
        return rc_wgsl_type_with_scalar(&w->types, type, scalar, arena);
    }

    return rc_wgsl_type_concretise(&w->types, type, arena);
}


bool rc_writer_note_polyfill(rc_writer *w, uint8_t polyfill, uint32_t operand_type, rc_arena *arena)
{
    rc_writer_polyfill wanted = {.operand_type = operand_type, .polyfill = polyfill};

    for (uint32_t i = 0; i < w->polyfills_written.num; i++) {
        rc_writer_polyfill written = rc_array_writer_polyfill_get(&w->polyfills_written, i);

        if (written.polyfill == wanted.polyfill && written.operand_type == wanted.operand_type) {
            return false;
        }
    }

    rc_array_writer_polyfill_push(&w->polyfills_written, wanted, arena);
    return true;
}


void rc_writer_add_slot(rc_writer *w, rc_writer_slot slot, rc_arena *arena)
{
    rc_array_writer_slot_push(&w->slots, slot, arena);
}

// ---- questions about expressions ----

bool rc_writer_has_calls(const rc_writer *w, uint32_t node)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_CALL: {
        rc_view_wgsl_index args = rc_wgsl_ast_list(w->module.ast, n.rhs);

        if (rc_writer_sema(w, node).call == RC_WGSL_CALL_FUNCTION) {
            return true;
        }

        for (uint32_t i = 0; i < args.num; i++) {
            if (rc_writer_has_calls(w, rc_view_wgsl_index_get(args, i))) {
                return true;
            }
        }
        return false;
    }
    case RC_WGSL_NODE_TYPE_INDEX:
    case RC_WGSL_NODE_TYPE_BINARY:
        return rc_writer_has_calls(w, n.lhs) || rc_writer_has_calls(w, n.rhs);
    case RC_WGSL_NODE_TYPE_MEMBER:
    case RC_WGSL_NODE_TYPE_UNARY:
        return rc_writer_has_calls(w, n.lhs);
    default:
        return false;
    }
}


// True if the index expression node has the same value each time: a constant, or the name of a let
// or a parameter.
static bool rc_writer_is_stable_index(const rc_writer *w, uint32_t node)
{
    rc_wgsl_sema index = rc_writer_sema(w, node);
    rc_wgsl_node_type decl = rc_wgsl_ast_type(w->module.ast, index.target);

    if (index.flags & RC_WGSL_SEMA_CONST) {
        return true;
    }

    if (rc_wgsl_ast_type(w->module.ast, node) != RC_WGSL_NODE_TYPE_IDENT) {
        return false;
    }

    return decl == RC_WGSL_NODE_TYPE_LET || decl == RC_WGSL_NODE_TYPE_PARAM;
}


bool rc_writer_is_stable_reference(const rc_writer *w, uint32_t node)
{
    rc_wgsl_node n = rc_wgsl_ast_node(w->module.ast, node);

    switch ((rc_wgsl_node_type)n.type) {
    case RC_WGSL_NODE_TYPE_UNARY:
    case RC_WGSL_NODE_TYPE_MEMBER:
        return rc_writer_is_stable_reference(w, n.lhs);
    case RC_WGSL_NODE_TYPE_INDEX:
        return rc_writer_is_stable_index(w, n.rhs) && rc_writer_is_stable_reference(w, n.lhs);
    default:
        return true;
    }
}

// ---- names ----

// True if name is one of the reserved words.
static bool rc_writer_is_reserved_word(rc_str name, rc_writer_reserved reserved)
{
    uint32_t i = rc_lower_bound_writer_word(reserved.words, name);

    return i < reserved.words.num && rc_str_is_equal(rc_view_writer_word_get(reserved.words, i), name);
}


// True if name can't be used as it is: a reserved word, or starting with the reserved prefix or our
// own rc_, or holding a double underscore.
static bool rc_writer_is_reserved(rc_str name, rc_writer_reserved reserved)
{
    if (rc_writer_is_reserved_word(name, reserved)) {
        return true;
    }

    if (rc_str_starts_with(name, reserved.prefix) || rc_str_starts_with(name, RC_STR("rc_"))) {
        return true;
    }

    return rc_str_contains(name, RC_STR("__"));
}


void rc_writer_append_name(rc_mstr *out, rc_str name, rc_writer_reserved reserved, rc_arena *arena)
{
    if (rc_writer_is_reserved(name, reserved)) {
        rc_mstr_append(out, RC_STR("rc_u_"), arena);
    }

    rc_mstr_append(out, name, arena);
}

// ---- literals ----

// The value of a hex float literal, without its 0x and suffix: digits, a point and a binary
// exponent. Mantissas past 53 bits lose their lowest bits.
static double rc_writer_parse_hex_float(rc_str text)
{
    double mantissa = 0.0;
    int32_t exponent = 0;
    int32_t written = 0;
    bool after_point = false;
    bool negative = false;
    uint32_t i = 0;

    for (; i < text.len && (text.data[i] | 0x20) != 'p'; i++) {
        if (text.data[i] == '.') {
            after_point = true;
            continue;
        }

        mantissa = mantissa * 16.0 + rc_wgsl_hex_digit(text.data[i]);
        exponent -= after_point ? 4 : 0;
    }

    if (i + 1 < text.len) {
        negative = text.data[i + 1] == '-';
        for (i += (text.data[i + 1] == '-' || text.data[i + 1] == '+') ? 2 : 1; i < text.len; i++) {
            written = written * 10 + (text.data[i] - '0');
            if (written > 2000) {
                written = 2000;
            }
        }
    }

    exponent += negative ? -written : written;
    for (; exponent > 0; exponent--) {
        mantissa *= 2.0;
    }
    for (; exponent < 0; exponent++) {
        mantissa *= 0.5;
    }
    return mantissa;
}


// True if the float literal ends in a suffix, f or h.
static bool rc_writer_has_float_suffix(rc_str literal)
{
    char last = literal.data[literal.len - 1];

    return last == 'f' || last == 'h';
}


// The hex float literal written in decimal, to 17 significant digits. Its suffix only counts as
// one after an exponent, where f is no hex digit.
static rc_str rc_writer_hex_float_as_decimal(rc_str literal, rc_arena *arena)
{
    bool exponent = rc_str_contains(literal, RC_STR("p")) || rc_str_contains(literal, RC_STR("P"));
    bool suffix = exponent && rc_writer_has_float_suffix(literal);
    rc_str body = rc_str_drop_last(rc_str_drop_first(literal, 2), suffix ? 1 : 0);
    rc_mstr decimal = {0};

    rc_mstr_append_f64(&decimal, rc_writer_parse_hex_float(body), (rc_float_format) {.precision = 17}, arena);
    return decimal.view;
}


// True if the decimal text reads as a float in GLSL: it has a point or an exponent.
static bool rc_writer_looks_float(rc_str text)
{
    if (rc_str_contains(text, RC_STR("."))) {
        return true;
    }

    return rc_str_contains(text, RC_STR("e")) || rc_str_contains(text, RC_STR("E"));
}


void rc_writer_append_float(rc_mstr *out, rc_str literal, rc_arena *arena)
{
    bool hex = rc_wgsl_is_hex_literal(literal);
    rc_str unsuffixed = rc_writer_has_float_suffix(literal) ? rc_str_drop_last(literal, 1) : literal;
    rc_str text = hex ? rc_writer_hex_float_as_decimal(literal, arena) : unsuffixed;

    rc_mstr_append(out, text, arena);
    if (!rc_writer_looks_float(text)) {
        rc_mstr_append(out, RC_STR(".0"), arena);
    }
}
