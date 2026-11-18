// test.c
//
// The runner and check functions behind richc/test.h. Native only, so unlike the rest of core we
// lean on libc here.

#include "richc/test.h"

#include <inttypes.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "richc/arena.h"

// Section boundaries. On Windows the sentinels sit either side of the items in name order
// ($a < $b < $c). On ELF the linker provides __start_ / __stop_, declared weak so a binary with no
// tests still links (the range is then empty). On Mach-O, ld64 provides section$start$ /
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

static jmp_buf rc_test_jmp;
static const double rc_test_epsilon = 0.0001;

// The tests a filter matches: how many, and the widest of their names.
typedef struct rc_test_census {
    int total;
    int max_width;
} rc_test_census;

// True if a check's operator is name.
static bool rc_test_op_is(const char *op, const char *name)
{
    return strcmp(op, name) == 0;
}


// Abandon the running test.
RC_NORETURN static void rc_test_fail(void)
{
    longjmp(rc_test_jmp, 1);
}


// Report a check whose operator its operand type can't do.
static void rc_test_unsupported(const char *op, const char *file, int line)
{
    printf("[FAIL]\n%s:%d: unsupported operation: %s\n", file, line, op);
}


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

    printf("[FAIL]\n%s:%d: %s: actual %s\n", file, line, expr, actual ? "true" : "false");
    rc_test_fail();
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

    printf("[FAIL]\n%s:%d: %s: actual %" PRId64 "\n", file, line, expr, actual);
    rc_test_fail();
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

    printf("[FAIL]\n%s:%d: %s: actual %" PRIu64 "\n", file, line, expr, actual);
    rc_test_fail();
}


void rc_test_check_f64(double actual, const char *op, double expected, const char *expr, const char *file, int line)
{
    bool pass = false;

    if (rc_test_op_is(op, "==")) {
        pass = actual == expected;
    }
    else if (rc_test_op_is(op, "~=")) {
        pass = fabs(actual - expected) < rc_test_epsilon;
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

    printf("[FAIL]\n%s:%d: %s: actual %f\n", file, line, expr, actual);
    rc_test_fail();
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

    if (rc_str_is_valid(actual)) {
        printf("[FAIL]\n%s:%d: %s: actual \"%.*s\"\n", file, line, expr, (int)actual.len, actual.data);
    }
    else {
        printf("[FAIL]\n%s:%d: %s: actual invalid\n", file, line, expr);
    }

    rc_test_fail();
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
        if (!(fabs((double)(actual[i] - expected[i])) < rc_test_epsilon)) {
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
    if (rc_test_equality_passes(op, rc_vec2i_is_equal(actual, expected), file, line)) {
        return;
    }

    printf("[FAIL]\n%s:%d: %s: actual (%d, %d)\n", file, line, expr, (int)actual.x, (int)actual.y);
    rc_test_fail();
}


void rc_test_check_vec3i(rc_vec3i actual, const char *op, rc_vec3i expected, const char *expr, const char *file, int line)
{
    if (rc_test_equality_passes(op, rc_vec3i_is_equal(actual, expected), file, line)) {
        return;
    }

    printf("[FAIL]\n%s:%d: %s: actual (%d, %d, %d)\n", file, line, expr, (int)actual.x, (int)actual.y, (int)actual.z);
    rc_test_fail();
}


void rc_test_check_vec2f(rc_vec2f actual, const char *op, rc_vec2f expected, const char *expr, const char *file, int line)
{
    bool near = rc_test_floats_near(rc_vec2f_as_floats(&actual), rc_vec2f_as_floats(&expected), 2);

    if (rc_test_float_vector_passes(op, rc_vec2f_is_equal(actual, expected), near, file, line)) {
        return;
    }

    printf("[FAIL]\n%s:%d: %s: actual (%g, %g)\n", file, line, expr, (double)actual.x, (double)actual.y);
    rc_test_fail();
}


void rc_test_check_vec3f(rc_vec3f actual, const char *op, rc_vec3f expected, const char *expr, const char *file, int line)
{
    bool near = rc_test_floats_near(rc_vec3f_as_floats(&actual), rc_vec3f_as_floats(&expected), 3);

    if (rc_test_float_vector_passes(op, rc_vec3f_is_equal(actual, expected), near, file, line)) {
        return;
    }

    printf("[FAIL]\n%s:%d: %s: actual (%g, %g, %g)\n", file, line, expr, (double)actual.x, (double)actual.y, (double)actual.z);
    rc_test_fail();
}


void rc_test_check_vec4f(rc_vec4f actual, const char *op, rc_vec4f expected, const char *expr, const char *file, int line)
{
    bool near = rc_test_floats_near(rc_vec4f_as_floats(&actual), rc_vec4f_as_floats(&expected), 4);

    if (rc_test_float_vector_passes(op, rc_vec4f_is_equal(actual, expected), near, file, line)) {
        return;
    }

    printf("[FAIL]\n%s:%d: %s: actual (%g, %g, %g, %g)\n", file, line, expr,
        (double)actual.x, (double)actual.y, (double)actual.z, (double)actual.w);
    rc_test_fail();
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

    printf("[FAIL]\n%s:%d: %s: actual %" PRId64 "/%" PRId64 "\n", file, line, expr, rc_rational_num(actual), rc_rational_denom(actual));
    rc_test_fail();
}


// True if the test is in a group whose name starts with filter.
static bool rc_test_matches(const rc_test *t, const char *filter)
{
    return t && strncmp(t->group_name, filter, strlen(filter)) == 0;
}


// Run one test, returning whether it passed. The setjmp lives here, away from the run loop, so none
// of the loop's locals are live across a longjmp.
static bool rc_test_run_one(const rc_test *t)
{
    // Set once init has succeeded, so a failing test still tears its fixture down; volatile because
    // it must survive the longjmp
    volatile bool owes_deinit = false;

    // Every test gets a page heap of its own
    if (!rc_arena_init()) {
        printf("[FAIL]\ncannot reserve the page heap\n");
        return false;
    }

    if (setjmp(rc_test_jmp)) {
        // Cleared before the call: a failing deinit longjmps back here and must not run twice
        if (owes_deinit) {
            owes_deinit = false;
            t->deinit_fn(t->context);
        }

        rc_arena_deinit();
        return false;
    }

    if (t->init_fn) {
        t->init_fn(t->context);
    }

    owes_deinit = t->deinit_fn != NULL;
    if (t->step_fn) {
        t->step_fn(t->context);
    }
    else {
        t->fn();
    }

    owes_deinit = false;
    if (t->deinit_fn) {
        t->deinit_fn(t->context);
    }

    rc_arena_deinit();
    return true;
}


// The length of a test's group and test names together.
static int rc_test_name_width(const rc_test *t)
{
    return (int)(strlen(t->group_name) + strlen(t->test_name));
}


// Count the tests filter matches, and find the widest name.
static rc_test_census rc_test_take_census(const char *filter)
{
    int total = 0;
    int max_width = 0;

    for (const rc_test **it = RC_TEST_BEGIN; it != RC_TEST_END; ++it) {
        if (rc_test_matches(*it, filter)) {
            int width = rc_test_name_width(*it);

            max_width = width > max_width ? width : max_width;
            total++;
        }
    }

    return (rc_test_census) {.total = total, .max_width = max_width};
}


// Print the test's number and name, padded so the results line up.
static void rc_test_print_name(const rc_test *t, int count, rc_test_census census)
{
    int padding = census.max_width + 3 - rc_test_name_width(t);

    printf("%4d/%d:  %s.%s%-*s", count, census.total, t->group_name, t->test_name, padding, ":");
    fflush(stdout);
}


int rc_test_run(const char *filter)
{
    rc_test_census census = rc_test_take_census(filter);
    int count = 1;
    int num_fail = 0;
    int num_pass = 0;
    int num_skip = 0;

    for (const rc_test **it = RC_TEST_BEGIN; it != RC_TEST_END; ++it) {
        const rc_test *t = *it;

        if (!rc_test_matches(t, filter)) {
            continue;
        }

        rc_test_print_name(t, count, census);
        count++;
        if (t->skip) {
            printf("[SKIP]\n");
            num_skip++;
            continue;
        }

        if (rc_test_run_one(t)) {
            printf("ok\n");
            num_pass++;
        }
        else {
            num_fail++;
        }
    }

    printf("\nRESULTS: %d tests (%d ok, %d failed, %d skipped)\n", census.total, num_pass, num_fail, num_skip);
    fflush(stdout);
    return num_fail;
}
