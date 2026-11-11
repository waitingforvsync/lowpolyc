// test.c
//
// The runner and check functions behind richc/test.h. Native only, so unlike
// the rest of core we lean on libc here.

#include "richc/test.h"

#include <inttypes.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "richc/arena.h"
#include "richc/macros.h"

// Section boundaries. On Windows the sentinels sit either side of the items in
// name order ($a < $b < $c). On ELF the linker provides __start_ / __stop_,
// declared weak so a binary with no tests still links (the range is then
// empty). On Mach-O, ld64 provides section$start$ / section$end$, bound with
// asm labels because the names contain '$'. The linker symbols are arrays of
// unknown size, so walking from one to the other is not an out-of-bounds read.
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

static bool rc_test_op_is(const char *op, const char *name)
{
    return strcmp(op, name) == 0;
}


RC_NORETURN static void rc_test_fail(void)
{
    longjmp(rc_test_jmp, 1);
}


static void rc_test_unsupported(const char *op, const char *file, int line)
{
    printf("[FAIL]\n%s:%d: unsupported operation: %s\n", file, line, op);
}


void rc_test_check_bool(bool actual, const char *op, bool expected, const char *expr, const char *file, int line)
{
    if (rc_test_op_is(op, "==")) {
        if (actual == expected) {
            return;
        }
    }
    else if (rc_test_op_is(op, "!=")) {
        if (actual != expected) {
            return;
        }
    }
    else {
        rc_test_unsupported(op, file, line);
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


void rc_test_check_float(double actual, const char *op, double expected, const char *expr, const char *file, int line)
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


static bool rc_test_matches(const rc_test *t, const char *filter)
{
    return t && strncmp(t->group_name, filter, strlen(filter)) == 0;
}


// Run one test, returning whether it passed. The setjmp lives here, away from
// the run loop, so none of the loop's locals are live across a longjmp.
static bool rc_test_run_one(const rc_test *t)
{
    // Set once init has succeeded, so a failing test still tears its fixture
    // down; volatile because it must survive the longjmp
    volatile bool owes_deinit = false;

    // Every test gets a page heap of its own
    if (!rc_arena_init()) {
        printf("[FAIL]\ncannot reserve the page heap\n");
        return false;
    }

    if (setjmp(rc_test_jmp)) {

        // Cleared before the call: a failing deinit longjmps back here and
        // must not run a second time
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


int rc_test_run(const char *filter)
{
    int num_fail = 0;
    int num_pass = 0;
    int num_skip = 0;
    int total = 0;
    int max_width = 0;

    // First pass: count the matching tests and find the widest name
    for (const rc_test **it = RC_TEST_BEGIN; it != RC_TEST_END; ++it) {
        if (rc_test_matches(*it, filter)) {
            int width = (int)(strlen((*it)->group_name) + strlen((*it)->test_name));
            if (width > max_width) {
                max_width = width;
            }
            total++;
        }
    }

    // Second pass: run each matching test
    int count = 1;
    for (const rc_test **it = RC_TEST_BEGIN; it != RC_TEST_END; ++it) {
        const rc_test *t = *it;
        if (!rc_test_matches(t, filter)) {
            continue;
        }

        int padding = max_width + 3 - (int)(strlen(t->group_name) + strlen(t->test_name));
        printf("%4d/%d:  %s.%s%-*s", count, total, t->group_name, t->test_name, padding, ":");
        fflush(stdout);
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

    printf("\nRESULTS: %d tests (%d ok, %d failed, %d skipped)\n", total, num_pass, num_fail, num_skip);
    fflush(stdout);
    return num_fail;
}
