// test.c
//
// The runner and check functions behind richc/test.h. Leaving a test part-way through is the one
// thing that differs between targets, so it is in test_guard.h.

#include "richc/test.h"

#include "richc/arena.h"
#include "richc/math/math.h"
#include "richc/mstr.h"
#include "richc/print.h"

#include "test_guard.h"

// Section boundaries. On Windows the sentinels sit either side of the items in name order
// ($a < $b < $c). On ELF and wasm the linker provides __start_ / __stop_, declared weak so a binary
// with no tests still links (the range is then empty). On Mach-O, ld64 provides section$start$ /
// section$end$, bound with asm labels because the names contain '$'. The linker symbols are arrays
// of unknown size, so walking from one to the other is not an out-of-bounds read.
#if defined(_WIN32)
RC_TEST_SECTION_("rc_test$a") static const rc_test *rc_test_section_start;
RC_TEST_SECTION_("rc_test$c") static const rc_test *rc_test_section_stop;
#  define RC_TEST_BEGIN (&rc_test_section_start)
#  define RC_TEST_END   (&rc_test_section_stop)
#elif defined(__APPLE__)
extern const rc_test *rc_test_section_start[] __asm("section$start$__DATA$rc_test");
extern const rc_test *rc_test_section_stop[]  __asm("section$end$__DATA$rc_test");
#  define RC_TEST_BEGIN (rc_test_section_start)
#  define RC_TEST_END   (rc_test_section_stop)
#else
extern const rc_test *__start_rc_test[] __attribute__((weak));
extern const rc_test *__stop_rc_test[] __attribute__((weak));
#  define RC_TEST_BEGIN (__start_rc_test)
#  define RC_TEST_END   (__stop_rc_test)
#endif

static const double rc_test_epsilon = 0.0001;

// True while an rc_test_try is running.
static bool rc_test_trying;

// The tests a filter matches: how many, and the widest of their names.
typedef struct rc_test_census {
    uint32_t total;
    uint32_t max_width;
} rc_test_census;

// True if a check's operator is name.
static bool rc_test_op_is(const char *op, const char *name)
{
    return rc_str_is_equal(rc_str_from_cstr(op), rc_str_from_cstr(name));
}


// Abandon the running test.
RC_NORETURN static void rc_test_fail(void)
{
    rc_test_guard_leave(RC_TEST_LEVEL_TEST);
}


// ---- reporting ----

// An arena for a failure's message, which is made in the failing test's page heap. The test may
// have set an out-of-memory handler of its own, which must not see this.
static rc_arena rc_test_message_arena(void)
{
    rc_arena_set_oom_handler(NULL);
    return rc_arena_make_default();
}


// A failure's message as far as the actual value: "[FAIL]", then where and what was checked.
static rc_mstr rc_test_message_start(const char *expr, const char *file, int line, rc_arena *arena)
{
    rc_mstr text = {0};

    rc_mstr_append(&text, RC_STR("[FAIL]\n"), arena);
    rc_mstr_append(&text, rc_str_from_cstr(file), arena);
    rc_mstr_append_char(&text, ':', arena);
    rc_mstr_append_i32(&text, line, arena);
    rc_mstr_append(&text, RC_STR(": "), arena);
    rc_mstr_append(&text, rc_str_from_cstr(expr), arena);
    rc_mstr_append(&text, RC_STR(": actual "), arena);
    return text;
}


// Print a failure's message, which ends with the actual value, and abandon the test.
RC_NORETURN static void rc_test_message_finish(rc_mstr text, rc_arena *arena)
{
    rc_mstr_append_char(&text, '\n', arena);
    rc_print(text.view);
    rc_arena_release(arena);
    rc_test_fail();
}


// Append "(a, b, ...)" for the num floats at values.
static void rc_test_append_floats(rc_mstr *text, const float *values, uint32_t num, rc_arena *arena)
{
    rc_mstr_append_char(text, '(', arena);
    for (uint32_t i = 0; i < num; i++) {
        if (i > 0) {
            rc_mstr_append(text, RC_STR(", "), arena);
        }
        rc_mstr_append_f32(text, values[i], (rc_float_format) {0}, arena);
    }
    rc_mstr_append_char(text, ')', arena);
}


// Append "(a, b, ...)" for the num integers at values.
static void rc_test_append_ints(rc_mstr *text, const int32_t *values, uint32_t num, rc_arena *arena)
{
    rc_mstr_append_char(text, '(', arena);
    for (uint32_t i = 0; i < num; i++) {
        if (i > 0) {
            rc_mstr_append(text, RC_STR(", "), arena);
        }
        rc_mstr_append_i32(text, values[i], arena);
    }
    rc_mstr_append_char(text, ')', arena);
}


// Report a check whose operator its operand type can't do.
static void rc_test_unsupported(const char *op, const char *file, int line)
{
    rc_arena arena = rc_test_message_arena();
    rc_mstr text = {0};

    rc_mstr_append(&text, RC_STR("[FAIL]\n"), &arena);
    rc_mstr_append(&text, rc_str_from_cstr(file), &arena);
    rc_mstr_append_char(&text, ':', &arena);
    rc_mstr_append_i32(&text, line, &arena);
    rc_mstr_append(&text, RC_STR(": unsupported operation: "), &arena);
    rc_mstr_append(&text, rc_str_from_cstr(op), &arena);
    rc_mstr_append_char(&text, '\n', &arena);
    rc_print(text.view);
    rc_arena_release(&arena);
}


// Fail a check whose actual value is text.
RC_NORETURN static void rc_test_fail_text(rc_str actual, const char *expr, const char *file, int line)
{
    rc_arena arena = rc_test_message_arena();
    rc_mstr text = rc_test_message_start(expr, file, line, &arena);

    rc_mstr_append(&text, actual, &arena);
    rc_test_message_finish(text, &arena);
}


// Fail a check whose actual value is a string, shown in quotes.
RC_NORETURN static void rc_test_fail_quoted(rc_str actual, const char *expr, const char *file, int line)
{
    rc_arena arena = rc_test_message_arena();
    rc_mstr text = rc_test_message_start(expr, file, line, &arena);

    rc_mstr_append_char(&text, '"', &arena);
    rc_mstr_append(&text, actual, &arena);
    rc_mstr_append_char(&text, '"', &arena);
    rc_test_message_finish(text, &arena);
}


// Fail a check whose actual value is a signed integer.
RC_NORETURN static void rc_test_fail_i64(int64_t actual, const char *expr, const char *file, int line)
{
    rc_arena arena = rc_test_message_arena();
    rc_mstr text = rc_test_message_start(expr, file, line, &arena);

    rc_mstr_append_i64(&text, actual, &arena);
    rc_test_message_finish(text, &arena);
}


// Fail a check whose actual value is an unsigned integer.
RC_NORETURN static void rc_test_fail_u64(uint64_t actual, const char *expr, const char *file, int line)
{
    rc_arena arena = rc_test_message_arena();
    rc_mstr text = rc_test_message_start(expr, file, line, &arena);

    rc_mstr_append_u64(&text, actual, &arena);
    rc_test_message_finish(text, &arena);
}


// Fail a check whose actual value is a double, written as "%f" would.
RC_NORETURN static void rc_test_fail_f64(double actual, const char *expr, const char *file, int line)
{
    rc_arena arena = rc_test_message_arena();
    rc_mstr text = rc_test_message_start(expr, file, line, &arena);

    rc_mstr_append_f64(&text, actual, (rc_float_format) {.fixed = true}, &arena);
    rc_test_message_finish(text, &arena);
}


// Fail a check whose actual value is num floats.
RC_NORETURN static void rc_test_fail_floats(const float *actual, uint32_t num, const char *expr, const char *file, int line)
{
    rc_arena arena = rc_test_message_arena();
    rc_mstr text = rc_test_message_start(expr, file, line, &arena);

    rc_test_append_floats(&text, actual, num, &arena);
    rc_test_message_finish(text, &arena);
}


// Fail a check whose actual value is num integers.
RC_NORETURN static void rc_test_fail_ints(const int32_t *actual, uint32_t num, const char *expr, const char *file, int line)
{
    rc_arena arena = rc_test_message_arena();
    rc_mstr text = rc_test_message_start(expr, file, line, &arena);

    rc_test_append_ints(&text, actual, num, &arena);
    rc_test_message_finish(text, &arena);
}


// Fail a check whose actual value is a fraction.
RC_NORETURN static void rc_test_fail_rational(rc_rational actual, const char *expr, const char *file, int line)
{
    rc_arena arena = rc_test_message_arena();
    rc_mstr text = rc_test_message_start(expr, file, line, &arena);

    rc_mstr_append_i64(&text, rc_rational_num(actual), &arena);
    rc_mstr_append_char(&text, '/', &arena);
    rc_mstr_append_i64(&text, rc_rational_denom(actual), &arena);
    rc_test_message_finish(text, &arena);
}


// ---- checks ----

void rc_test_check_bool(bool actual, const char *op, bool expected, const char *expr, const char *file, int line)
{
    bool pass = false;

    if (rc_test_op_is(op, "==")) {
        pass = actual == expected;
    }
    else if (rc_test_op_is(op, "!=")) {
        pass = actual != expected;
    }
    else {
        rc_test_unsupported(op, file, line);
    }

    if (pass) {
        return;
    }

    rc_test_fail_text(actual ? RC_STR("true") : RC_STR("false"), expr, file, line);
}


void rc_test_check_int(int64_t actual, const char *op, int64_t expected, const char *expr, const char *file, int line)
{
    bool pass = false;

    if (rc_test_op_is(op, "==")) {
        pass = actual == expected;
    }
    else if (rc_test_op_is(op, "!=")) {
        pass = actual != expected;
    }
    else if (rc_test_op_is(op, "<")) {
        pass = actual < expected;
    }
    else if (rc_test_op_is(op, ">")) {
        pass = actual > expected;
    }
    else if (rc_test_op_is(op, "<=")) {
        pass = actual <= expected;
    }
    else if (rc_test_op_is(op, ">=")) {
        pass = actual >= expected;
    }
    else {
        rc_test_unsupported(op, file, line);
    }

    if (pass) {
        return;
    }

    rc_test_fail_i64(actual, expr, file, line);
}


void rc_test_check_u64(uint64_t actual, const char *op, uint64_t expected, const char *expr, const char *file, int line)
{
    bool pass = false;

    if (rc_test_op_is(op, "==")) {
        pass = actual == expected;
    }
    else if (rc_test_op_is(op, "!=")) {
        pass = actual != expected;
    }
    else if (rc_test_op_is(op, "<")) {
        pass = actual < expected;
    }
    else if (rc_test_op_is(op, ">")) {
        pass = actual > expected;
    }
    else if (rc_test_op_is(op, "<=")) {
        pass = actual <= expected;
    }
    else if (rc_test_op_is(op, ">=")) {
        pass = actual >= expected;
    }
    else {
        rc_test_unsupported(op, file, line);
    }

    if (pass) {
        return;
    }

    rc_test_fail_u64(actual, expr, file, line);
}


void rc_test_check_f64(double actual, const char *op, double expected, const char *expr, const char *file, int line)
{
    bool pass = false;

    if (rc_test_op_is(op, "==")) {
        pass = actual == expected;
    }
    else if (rc_test_op_is(op, "~=")) {
        pass = rc_abs_f64(actual - expected) < rc_test_epsilon;
    }
    else if (rc_test_op_is(op, "!=")) {
        pass = actual != expected;
    }
    else if (rc_test_op_is(op, "<")) {
        pass = actual < expected;
    }
    else if (rc_test_op_is(op, ">")) {
        pass = actual > expected;
    }
    else if (rc_test_op_is(op, "<=")) {
        pass = actual <= expected;
    }
    else if (rc_test_op_is(op, ">=")) {
        pass = actual >= expected;
    }
    else {
        rc_test_unsupported(op, file, line);
    }

    if (pass) {
        return;
    }

    rc_test_fail_f64(actual, expr, file, line);
}


// Widened explicitly, so a float check does not trip -Wdouble-promotion.
void rc_test_check_f32(float actual, const char *op, float expected, const char *expr, const char *file, int line)
{
    rc_test_check_f64((double)actual, op, (double)expected, expr, file, line);
}


// Two views match when both are invalid, or both are valid with the same bytes.
static bool rc_test_str_matches(rc_str a, rc_str b)
{
    if (!rc_str_is_valid(a) || !rc_str_is_valid(b)) {
        return rc_str_is_valid(a) == rc_str_is_valid(b);
    }

    return rc_str_is_equal(a, b);
}


void rc_test_check_str(rc_str actual, const char *op, rc_str expected, const char *expr, const char *file, int line)
{
    bool pass = false;

    if (rc_test_op_is(op, "==")) {
        pass = rc_test_str_matches(actual, expected);
    }
    else if (rc_test_op_is(op, "!=")) {
        pass = !rc_test_str_matches(actual, expected);
    }
    else {
        rc_test_unsupported(op, file, line);
    }

    if (pass) {
        return;
    }

    if (!rc_str_is_valid(actual)) {
        rc_test_fail_text(RC_STR("invalid"), expr, file, line);
    }

    rc_test_fail_quoted(actual, expr, file, line);
}


// Whether a check of two values that can only be equal or not passes.
static bool rc_test_equality_passes(const char *op, bool equal, const char *file, int line)
{
    if (rc_test_op_is(op, "==")) {
        return equal;
    }

    if (rc_test_op_is(op, "!=")) {
        return !equal;
    }

    rc_test_unsupported(op, file, line);
    return false;
}


// True if each of the num floats at actual is within the epsilon of the one at expected.
static bool rc_test_floats_near(const float *actual, const float *expected, uint32_t num)
{
    for (uint32_t i = 0; i < num; i++) {
        if (!(rc_abs_f64((double)(actual[i] - expected[i])) < rc_test_epsilon)) {
            return false;
        }
    }

    return true;
}


// Whether a check of two float vectors passes, given whether they are equal and whether
// they are near.
static bool rc_test_float_vector_passes(const char *op, bool equal, bool near, const char *file, int line)
{
    if (rc_test_op_is(op, "~=")) {
        return near;
    }

    return rc_test_equality_passes(op, equal, file, line);
}


void rc_test_check_vec2i(rc_vec2i actual, const char *op, rc_vec2i expected, const char *expr, const char *file, int line)
{
    int32_t values[2] = {actual.x, actual.y};

    if (rc_test_equality_passes(op, rc_vec2i_is_equal(actual, expected), file, line)) {
        return;
    }

    rc_test_fail_ints(values, 2, expr, file, line);
}


void rc_test_check_vec3i(rc_vec3i actual, const char *op, rc_vec3i expected, const char *expr, const char *file, int line)
{
    int32_t values[3] = {actual.x, actual.y, actual.z};

    if (rc_test_equality_passes(op, rc_vec3i_is_equal(actual, expected), file, line)) {
        return;
    }

    rc_test_fail_ints(values, 3, expr, file, line);
}


void rc_test_check_vec2f(rc_vec2f actual, const char *op, rc_vec2f expected, const char *expr, const char *file, int line)
{
    bool near = rc_test_floats_near(rc_vec2f_as_floats(&actual), rc_vec2f_as_floats(&expected), 2);

    if (rc_test_float_vector_passes(op, rc_vec2f_is_equal(actual, expected), near, file, line)) {
        return;
    }

    rc_test_fail_floats(rc_vec2f_as_floats(&actual), 2, expr, file, line);
}


void rc_test_check_vec3f(rc_vec3f actual, const char *op, rc_vec3f expected, const char *expr, const char *file, int line)
{
    bool near = rc_test_floats_near(rc_vec3f_as_floats(&actual), rc_vec3f_as_floats(&expected), 3);

    if (rc_test_float_vector_passes(op, rc_vec3f_is_equal(actual, expected), near, file, line)) {
        return;
    }

    rc_test_fail_floats(rc_vec3f_as_floats(&actual), 3, expr, file, line);
}


void rc_test_check_vec4f(rc_vec4f actual, const char *op, rc_vec4f expected, const char *expr, const char *file, int line)
{
    bool near = rc_test_floats_near(rc_vec4f_as_floats(&actual), rc_vec4f_as_floats(&expected), 4);

    if (rc_test_float_vector_passes(op, rc_vec4f_is_equal(actual, expected), near, file, line)) {
        return;
    }

    rc_test_fail_floats(rc_vec4f_as_floats(&actual), 4, expr, file, line);
}


// Fractions are exactly ordered, so they take every comparison. rc_rational_compare never
// overflows, and both it and rc_rational_is_equal assert their operands are valid.
void rc_test_check_rational(rc_rational actual, const char *op, rc_rational expected, const char *expr, const char *file, int line)
{
    bool pass = false;

    if (rc_test_op_is(op, "==")) {
        pass = rc_rational_is_equal(actual, expected);
    }
    else if (rc_test_op_is(op, "!=")) {
        pass = !rc_rational_is_equal(actual, expected);
    }
    else if (rc_test_op_is(op, "<")) {
        pass = rc_rational_compare(actual, expected) < 0;
    }
    else if (rc_test_op_is(op, ">")) {
        pass = rc_rational_compare(actual, expected) > 0;
    }
    else if (rc_test_op_is(op, "<=")) {
        pass = rc_rational_compare(actual, expected) <= 0;
    }
    else if (rc_test_op_is(op, ">=")) {
        pass = rc_rational_compare(actual, expected) >= 0;
    }
    else {
        rc_test_unsupported(op, file, line);
    }

    if (pass) {
        return;
    }

    rc_test_fail_rational(actual, expr, file, line);
}


// ---- leaving code that must not return ----

bool rc_test_try(void (*fn)(void *), void *context)
{
    bool returned = false;

    RC_PANIC(!rc_test_trying);
    rc_test_trying = true;
    returned = rc_test_guard_call(RC_TEST_LEVEL_TRY, fn, context);
    rc_test_trying = false;
    return returned;
}


void rc_test_bail(void)
{
    RC_PANIC(rc_test_trying);
    rc_test_guard_leave(RC_TEST_LEVEL_TRY);
}


// ---- running ----

// True if the test is in a group whose name starts with filter.
static bool rc_test_matches(const rc_test *t, const char *filter)
{
    return t && rc_str_starts_with(rc_str_from_cstr(t->group_name), rc_str_from_cstr(filter));
}


// The fixture's init, if the test has one.
static void rc_test_set_up(void *test)
{
    const rc_test *t = test;

    if (t->init_fn) {
        t->init_fn(t->context);
    }
}


// The test itself.
static void rc_test_body(void *test)
{
    const rc_test *t = test;

    if (t->step_fn) {
        t->step_fn(t->context);
    }
    else {
        t->fn();
    }
}


// The fixture's deinit, if the test has one.
static void rc_test_tear_down(void *test)
{
    const rc_test *t = test;

    if (t->deinit_fn) {
        t->deinit_fn(t->context);
    }
}


// Run a test's steps in the page heap set up for it, then take the heap down, returning whether it
// passed. A failing check leaves only the step it is in, so a fixture that was set up is torn down
// whether or not the test passed, and only once.
static bool rc_test_run_steps(const rc_test *t)
{
    bool set_up = rc_test_guard_call(RC_TEST_LEVEL_TEST, rc_test_set_up, (void *)t);
    bool ran = set_up && rc_test_guard_call(RC_TEST_LEVEL_TEST, rc_test_body, (void *)t);
    bool torn_down = !set_up || rc_test_guard_call(RC_TEST_LEVEL_TEST, rc_test_tear_down, (void *)t);

    // A failing check can leave a try running, and the next test must not see the out-of-memory
    // handler this one set
    rc_test_trying = false;
    rc_arena_set_oom_handler(NULL);
    rc_arena_deinit();
    return ran && torn_down;
}


// Run one test, returning whether it passed.
static bool rc_test_run_one(const rc_test *t)
{
    // Every test gets a page heap of its own
    if (!rc_arena_init()) {
        rc_print(RC_STR("[FAIL]\ncannot reserve the page heap\n"));
        return false;
    }

    return rc_test_run_steps(t);
}


// The length of a test's group and test names together.
static uint32_t rc_test_name_width(const rc_test *t)
{
    return rc_str_from_cstr(t->group_name).len + rc_str_from_cstr(t->test_name).len;
}


// Count the tests filter matches, and find the widest name.
static rc_test_census rc_test_take_census(const char *filter)
{
    uint32_t total = 0;
    uint32_t max_width = 0;

    for (const rc_test **it = RC_TEST_BEGIN; it != RC_TEST_END; ++it) {
        if (rc_test_matches(*it, filter)) {
            uint32_t width = rc_test_name_width(*it);

            max_width = width > max_width ? width : max_width;
            total++;
        }
    }

    return (rc_test_census) {.total = total, .max_width = max_width};
}


// Print count spaces.
static void rc_test_print_spaces(uint32_t count)
{
    static const char spaces[] = "                                ";
    uint32_t left = count;

    while (left > 0) {
        uint32_t num = left < sizeof spaces - 1 ? left : (uint32_t)sizeof spaces - 1;

        rc_print(rc_str_from_ptr(spaces, num));
        left -= num;
    }
}


// Print a number in decimal, padded on the left with spaces to at least width characters. The run
// loop prints outside any test's page heap, so there is no arena for an rc_mstr here.
static void rc_test_print_u32(uint32_t value, uint32_t width)
{
    char digits[10] = {0};
    uint32_t start = sizeof digits;
    uint32_t rest = value;

    while (start == sizeof digits || rest > 0) {
        start--;
        digits[start] = (char)('0' + rest % 10);
        rest /= 10;
    }

    rc_test_print_spaces(width > sizeof digits - start ? width - ((uint32_t)sizeof digits - start) : 0);
    rc_print(rc_str_from_ptr(digits + start, (uint32_t)sizeof digits - start));
}


// Print the test's number and name, padded so the results line up.
static void rc_test_print_name(const rc_test *t, uint32_t count, rc_test_census census)
{
    rc_test_print_u32(count, 4);
    rc_print(RC_STR("/"));
    rc_test_print_u32(census.total, 0);
    rc_print(RC_STR(":  "));
    rc_print(rc_str_from_cstr(t->group_name));
    rc_print(RC_STR("."));
    rc_print(rc_str_from_cstr(t->test_name));
    rc_print(RC_STR(":"));
    rc_test_print_spaces(census.max_width + 2 - rc_test_name_width(t));
}


// Print the totals, as "RESULTS: 10 tests (8 ok, 1 failed, 1 skipped)".
static void rc_test_print_results(uint32_t total, uint32_t num_pass, uint32_t num_fail, uint32_t num_skip)
{
    rc_print(RC_STR("\nRESULTS: "));
    rc_test_print_u32(total, 0);
    rc_print(RC_STR(" tests ("));
    rc_test_print_u32(num_pass, 0);
    rc_print(RC_STR(" ok, "));
    rc_test_print_u32(num_fail, 0);
    rc_print(RC_STR(" failed, "));
    rc_test_print_u32(num_skip, 0);
    rc_print(RC_STR(" skipped)\n"));
}


int rc_test_run(const char *filter)
{
    rc_test_census census = rc_test_take_census(filter);
    uint32_t count = 1;
    uint32_t num_fail = 0;
    uint32_t num_pass = 0;
    uint32_t num_skip = 0;

    for (const rc_test **it = RC_TEST_BEGIN; it != RC_TEST_END; ++it) {
        const rc_test *t = *it;

        if (!rc_test_matches(t, filter)) {
            continue;
        }

        rc_test_print_name(t, count, census);
        count++;
        if (t->skip) {
            rc_print(RC_STR("[SKIP]\n"));
            num_skip++;
            continue;
        }

        if (rc_test_run_one(t)) {
            rc_print(RC_STR("ok\n"));
            num_pass++;
        }
        else {
            num_fail++;
        }
    }

    rc_test_print_results(census.total, num_pass, num_fail, num_skip);
    return (int)num_fail;
}
